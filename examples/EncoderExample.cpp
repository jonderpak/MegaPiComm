#include <Arduino.h>
#include <MeMegaPi.h>
#include <MegaPiComm.h>

namespace {
// Encoder motor object on the board slot selected by the library's SLOT macro.
MeEncoderOnBoard motor(SLOT1);

// Command characters accepted over USB/Bluetooth.
constexpr int kMotorStartSpeed = 200;
constexpr char kCmdStart = 'M';
constexpr char kCmdStop = 'S';
constexpr char kCmdStatus = 'B';
constexpr char kCmdHelp = '?';
constexpr char kCmdLed = 'L';
constexpr char kCmdTest = 'T';

// Interrupt handler for the motor encoder.
// Reads encoder direction pin and updates pulse count accordingly.
void isr_process_encoder() {
  if (digitalRead(motor.getPortB()) == 0) {
    motor.pulsePosMinus();
  } else {
    motor.pulsePosPlus();
  }
}

// Handle one incoming command character from USB or Bluetooth.
// The command is normalized to uppercase, then dispatched.
void handleBleChar(MegaPiComm &bridge, HardwareSerial &port, const __FlashStringHelper *source, char incoming) {
  const char cmd = (incoming >= 'a' && incoming <= 'z')
      ? static_cast<char>(incoming - ('a' - 'A'))
      : incoming;

  Serial.print(F("CMD from "));
  Serial.print(source);
  Serial.print(F(": '"));
  Serial.print(cmd);
  Serial.println(F("'"));

  if (cmd == kCmdStart) {
    // Start motor at a fixed PWM value.
    motor.setMotorPwm(kMotorStartSpeed);
    motor.updateSpeed();
    bridge.sendLine(port, F("MOTOR START"));
    return;
  }

  if (cmd == kCmdStop) {
    // Stop motor.
    motor.setMotorPwm(0);
    motor.updateSpeed();
    bridge.sendLine(port, F("MOTOR STOP"));
    return;
  }

  if (cmd == kCmdLed) {
    // Toggle onboard status LED.
    digitalWrite(LED_BUILTIN, digitalRead(LED_BUILTIN) == HIGH ? LOW : HIGH);
    bridge.sendLine(port, F("LED TOGGLED"));
    return;
  }

  if (cmd == kCmdTest) {
    bridge.sendLine(port, F("TEST OK"));
    return;
  }

  if (cmd == kCmdStatus) {
    bridge.sendLine(port, F("BT baud: 115200"));
    bridge.sendLine(port, F("Motor mode: Encoder SLOT2"));
    return;
  }

  if (cmd == kCmdHelp) {
    bridge.sendLine(port, F("Commands: M=start, S=stop, L=led, T=test, B=status, ?=help"));
    return;
  }

  if (cmd >= 32 && cmd < 127) {
    // Printable but unsupported command.
    port.print(F("Unknown command: "));
    port.println(cmd);
  }
}

// Build serial settings used by the communication bridge.
MegaPiCommConfig makeConfig() {
  MegaPiCommConfig c;
  c.usbBaud = 115200;
  c.bluetoothBaud = 115200;  // Held fixed at 115200 by the library.
  c.verboseRxLogging = false;
  c.ignoreWhitespace = true;
  return c;
}

MegaPiComm comm(makeConfig());
}  // namespace

void setup() {
  // Initialize status LED.
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // Attach encoder interrupt so pulse count updates continuously.
  attachInterrupt(motor.getIntNum(), isr_process_encoder, RISING);

  // Set PWM to 8KHz for encoder motor drive.
  TCCR1A = _BV(WGM10);
  TCCR1B = _BV(CS11) | _BV(WGM12);
  TCCR2A = _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(CS21);

  // Register command callback and start serial links.
  comm.setReceiveHandler(handleBleChar);
  comm.begin();

  // Start in a known safe state with the motor stopped.
  motor.setMotorPwm(0);
  motor.updateSpeed();
}

void loop() {
  // Non-blocking poll processes incoming USB/Bluetooth data.
  comm.poll();
}
