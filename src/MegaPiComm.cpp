#include "MegaPiComm.h"

MegaPiComm::MegaPiComm(const MegaPiCommConfig &config)
    : config_(config),
      rxHandler_(nullptr),
      bluetoothPortDetected_(false),
      activeBlePort_(nullptr)
{
}

void MegaPiComm::begin()
{
  Serial.begin(config_.usbBaud);
  Serial1.begin(kFixedBluetoothBaud);
  Serial2.begin(kFixedBluetoothBaud);
  Serial3.begin(kFixedBluetoothBaud);

  sendLine(Serial, F("MegaPi BLE transport ready."));
  if (config_.bluetoothBaud != kFixedBluetoothBaud) {
    sendLine(Serial, F("Requested BLE baud ignored; using fixed 115200."));
  }
}

void MegaPiComm::poll()
{
  pollSerialPort(Serial1, F("Serial1"));
  pollSerialPort(Serial2, F("Serial2"));
  pollSerialPort(Serial3, F("Serial3"));
}

void MegaPiComm::setReceiveHandler(MegaPiRxHandler handler)
{
  rxHandler_ = handler;
}

bool MegaPiComm::hasActiveBlePort() const
{
  return activeBlePort_ != nullptr;
}

HardwareSerial *MegaPiComm::activeBlePort()
{
  return activeBlePort_;
}

void MegaPiComm::sendChar(HardwareSerial &port, char value)
{
  port.write(value);
}

void MegaPiComm::sendText(HardwareSerial &port, const char *message)
{
  port.print(message);
}

void MegaPiComm::sendLine(HardwareSerial &port, const char *message)
{
  port.println(message);
}

void MegaPiComm::sendLine(HardwareSerial &port, const __FlashStringHelper *message)
{
  port.println(message);
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

const __FlashStringHelper *MegaPiComm::sourceLabel(HardwareSerial &port) const
{
  return portLabel(port);
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

void MegaPiComm::pollSerialPort(HardwareSerial &port, const __FlashStringHelper *portName)
{
  while (port.available() > 0) {
    const uint8_t raw = static_cast<uint8_t>(port.read());
    const char incoming = static_cast<char>(raw);

    if (config_.verboseRxLogging) {
      logRawByte(portName, raw);
    }

    if (&port != &Serial) {
      if (!bluetoothPortDetected_) {
        bluetoothPortDetected_ = true;
        activeBlePort_ = &port;
        Serial.print(F("Detected active Bluetooth UART on "));
        Serial.println(portLabel(port));
      } else if (activeBlePort_ == nullptr) {
        activeBlePort_ = &port;
      }
    }

    if (config_.ignoreWhitespace && (incoming == '\r' || incoming == '\n' || incoming == ' ' || incoming == '\t')) {
      continue;
    }

    if (rxHandler_ != nullptr) {
      rxHandler_(*this, port, portName, incoming);
    }
  }
}
