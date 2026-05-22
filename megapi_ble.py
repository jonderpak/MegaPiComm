#!/usr/bin/env python3
"""Reusable BLE transport for MegaPi-style UART bridges."""

from __future__ import annotations

from dataclasses import dataclass
from typing import Callable, Optional

from bleak import BleakClient, BleakScanner

DEFAULT_WRITE_CHAR_CANDIDATES = (
    "0000ffe1-0000-1000-8000-00805f9b34fb",
    "6e400002-b5a3-f393-e0a9-e50e24dcca9e",
)
DEFAULT_NOTIFY_CHAR_CANDIDATES = (
    "0000ffe1-0000-1000-8000-00805f9b34fb",
    "6e400003-b5a3-f393-e0a9-e50e24dcca9e",
)


@dataclass(frozen=True)
class BleConnectionConfig:
    preferred_names: tuple[str, ...] = ("Makeblock_LE",)
    name_prefixes: tuple[str, ...] = ("Makeblock_LE",)
    scan_timeout: float = 8.0
    command_suffix: str = ""
    suppressed_response_substrings: tuple[str, ...] = ("eeprom",)
    write_char_candidates: tuple[str, ...] = DEFAULT_WRITE_CHAR_CANDIDATES
    notify_char_candidates: tuple[str, ...] = DEFAULT_NOTIFY_CHAR_CANDIDATES


class MegaPiBleClient:
    def __init__(
        self,
        config: BleConnectionConfig,
        response_handler: Optional[Callable[[str], None]] = None,
    ):
        self.config = config
        self.response_handler = response_handler or self._default_response_handler
        self.client = None
        self.device_name = None
        self.device_address = None
        self.write_char = None
        self.notify_char = None

    @staticmethod
    def _default_response_handler(message: str):
        print(f"Response: {message}")

    @staticmethod
    def _normalize_uuid(uuid_value):
        return str(uuid_value).lower()

    def _matches_device(self, device_name: Optional[str]) -> bool:
        if not device_name:
            return False
        if device_name in self.config.preferred_names:
            return True
        return any(device_name.startswith(prefix) for prefix in self.config.name_prefixes)

    def _pick_device(self, devices):
        matches = [device for device in devices if self._matches_device(device.name)]
        if not matches:
            return None

        for preferred_name in self.config.preferred_names:
            for device in matches:
                if device.name == preferred_name:
                    return device

        return matches[0]

    @staticmethod
    def _choose_characteristic(services, candidate_uuids, required_property):
        candidate_uuids = {uuid.lower() for uuid in candidate_uuids}

        for service in services:
            for characteristic in service.characteristics:
                if (
                    MegaPiBleClient._normalize_uuid(characteristic.uuid) in candidate_uuids
                    and required_property in characteristic.properties
                ):
                    return characteristic

        for service in services:
            for characteristic in service.characteristics:
                if required_property in characteristic.properties:
                    return characteristic

        return None

    @staticmethod
    def _describe_services(services):
        print("Discovered GATT services:")
        for service in services:
            print(f"  {service.uuid} ({service.description})")
            for characteristic in service.characteristics:
                properties = ", ".join(characteristic.properties)
                print(f"    {characteristic.uuid} [{properties}]")

    async def _resolve_services(self):
        if hasattr(self.client, "get_services"):
            return await self.client.get_services()

        services = getattr(self.client, "services", None)
        if services is None:
            raise RuntimeError("No services were discovered by the BLE client")
        return services

    async def connect(self) -> bool:
        print("Scanning for BLE devices...")
        devices = await BleakScanner.discover(timeout=self.config.scan_timeout)

        if not devices:
            print("No BLE devices were found.")
            return False

        device = self._pick_device(devices)
        if device is None:
            print("No matching BLE device was found.")
            print("Nearby BLE devices:")
            for nearby in devices:
                print(f"  {nearby.name or '(no name)'} | {nearby.address}")
            return False

        self.device_name = device.name
        self.device_address = device.address
        self.client = BleakClient(self.device_address)

        print(f"Found {self.device_name} at {self.device_address}")

        try:
            await self.client.connect()
            services = await self._resolve_services()

            self.write_char = self._choose_characteristic(
                services,
                self.config.write_char_candidates,
                "write",
            )
            if self.write_char is None:
                self.write_char = self._choose_characteristic(
                    services,
                    self.config.write_char_candidates,
                    "write-without-response",
                )

            self.notify_char = self._choose_characteristic(
                services,
                self.config.notify_char_candidates,
                "notify",
            )

            if self.write_char is None:
                self._describe_services(services)
                print("Could not find a writable BLE characteristic.")
                return False

            if self.notify_char is not None:
                await self.client.start_notify(self.notify_char.uuid, self._handle_notification)

            print("Connected to BLE device.")
            print(f"Write characteristic: {self.write_char.uuid}")
            print(f"Write properties: {', '.join(self.write_char.properties)}")
            if self.notify_char is not None:
                print(f"Notify characteristic: {self.notify_char.uuid}")
            return True
        except Exception as exc:
            print(f"BLE connection failed: {exc}")
            return False

    async def disconnect(self):
        if self.client is None:
            return

        try:
            if self.notify_char is not None:
                await self.client.stop_notify(self.notify_char.uuid)
        except Exception:
            pass

        if self.client.is_connected:
            await self.client.disconnect()
            print("BLE connection closed")

    def _handle_notification(self, _sender, data):
        try:
            text = data.decode("utf-8", errors="replace").strip()
        except Exception:
            text = repr(bytes(data))

        if not text:
            return

        lowered = text.lower()
        if any(token in lowered for token in self.config.suppressed_response_substrings):
            return

        self.response_handler(text)

    async def send_command(self, cmd: str):
        if self.client is None or not self.client.is_connected or self.write_char is None:
            return

        payload = f"{cmd}{self.config.command_suffix}".encode("ascii")
        supports_wwr = "write-without-response" in self.write_char.properties
        supports_write = "write" in self.write_char.properties

        write_attempts = []
        if supports_wwr:
            write_attempts.append(False)
        if supports_write:
            write_attempts.append(True)
        if not write_attempts:
            write_attempts = [False]

        last_error = None
        for response in write_attempts:
            try:
                await self.client.write_gatt_char(self.write_char.uuid, payload, response=response)
                mode = "with-response" if response else "without-response"
                print(f"Sent: {cmd} ({mode})")
                return
            except Exception as exc:
                last_error = exc

        raise RuntimeError(f"BLE write failed for all modes: {last_error}")
