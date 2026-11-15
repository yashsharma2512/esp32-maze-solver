#include "../../vision/NiclaVision/DropperServo.h"

DropperServo dropper;

void setup() {
  pinMode(33, OUTPUT);  // Keep the TB6612 in standby.
  digitalWrite(33, LOW);
  Serial.begin(115200);
  Serial.setTimeout(150);
  Serial.printf("Dropper signal on GPIO%d. Send an angle (0-180) with Newline or off.\n", DROPPER_PIN);
  Serial.println("The servo moves only after receiving an angle.");
}

void loop() {
  if (!Serial.available()) return;
  String command = Serial.readStringUntil('\n');
  command.trim();
  if (command == "off") {
    dropper.off();
    Serial.println("Servo PWM off.");
    return;
  }

  const char *start = command.c_str();
  char *end;
  const long angle = strtol(start, &end, 10);
  if (end == start || *end != '\0' || angle < 0 || angle > 180) {
    Serial.println("Enter an angle from 0 to 180, or off.");
    return;
  }
  if (dropper.setAngle(int(angle))) Serial.printf("Commanded angle: %ld degrees.\n", angle);
  else Serial.println("Could not set the servo position.");
}
