#include "header.h"

bool IR_CALIBRATE = false;

void setup() {
  /* Initialize Serial module */
  Serial.begin(115200);

  Serial1.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
  // SerialUSB.begin(115200);

  pinMode(STBY, OUTPUT);
  digitalWrite(STBY, LOW);

  /* Set motor outputs */
  setMotorIOs();
  SerialDebug.begin();
  /* Initialize I2C module */
  beginWire();

  /* Initialize MPU6050 module */
  beginMpu();

  /* Initialize neopixel LEDs*/
  initRGB();

  /* Set up optional outputs when a GPIO is assigned */
  if (BUZZPIN >= 0) pinMode(BUZZPIN, OUTPUT);
  if (LED13 >= 0) pinMode(LED13, OUTPUT);
  if (LEFTLED >= 0) pinMode(LEFTLED, OUTPUT);
  if (RIGHTLED >= 0) pinMode(RIGHTLED, OUTPUT);

  pinMode(LEFTCLICK, INPUT_PULLUP);
  pinMode(RIGHTCLICK, INPUT_PULLUP);

  // dropper.attach(2);
  // delay(500);
  // dropper.write(85);

  moveMotor(0, 0);

  // reset both encoders to zero degrees
  encLeft.init();
  encRight.init();

  /* Scan I2C devices (uncomment to check connected I2C device addresses) */
  // testI2C();


  /* Initializes all proximity sensors */
  initAPDS(FL);
  initAPDS(DL);
  initAPDS(DR);
  setupColorSensor();

  while (0) {
    updateProximitySensors();
    SerialDebug.print(dl_filtered);
    SerialDebug.print(",");
    SerialDebug.print(fl_filtered);
    SerialDebug.print(",");
    SerialDebug.println(dr_filtered);

    delay(10);
  }

  showColor(strip.Color(0, 0, 100));
  SerialDebug.println("Show a hand to the left APDS or send start with a newline.");
  SerialDebug.println("Send camera to check the Nicla Vision link before starting.");
  const bool serialStart = waitForUserInput1();

  showColor(strip.Color(0, 100, 100));

  SerialDebug.println("START1");

  // /* calibrate offset for IR sensor */
  // bool state = false;
  // for (int i = 0; i < 100; i++) {
  //   int x = analogRead(A4);
  //   int y = analogRead(A5);

  //   long z = (x + y) / 2;
  //   calibrated_ir = alpha_ir * (calibrated_ir) + (1 - alpha_ir) * z;
  //   state = !state;
  //   digitalWrite(LED13, state);
  //   delay(10);
  // }
  // offset_ir = calibrated_ir;

  /* Calculate gyroscope offsets before allowing the motors to start. */
  SerialDebug.println("Keep the robot still for gyro calibration.");
  mpu.calcOffsets(true, true);  // gyro and accel
  mpu.resetAngleZ();

  encLeft.tare();
  encRight.tare();


  // while (IR_CALIBRATE) {
  //   interpretIR(true);
  //   delay(5);
  // }

  SerialDebug.println("MPU calibrated.");

  if (!serialStart) {
    SerialDebug.println("Show a hand to the right APDS or send start with a newline.");
    waitForUserInput2();
  }

  SerialDebug.endStartup();
  startFloorDetection();
  showColor(strip.Color(0, 50, 0));

  SerialDebug.println("START2");

  encoderPos = 0;
  enableISR();

  SerialDebug.println("Start");
  digitalWrite(STBY, HIGH);

  while (0) {
    mpu.update();
    updateProximitySensors();
    moveStraightAligned();
    
    victimDetection();
  }

  digitalWrite(STBY, HIGH);
  /* test the rotation accuracy using diagonal sensors. comment to disable testing. */
  // testRotation();

  /* Run main algorithm
  Note: function is blocking*/
  // solveMaze();
  while (1) {
    // MoveStrightToDistance(500);
    wallFollow(LEFT);
    // delayWithSensorUpdate(500);
  }
  /* Stop the motors*/
  moveMotor(0, 0);

  /* indicate finish state by blinking both leds continously. */
  while (1) {
    state = !state;
    if (LEFTLED >= 0) digitalWrite(LEFTLED, state);
    if (RIGHTLED >= 0) digitalWrite(RIGHTLED, state);
    delay(400);
  }
}

void loop() {
  /* Nothing to do here */
  delay(1);
}
