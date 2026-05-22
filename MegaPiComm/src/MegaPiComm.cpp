#include "MegaPiComm.h"

MegaPiComm::MegaPiComm(const MegaPiCommConfig &config)
    : config_(config),
      commandCount_(0),
      unknownCommandHandler_(nullptr),
      helpText_(F("No help text configured.")),
      bluetoothPortDetected_(false)
{
}

void MegaPiComm::begin()
{
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(config_.usbBaud);
  Serial1.begin(kFixedBluetoothBaud);
  Serial2.begin(kFixedBluetoothBaud);
  Serial3.begin(kFixedBluetoothBaud);

  sendLine(Serial, F("MegaPi USB + Bluetooth communicator ready."));
  if (config_.bluetoothBaud != kFixedBluetoothBaud) {
    sendLine(Serial, F("Requested BLE baud ignored; using fixed 115200."));
  }

  printStatus(Serial);
  printHelp(Serial);
}

void MegaPiComm::poll()
{
  pollSerialPort(Serial1, F("Serial1"));
  pollSerialPort(Serial2, F("Serial2"));
  pollSerialPort(Serial3, F("Serial3"));
  pollSerialPort(Serial, F("Serial(USB)"));
}

bool MegaPiComm::setCommandHandler(char command, MegaPiCommandHandler handler)
{
  const char normalized = normalizeCommand(command);

  for (uint8_t i = 0; i < commandCount_; ++i) {
    if (commandBindings_[i].command == normalized) {
      commandBindings_[i].handler = handler;
      return true;
    }
  }

  if (commandCount_ >= kMaxCommands) {
    return false;
  }

  commandBindings_[commandCount_].command = normalized;
  commandBindings_[commandCount_].handler = handler;
  ++commandCount_;
  return true;
}

void MegaPiComm::clearCommandHandlers()
{
  commandCount_ = 0;
}

void MegaPiComm::setUnknownCommandHandler(MegaPiUnknownCommandHandler handler)
{
  unknownCommandHandler_ = handler;
}

void MegaPiComm::setHelpText(const __FlashStringHelper *helpText)
{
  helpText_ = helpText ? helpText : F("No help text configured.");
}

void MegaPiComm::printHelp(HardwareSerial &port)
{
  port.print(F("Help: "));
  port.println(helpText_);
  port.print(F("Built-ins: "));
  port.print(config_.helpCommand);
  port.print(F("=help, "));
  port.print(config_.statusCommand);
  port.println(F("=status"));
}

void MegaPiComm::printStatus(HardwareSerial &port)
{
  port.print(F("USB baud: "));
  port.println(config_.usbBaud);
  port.print(F("BT baud (fixed): "));
  port.println(kFixedBluetoothBaud);
}

void MegaPiComm::sendLine(HardwareSerial &port, const __FlashStringHelper *message)
{
  port.println(message);
}

void MegaPiComm::sendAck(HardwareSerial &sourcePort, const __FlashStringHelper *sourceName, const __FlashStringHelper *message)
{
  sourcePort.println(message);
  if (&sourcePort != &Serial) {
    Serial.print(F("ACK -> "));
    Serial.print(sourceName);
    Serial.print(F(": "));
    Serial.println(message);
  }
}

char MegaPiComm::normalizeCommand(char cmd) const
{
  if (cmd >= 'a' && cmd <= 'z') {
    return static_cast<char>(cmd - ('a' - 'A'));
  }
  return cmd;
}

const __FlashStringHelper *MegaPiComm::portLabel(HardwareSerial &port) const
{
  if (&port == &Serial1) {
    return F("Serial1");
  }
  if (&port == &Serial2) {
    return F("Serial2");
  }
  if (&port == &Serial3) {
    return F("Serial3");
  }
  return F("Serial(USB)");
}

void MegaPiComm::reportCommandSource(char cmd, const __FlashStringHelper *sourceName)
{
  Serial.print(F("CMD from "));
  Serial.print(sourceName);
  Serial.print(F(": '"));
  Serial.print(cmd);
  Serial.println(F("'"));
}

void MegaPiComm::logRawByte(const __FlashStringHelper *portName, uint8_t incoming)
{
  Serial.print(F("RX "));
  Serial.print(portName);
  Serial.print(F(": 0x"));
  if (incoming < 16) {
    Serial.print('0');
  }
  Serial.print(incoming, HEX);

  if (incoming >= 32 && incoming < 127) {
    Serial.print(F(" ('"));
    Serial.print(static_cast<char>(incoming));
    Serial.print(F("')"));
  }
  Serial.println();
}

void MegaPiComm::handleCommand(char cmd, HardwareSerial &sourcePort, const __FlashStringHelper *sourceName)
{
  if (&sourcePort != &Serial && !bluetoothPortDetected_) {
    bluetoothPortDetected_ = true;
    Serial.print(F("Detected active Bluetooth UART on "));
    Serial.println(portLabel(sourcePort));
  }

  const char normalized = normalizeCommand(cmd);
  reportCommandSource(normalized, sourceName);

  if (normalized == normalizeCommand(config_.statusCommand)) {
    printStatus(sourcePort);
    return;
  }

  if (normalized == normalizeCommand(config_.helpCommand)) {
    printHelp(sourcePort);
    return;
  }

  for (uint8_t i = 0; i < commandCount_; ++i) {
    if (commandBindings_[i].command == normalized && commandBindings_[i].handler != nullptr) {
      commandBindings_[i].handler(*this, sourcePort, sourceName, normalized);
      return;
    }
  }

  if (unknownCommandHandler_ != nullptr && unknownCommandHandler_(*this, sourcePort, sourceName, normalized)) {
    return;
  }

  if (normalized >= 32 && normalized < 127) {
    sourcePort.print(F("Unknown: "));
    sourcePort.println(normalized);
  }
}

void MegaPiComm::pollSerialPort(HardwareSerial &port, const __FlashStringHelper *portName)
{
  while (port.available() > 0) {
    const uint8_t raw = static_cast<uint8_t>(port.read());
    const char incoming = static_cast<char>(raw);

    if (config_.verboseRxLogging) {
      logRawByte(portName, raw);
    }

    if (incoming != '\r' && incoming != '\n' && incoming != ' ' && incoming != '\t') {
      handleCommand(incoming, port, portName);
    }
  }
}
