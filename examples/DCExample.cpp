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
