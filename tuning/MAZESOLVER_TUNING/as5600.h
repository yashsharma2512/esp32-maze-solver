class MagneticEncoder {
private:
  int lowbyte;     //raw angle 7:0
  word highbyte;   //raw angle 7:0 and 11:8
  int rawAngle;    //final raw angle
  float degAngle;  //raw angle in degrees (360/4096 * [value between 0-4095])

  int quadrantNumber, previousquadrantNumber;  //quadrant IDs
  float numberofTurns = 0;                     //number of turns
  float correctedAngle = 0;                    //tared angle - based on the startup value
  float startAngle = 0;                        //starting angle
  float totalAngle = 0;                        //total absolute angular displacement
  float previoustotalAngle = 0;                //for the display printing

  int channel;

  bool reverse = false;

  void ReadRawAngle() {
    if (!TCA9548A(channel)) readError();
    Wire.beginTransmission(0x36);
    Wire.write(0x0C);
    if (Wire.endTransmission(false) != 0 ||
        Wire.requestFrom(uint8_t(0x36), uint8_t(2)) != 2) readError();

    highbyte = Wire.read() & 0x0F;  //11:8 - 4 bits
    lowbyte = Wire.read();   //7:0 - bits

    highbyte = highbyte << 8;       //shifting to left
    rawAngle = highbyte | lowbyte;  //int is 16 bits (as well as the word)

    if (!reverse) {
      degAngle = rawAngle * 0.087890625;
    } else {
      degAngle = 360.0 - (rawAngle * 0.087890625);  // Reverse the angle
      if (degAngle >= 360.0) {
        degAngle -= 360.0;  // Normalize the angle in case of overflow
      }
    }

    correctedAngle = degAngle - startAngle;   //this tares the position
    if (correctedAngle < 0)                   //if the calculated angle is negative, we need to "normalize" it
      correctedAngle = correctedAngle + 360;  //correction for negative numbers (i.e. -15 becomes +345)
  }

  void checkQuadrant() {
    if (correctedAngle >= 0 && correctedAngle <= 90) quadrantNumber = 1;    //Quadrant 1
    if (correctedAngle > 90 && correctedAngle <= 180) quadrantNumber = 2;   //Quadrant 2
    if (correctedAngle > 180 && correctedAngle <= 270) quadrantNumber = 3;  //Quadrant 3
    if (correctedAngle > 270 && correctedAngle < 360) quadrantNumber = 4;   //Quadrant 4

    if (quadrantNumber != previousquadrantNumber) {                             //if we changed quadrant
      if (quadrantNumber == 1 && previousquadrantNumber == 4) numberofTurns++;  // 4 --> 1 transition: CW rotation
      if (quadrantNumber == 4 && previousquadrantNumber == 1) numberofTurns--;  // 1 --> 4 transition: CCW rotation
      previousquadrantNumber = quadrantNumber;                                  //update to the current quadrant
    }

    totalAngle = (numberofTurns * 360) + correctedAngle;
  }

  bool TCA9548A(uint8_t bus) {
    Wire.beginTransmission(0x70);  // TCA9548A address is 0x70
    Wire.write(1 << bus);          // send byte to select bus
    return Wire.endTransmission() == 0;
  }

  void readError() {
    digitalWrite(STBY, LOW);
    Serial.printf("AS5600 CH%d read failed. Check wiring and reset.\n", channel);
    while (true) delay(500);
  }

public:
  // Constructor
  MagneticEncoder(int chann, bool rev) {
    channel = chann;
    reverse = rev;
  }

  void init() {
    ReadRawAngle();
    startAngle = degAngle;  //update startAngle with degAngle - for taring
  }

  void tare() {
    ReadRawAngle();
    startAngle = degAngle;       // Update startAngle to the current angle
    numberofTurns = 0;           // Reset the number of turns
    totalAngle = 0;              // Reset the total angle
    previoustotalAngle = 0;      // Reset the previous total angle for consistency
    previousquadrantNumber = 0;  // Reset the previous quadrant for fresh tracking
  }

  float getAngle() {
    ReadRawAngle();
    checkQuadrant();
    return totalAngle;
  }
};