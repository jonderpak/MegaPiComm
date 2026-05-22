#include <Arduino.h>
#include <MeMegaPi.h>
#include <MegaPiComm.h>

namespace {
constexpr uint8_t kEncoderSlot = SLOT1;
constexpr int kMotorStartSpeed = 200;

constexpr char kCmdStart = 'M';
constexpr char kCmdStop = 'S';
constexpr char kCmdLed = 'L';
constexpr char kCmdTest = 'T';

MeEncoderOnBoard motor(kEncoderSlot);

void isr_process_encoder()
{
  if (digitalRead(motor.getPortB()) == 0) {
    motor.pulsePosMinus();
  } else {
    motor.pulsePosPlus();
  }
}

void startMotor(MegaPiComm &bridge, HardwareSerial &sourcePort, const __FlashStringHelper *sourceName, char command)
{
  (void)command;
  motor.setMotorPwm(kMotorStartSpeed);
  motor.updateSpeed();
  bridge.sendAck(sourcePort, sourceName, F("MOTOR START"));
}

void stopMotor(MegaPiComm &bridge, HardwareSerial &sourcePort, const __FlashStringHelper *sourceName, char command)
{
  (void)command;
  motor.setMotorPwm(0);
  motor.updateSpeed();
  bridge.sendAck(sourcePort, sourceName, F("MOTOR STOP"));
}

void toggleLed(MegaPiComm &bridge, HardwareSerial &sourcePort, const __FlashStringHelper *sourceName, char command)
{
  (void)command;
  digitalWrite(LED_BUILTIN, digitalRead(LED_BUILTIN) == HIGH ? LOW : HIGH);
  bridge.sendAck(sourcePort, sourceName, F("LED TOGGLED"));
}

void testLink(MegaPiComm &bridge, HardwareSerial &sourcePort, const __FlashStringHelper *sourceName, char command)
{
  (void)command;
  bridge.sendAck(sourcePort, sourceName, F("TEST OK"));
}

bool handleUnknown(MegaPiComm &bridge, HardwareSerial &sourcePort, const __FlashStringHelper *sourceName, char command)
{
  (void)bridge;
  (void)sourceName;
  if (command >= 32 && command < 127) {
    sourcePort.print(F("Unknown command: "));
    sourcePort.println(command);
    return true;
  }
  return false;
}

MegaPiCommConfig makeConfig()
{
  MegaPiCommConfig config;
  config.usbBaud = 115200;
  config.bluetoothBaud = 115200;  // Held at 115200 by library to ensure BLE compatibility.
  config.verboseRxLogging = false;
  config.helpCommand = '?';
  config.statusCommand = 'B';
  return config;
}

MegaPiComm communicator(makeConfig());
}  // namespace

void setup()
{
  attachInterrupt(motor.getIntNum(), isr_process_encoder, RISING);

  // Set PWM to 8KHz for encoder motor drive.
  TCCR1A = _BV(WGM10);
  TCCR1B = _BV(CS11) | _BV(WGM12);
  TCCR2A = _BV(WGM21) | _BV(WGM20);
  TCCR2B = _BV(CS21);

  communicator.setHelpText(F("M=start, S=stop, L=LED toggle, T=test, B=status, ?=help"));
  communicator.setCommandHandler(kCmdStart, startMotor);
  communicator.setCommandHandler(kCmdStop, stopMotor);
  communicator.setCommandHandler(kCmdLed, toggleLed);
  communicator.setCommandHandler(kCmdTest, testLink);
  communicator.setUnknownCommandHandler(handleUnknown);

  communicator.begin();
  motor.setMotorPwm(0);
  motor.updateSpeed();
}

void loop()
{
  communicator.poll();
}
