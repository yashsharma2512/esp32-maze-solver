// STEP 2 - Start the board and verify the main I2C bus.
bool busReady = false;

bool startBoard() {
  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  Serial.begin(115200);
  delay(750);
  Serial.println("\nMAZESOLVER | ESP32 Dev Module | left-wall following");
  Serial.println("STEP 2: Checking the I2C bus. Motors are in standby.");

  if (!Wire.begin(SDA_PIN, SCL_PIN, 100000)) {
    Serial.println("ERROR: could not start I2C.");
    return false;
  }
  Wire.setTimeOut(20);
  if (!selectMux(0)) {
    Serial.println("ERROR: TCA9548A at 0x70 did not respond.");
    return false;
  }
  Wire.beginTransmission(APDS_ADDR);
  if (Wire.endTransmission() == 0) {
    Serial.println("ERROR: 0x39 is on the main bus. Put the AS7341 on TCA CH2.");
    return false;
  }
  busReady = true;
  return true;
}
