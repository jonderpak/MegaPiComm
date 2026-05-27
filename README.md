# MegaPiComm

MegaPiComm is a minimal BLE serial transport for MegaPi projects.

It does one job:
- read incoming characters from Bluetooth serial channels
- pass those characters to your sketch callback
- provide simple send helpers to write text/bytes back

It does not implement motor logic, command parsing, or project behavior.
Your `main.cpp` owns all command interpretation and peripheral control.

## Dependency Setup

This project includes two dependency manifests:

- `lib/MegaPiComm/library.json`: PlatformIO library metadata and Arduino dependency declaration
- `requirements.txt` (project root): Python dependencies for BLE tooling

### requirements.txt

Expected contents:

```txt
bleak>=3.0.2
pynput>=1.8.0
```

### Install commands

```powershell
pip install -r requirements.txt
platformio run
```

## Library Behavior

- USB serial (`Serial`) starts at `config.usbBaud`
- Bluetooth serial transport always runs at **115200** (`MegaPiComm::kFixedBluetoothBaud`)
- Polls `Serial1`, `Serial2`, and `Serial3` for BLE bridge traffic
- Calls your receive handler for each incoming character
- Optionally ignores whitespace (`\r`, `\n`, space, tab)

## Quick Start

```cpp
#include <Arduino.h>
#include <MeMegaPi.h>
#include <MegaPiComm.h>

namespace {
MeEncoderOnBoard motor(SLOT2);

void isr_process_encoder() {
  if (digitalRead(motor.getPortB()) == 0) {
    motor.pulsePosMinus();
  } else {
    motor.pulsePosPlus();
  }
}

void onBleChar(MegaPiComm &bridge, HardwareSerial &port, const __FlashStringHelper *source, char incoming) {
  const char cmd = (incoming >= 'a' && incoming <= 'z')
      ? static_cast<char>(incoming - ('a' - 'A'))
      : incoming;

  if (cmd == 'M') {
    motor.setMotorPwm(200);
    motor.updateSpeed();
    bridge.sendLine(port, F("MOTOR START"));
    return;
  }

  if (cmd == 'S') {
    motor.setMotorPwm(0);
    motor.updateSpeed();
    bridge.sendLine(port, F("MOTOR STOP"));
    return;
  }

  if (cmd == 'T') {
    bridge.sendLine(port, F("TEST OK"));
    return;
  }
}

MegaPiCommConfig makeConfig() {
  MegaPiCommConfig c;
  c.usbBaud = 115200;
  c.bluetoothBaud = 115200;  // Informational; library enforces fixed 115200.
  c.verboseRxLogging = false;
  c.ignoreWhitespace = true;
  return c;
}

MegaPiComm comm(makeConfig());
}  // namespace

void setup() {
  attachInterrupt(motor.getIntNum(), isr_process_encoder, RISING);

  TCCR1A = _BV(WGM10);
  TCCR1B = _BV(CS11) | _BV(WGM12);
  TCCR2A = _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(CS21);

  comm.setReceiveHandler(onBleChar);
  comm.begin();

  motor.setMotorPwm(0);
  motor.updateSpeed();
}

void loop() {
  comm.poll();
}
```

## API Summary

### Config

`MegaPiCommConfig`

- `usbBaud`: USB monitor baud
- `bluetoothBaud`: informational only; BLE remains fixed at 115200
- `verboseRxLogging`: prints hex RX logs to USB serial
- `ignoreWhitespace`: skips CR/LF/space/tab before callback

### Core

- `MegaPiComm(const MegaPiCommConfig &config)`
- `void begin()`
- `void poll()`
- `void setReceiveHandler(MegaPiRxHandler handler)`

### Port helpers

- `bool hasActiveBlePort() const`
- `HardwareSerial *activeBlePort()`
- `const __FlashStringHelper *sourceLabel(HardwareSerial &port) const`

### TX helpers

- `void sendChar(HardwareSerial &port, char value)`
- `void sendText(HardwareSerial &port, const char *message)`
- `void sendLine(HardwareSerial &port, const char *message)`
- `void sendLine(HardwareSerial &port, const __FlashStringHelper *message)`

## Notes

- Keep command parsing in your sketch callback.
- Keep hardware/peripheral control in your sketch.
- Call `poll()` continuously in `loop()`.
