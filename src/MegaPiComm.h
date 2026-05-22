#pragma once

#include <Arduino.h>

class MegaPiComm;

using MegaPiCommandHandler = void (*)(MegaPiComm &bridge, HardwareSerial &sourcePort, const __FlashStringHelper *sourceName, char command);
using MegaPiUnknownCommandHandler = bool (*)(MegaPiComm &bridge, HardwareSerial &sourcePort, const __FlashStringHelper *sourceName, char command);

struct MegaPiCommConfig {
  unsigned long usbBaud = 115200;
  unsigned long bluetoothBaud = 115200;  // Informational input; bridge uses fixed BLE baud.
  bool verboseRxLogging = false;
  char helpCommand = '?';
  char statusCommand = 'B';
};

class MegaPiComm {
public:
  static constexpr unsigned long kFixedBluetoothBaud = 115200;
  static constexpr uint8_t kMaxCommands = 16;

  explicit MegaPiComm(const MegaPiCommConfig &config = MegaPiCommConfig());

  void begin();
  void poll();

  bool setCommandHandler(char command, MegaPiCommandHandler handler);
  void clearCommandHandlers();
  void setUnknownCommandHandler(MegaPiUnknownCommandHandler handler);

  void setHelpText(const __FlashStringHelper *helpText);
  void printHelp(HardwareSerial &port);
  void printStatus(HardwareSerial &port);

  void sendLine(HardwareSerial &port, const __FlashStringHelper *message);
  void sendAck(HardwareSerial &sourcePort, const __FlashStringHelper *sourceName, const __FlashStringHelper *message);

private:
  struct CommandBinding {
    char command;
    MegaPiCommandHandler handler;
  };

  char normalizeCommand(char cmd) const;
  const __FlashStringHelper *portLabel(HardwareSerial &port) const;
  void reportCommandSource(char cmd, const __FlashStringHelper *sourceName);
  void logRawByte(const __FlashStringHelper *portName, uint8_t incoming);
  void handleCommand(char cmd, HardwareSerial &sourcePort, const __FlashStringHelper *sourceName);
  void pollSerialPort(HardwareSerial &port, const __FlashStringHelper *portName);

  MegaPiCommConfig config_;
  CommandBinding commandBindings_[kMaxCommands];
  uint8_t commandCount_;
  MegaPiUnknownCommandHandler unknownCommandHandler_;
  const __FlashStringHelper *helpText_;
  bool bluetoothPortDetected_;
};
