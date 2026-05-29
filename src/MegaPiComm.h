#pragma once

#include <Arduino.h>

class MegaPiComm;

using MegaPiRxHandler = void (*)(MegaPiComm &bridge, HardwareSerial &sourcePort, const __FlashStringHelper *sourceName, char incoming);

struct MegaPiCommConfig {
  unsigned long usbBaud = 115200;
  unsigned long bluetoothBaud = 115200;  // Informational input; transport uses fixed BLE baud.
  bool verboseRxLogging = false;
  bool ignoreWhitespace = true;
};

class MegaPiComm {
public:
  static constexpr unsigned long kFixedBluetoothBaud = 115200;

  explicit MegaPiComm(const MegaPiCommConfig &config = MegaPiCommConfig());

  void begin();
  void poll();

  void setReceiveHandler(MegaPiRxHandler handler);

  bool hasActiveBlePort() const;
  HardwareSerial *activeBlePort();

  void sendChar(HardwareSerial &port, char value);
  void sendText(HardwareSerial &port, const char *message);
  void sendLine(HardwareSerial &port, const char *message);
  void sendLine(HardwareSerial &port, const __FlashStringHelper *message);

  const __FlashStringHelper *sourceLabel(HardwareSerial &port) const;

private:
  const __FlashStringHelper *portLabel(HardwareSerial &port) const;
  void logRawByte(const __FlashStringHelper *portName, uint8_t incoming);
  void pollSerialPort(HardwareSerial &port, const __FlashStringHelper *portName);

  MegaPiCommConfig config_;
  MegaPiRxHandler rxHandler_;
  bool bluetoothPortDetected_;
  HardwareSerial *activeBlePort_;
};
