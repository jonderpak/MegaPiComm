#!/usr/bin/env python3
"""PowerShell-friendly launcher for the MegaPi BLE bridge."""

from __future__ import annotations

import argparse
import asyncio
from typing import Iterable

from megapi_ble import BleConnectionConfig, MegaPiBleClient


def _iter_commands(text: str) -> Iterable[str]:
    for char in text:
        if not char.isspace():
            yield char


async def _send_sequence(client: MegaPiBleClient, command_text: str) -> None:
    for command in _iter_commands(command_text):
        await client.send_command(command)


async def main() -> int:
    parser = argparse.ArgumentParser(description="Connect to a MegaPi BLE device and send commands.")
    parser.add_argument(
        "--command",
        help="Send one command sequence and exit. Example: --command M or --command MS",
    )
    parser.add_argument(
        "--scan-timeout",
        type=float,
        default=8.0,
        help="Seconds to scan for a matching BLE device.",
    )
    args = parser.parse_args()

    config = BleConnectionConfig(scan_timeout=args.scan_timeout)
    client = MegaPiBleClient(config)

    if not await client.connect():
        return 1

    try:
        if args.command:
            await _send_sequence(client, args.command)
            return 0

        print("Type a command character and press Enter. Use 'quit' or 'exit' to stop.")
        print("Common commands: M=start, S=stop, L=led, T=test, B=status, ?=help")

        while True:
            try:
                user_input = input("MegaPi> ").strip()
            except EOFError:
                break

            if not user_input:
                continue

            if user_input.lower() in {"quit", "exit"}:
                break

            await _send_sequence(client, user_input)
    finally:
        await client.disconnect()

    return 0


if __name__ == "__main__":
    raise SystemExit(asyncio.run(main()))
