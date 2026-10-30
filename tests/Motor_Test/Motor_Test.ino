// Standalone ESP32-WROOM-32 Dev Module motor test.
// Assumes a TB6612FNG driver and the remapped wiring below.
// Lift the wheels before uploading. Serial Monitor: 115200 baud.
#include <Arduino.h>

#define LMF 25
#define LMR 26
#define PWML 27
#define RMF 18
#define RMR 19
#define PWMR 23
#define STBY 33

const int TEST_SPEED = 120;  // PWM range 0..255

void moveMotor(int inL, int inR);
void setMotorIOs();
void runTest(const char *label, int left, int right);

void setMotorIOs() {
  // Hold the driver in standby while configuring its inputs.
  pinMode(STBY, OUTPUT);
  digitalWrite(STBY, LOW);

  pinMode(LMF, OUTPUT);
  pinMode(LMR, OUTPUT);
  pinMode(PWML, OUTPUT);
  pinMode(RMF, OUTPUT);
  pinMode(RMR, OUTPUT);
  pinMode(PWMR, OUTPUT);
  moveMotor(0, 0);
}

// Same interface as your maze sketch:
// positive = forward, negative = reverse, zero = stop.
void moveMotor(int inL, int inR) {
  inL = constrain(inL, -255, 255);
  inR = constrain(inR, -255, 255);
  inL = (int)(inL * 0.985f);  // Original sketch's left motor trim

  int leftpwm = abs(inL);
  int rightpwm = abs(inR);

  // Remove drive before updating direction inputs.
  analogWrite(PWML, 0);
  analogWrite(PWMR, 0);

  digitalWrite(LMF, inL > 0 ? HIGH : LOW);
  digitalWrite(LMR, inL < 0 ? HIGH : LOW);
  digitalWrite(RMF, inR > 0 ? HIGH : LOW);
  digitalWrite(RMR, inR < 0 ? HIGH : LOW);

  analogWrite(PWML, leftpwm);
  analogWrite(PWMR, rightpwm);
}

void runTest(const char *label, int left, int right) {
  Serial.println(label);
  moveMotor(left, right);
  delay(1500);
  moveMotor(0, 0);
  Serial.println("STOP");
  delay(800);
}

void setup() {
  Serial.begin(115200);
  setMotorIOs();

  Serial.println("\nESP32 MOTOR TEST - wheels must be lifted");
  Serial.println("Starting in 3 seconds...");
  delay(3000);
  digitalWrite(STBY, HIGH);

  runTest("1. LEFT motor FORWARD", TEST_SPEED, 0);
  runTest("2. LEFT motor REVERSE", -TEST_SPEED, 0);
  runTest("3. RIGHT motor FORWARD", 0, TEST_SPEED);
  runTest("4. RIGHT motor REVERSE", 0, -TEST_SPEED);
  runTest("5. BOTH motors FORWARD", TEST_SPEED, TEST_SPEED);
  runTest("6. BOTH motors REVERSE", -TEST_SPEED, -TEST_SPEED);
  runTest("7. Pivot LEFT", -TEST_SPEED, TEST_SPEED);
  runTest("8. Pivot RIGHT", TEST_SPEED, -TEST_SPEED);

  moveMotor(0, 0);
  digitalWrite(STBY, LOW);
  Serial.println("TEST FINISHED. Driver in standby. Press EN to repeat.");
}

void loop() {
  delay(1000);
}
