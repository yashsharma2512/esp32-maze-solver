void beginMpu()
{
  // Wire.begin();
  byte status = mpu.begin(3, 0);
  SerialDebug.print(F("MPU6050 status: "));
  SerialDebug.println(status, DEC);
  // SerialDebug.println((status == 0) ? "OK" : "ERROR");
  while (status != 0)
  {
    alterLEDstate();
    delay(500);
  } // stop everything if could not connect to MPU6050
}