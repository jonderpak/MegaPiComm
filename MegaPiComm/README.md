# MegaPiComm

MegaPiComm is a lightweight command bridge for MegaPi projects.
It reads commands from USB serial and Bluetooth serial channels, then dispatches those commands to handlers you define in your sketch.

The library is intentionally generic:
- Motor behavior stays in your sketch.
- Project-specific commands stay in your sketch.
- Transport and command routing live in the library.

## What It Does

- Initializes USB and hardware serial channels (`Serial`, `Serial1`, `Serial2`, `Serial3`)
- Holds Bluetooth UART at **115200** (`MegaPiComm::kFixedBluetoothBaud`)
- Normalizes letter commands to uppercase
- Routes commands to handlers registered with `setCommandHandler(...)`
- Provides built-in help and status commands
- Supports optional unknown-command handling
- Provides ACK helper for response messages

## Install / Layout

This project uses PlatformIO private libraries.

Place the library here:

- `lib/MegaPiComm/src/MegaPiComm.h`
- `lib/MegaPiComm/src/MegaPiComm.cpp`

Then include it from your sketch:

```cpp
#include <MegaPiComm.h>
```

## Quick Start

```cpp
#include <Arduino.h>
#include <MeMegaPi.h>
#include <MegaPiComm.h>

namespace {
MeEncoderOnBoard motor(SLOT1);

void isr_process_encoder() {
  if (digitalRead(motor.getPortB()) == 0) {
    motor.pulsePosMinus();
  } else {
    motor.pulsePosPlus();
  }
}

void startMotor(MegaPiComm &bridge, HardwareSerial &port, const __FlashStringHelper *source, char cmd) {
  (void)cmd;
  motor.setMotorPwm(200);
  motor.updateSpeed();
  bridge.sendAck(port, source, F("MOTOR START"));
}

void stopMotor(MegaPiComm &bridge, HardwareSerial &port, const __FlashStringHelper *source, char cmd) {
  (void)cmd;
  motor.setMotorPwm(0);
  motor.updateSpeed();
  bridge.sendAck(port, source, F("MOTOR STOP"));
}

MegaPiCommConfig makeConfig() {
  MegaPiCommConfig c;
  c.usbBaud = 115200;
  c.bluetoothBaud = 115200; // Library keeps BLE UART fixed at 115200.
  c.verboseRxLogging = false;
  c.helpCommand = '?';
  c.statusCommand = 'B';
  return c;
}

MegaPiComm comm(makeConfig());
}  // namespace

void setup() {
  attachInterrupt(motor.getIntNum(), isr_process_encoder, RISING);

  comm.setHelpText(F("M=start, S=stop, B=status, ?=help"));
  comm.setCommandHandler('M', startMotor);
  comm.setCommandHandler('S', stopMotor);

  comm.begin();
}

void loop() {
  comm.poll();
}
```

## DC Motor Example

If you are using a regular MegaPi DC motor output instead of encoder motor control:

```cpp
#include <Arduino.h>
#include <MeMegaPi.h>
#include <MegaPiComm.h>

namespace {
MeMegaPiDCMotor motor(PORT1A);

void startMotor(MegaPiComm &bridge, HardwareSerial &port, const __FlashStringHelper *source, char cmd) {
  (void)cmd;
  motor.run(180);
  bridge.sendAck(port, source, F("DC MOTOR START"));
}

void stopMotor(MegaPiComm &bridge, HardwareSerial &port, const __FlashStringHelper *source, char cmd) {
  (void)cmd;
  motor.stop();
  bridge.sendAck(port, source, F("DC MOTOR STOP"));
}

MegaPiComm comm;
}  // namespace

void setup() {
  comm.setHelpText(F("M=start DC motor, S=stop DC motor, B=status, ?=help"));
  comm.setCommandHandler('M', startMotor);
  comm.setCommandHandler('S', stopMotor);
  comm.begin();
}

void loop() {
  comm.poll();
}
```

Tip: Change `PORT1A` to `PORT1B`, `PORT2A`, `PORT2B`, `PORT3A`, `PORT3B`, `PORT4A`, or `PORT4B` based on your wiring.

## Configuration

Use `MegaPiCommConfig`:

- `usbBaud`: USB serial monitor baud rate
- `bluetoothBaud`: informational input; BLE UART still uses fixed 115200
- `verboseRxLogging`: print per-byte RX debug logs when true
- `helpCommand`: built-in help trigger (default `?`)
- `statusCommand`: built-in status trigger (default `B`)

## Core API

### Construction

- `MegaPiComm(const MegaPiCommConfig &config)`

### Runtime

- `begin()`
- `poll()`

### Command Binding

- `bool setCommandHandler(char command, MegaPiCommandHandler handler)`
- `void clearCommandHandlers()`
- `void setUnknownCommandHandler(MegaPiUnknownCommandHandler handler)`

### Built-in Text

- `void setHelpText(const __FlashStringHelper *helpText)`
- `void printHelp(HardwareSerial &port)`
- `void printStatus(HardwareSerial &port)`

### Response Helpers

- `void sendLine(HardwareSerial &port, const __FlashStringHelper *message)`
- `void sendAck(HardwareSerial &sourcePort, const __FlashStringHelper *sourceName, const __FlashStringHelper *message)`

## Handler Signatures

Command handler:

```cpp
void myHandler(MegaPiComm &bridge,
               HardwareSerial &sourcePort,
               const __FlashStringHelper *sourceName,
               char command)
```

Unknown-command handler:

```cpp
bool myUnknownHandler(MegaPiComm &bridge,
                      HardwareSerial &sourcePort,
                      const __FlashStringHelper *sourceName,
                      char command)
```

Return `true` if handled; `false` to let default unknown handling run.

## Notes

- Call `poll()` continuously in `loop()` for responsive command handling.
- Register all handlers before calling `begin()`.
- Keep heavy work out of command handlers where possible.
- Sketch owns all hardware behavior (motor, sensors, etc.).
