// TB6612 - Drive the wheels. STBY LOW turns off both motor outputs.
int leftPWM = 0, rightPWM = 0;

void stopMotors() {
  digitalWrite(MOTOR_STANDBY, LOW);
  analogWrite(LEFT_SPEED, 0);
  analogWrite(RIGHT_SPEED, 0);
  for (int pin : {LEFT_FORWARD, LEFT_REVERSE, RIGHT_FORWARD, RIGHT_REVERSE}) {
    digitalWrite(pin, LOW);
  }
  leftPWM = rightPWM = 0;
}

void setupMotorPins() {
  pinMode(MOTOR_STANDBY, OUTPUT);
  digitalWrite(MOTOR_STANDBY, LOW);
  for (int pin : {LEFT_FORWARD, LEFT_REVERSE, LEFT_SPEED,
                  RIGHT_FORWARD, RIGHT_REVERSE, RIGHT_SPEED}) {
    pinMode(pin, OUTPUT);
  }
  stopMotors();
}

// Positive speed drives forward. Negative speed drives backward.
void moveMotor(int left, int right) {
  leftPWM = int(constrain(left, -255, 255) * 0.985f);
  rightPWM = constrain(right, -255, 255);
  analogWrite(LEFT_SPEED, 0);
  analogWrite(RIGHT_SPEED, 0);
  digitalWrite(LEFT_FORWARD, leftPWM > 0 ? HIGH : LOW);
  digitalWrite(LEFT_REVERSE, leftPWM < 0 ? HIGH : LOW);
  digitalWrite(RIGHT_FORWARD, rightPWM > 0 ? HIGH : LOW);
  digitalWrite(RIGHT_REVERSE, rightPWM < 0 ? HIGH : LOW);
  analogWrite(LEFT_SPEED, abs(leftPWM));
  analogWrite(RIGHT_SPEED, abs(rightPWM));
  digitalWrite(MOTOR_STANDBY, HIGH);
}
