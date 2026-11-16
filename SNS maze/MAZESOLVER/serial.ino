void readSerial() {
  while (Serial1.available()) {
    char inChar = (char)Serial1.read();
    if (inChar == '\n') {
      stringComplete = true;
    } else inputString += inChar;
  }
}


void interpretSerial() {

  if (stringComplete) {

    // Remove whitespace / newline
    inputString.trim();

    // ---------------------------------------------------
    // Find commas
    // ---------------------------------------------------

    int comma1 = inputString.indexOf(',');
    int comma2 = inputString.indexOf(',', comma1 + 1);
    int comma3 = inputString.indexOf(',', comma2 + 1);

    if (comma1 >= 0 && comma2 >= 0 && comma3 >= 0) {

      // -------------------------------------------------
      // Extract values
      // -------------------------------------------------

      victimType = inputString.substring(0, comma1).toInt();

      elapsedTime = inputString.substring(
                                 comma1 + 1,
                                 comma2)
                      .toInt();

      victimX = inputString.substring(
                             comma2 + 1,
                             comma3)
                  .toInt();

      victimY = inputString.substring(
                             comma3 + 1)
                  .toInt();


      // -------------------------------------------------
      // Interpret victim type
      // -------------------------------------------------

      switch (victimType) {

        case 2:
          victimStatus = "HARMED";
          break;

        case 1:
          victimStatus = "UNHARMED";
          break;

        case 0:
          victimStatus = "STABLE";
          break;

        default:
          victimStatus = "FAKE";
          break;
      }


      // -------------------------------------------------
      // Debug output
      // -------------------------------------------------

      // SerialDebug.print("Status: ");
      // SerialDebug.print(victimStatus);

      // SerialDebug.print("\tElapsed: ");
      // SerialDebug.print(elapsedTime);

      // SerialDebug.print("\tX: ");
      // SerialDebug.print(victimX);

      // SerialDebug.print("\tY: ");
      // SerialDebug.println(victimY);

    } else {

      // SerialDebug.print("Invalid data: ");
      // SerialDebug.println(inputString);
    }


    // ---------------------------------------------------
    // Clear the string
    // ---------------------------------------------------

    inputString = "";
    stringComplete = false;
  }
}



void victimDetection() {

  readSerial();
  interpretSerial();
  unsigned long lastelapsed = elapsedTime;
  unsigned long _t = millis();

  if (elapsedTime > 5) {
    if (victimType == 2 or victimType == 1 or victimType == 0) {
      moveMotor(0, 0);
      while (true) {
        mpu.update();
        updateProximitySensors();
        readSerial();
        interpretSerial();
        if (elapsedTime != lastelapsed) {
          lastelapsed = elapsedTime;
          _t = millis();
        }

        if (millis() - _t > 200) {
          SerialDebug.println("break");
          elapsedTime = 0;
          victimType = -1;
          victimX = -1;
          victimY = -1;
          break;
        }

        moveMotor(-45, -45);

        if (victimX > 70) {
          moveMotor(0, 0);
          SerialDebug.print("FOUND: \t");
          // delay(2000);
          if (elapsedTime > 500) {
            SerialDebug.print("found victim: ");
            SerialDebug.println(victimStatus);
            SerialDebug.println("Waiting 5 sec....");
            ;

            uint32_t _clr = strip.Color(10, 10, 10);
            if (victimType == 2)
              _clr = strip.Color(100, 0, 0);
            else if (victimType == 1)
              _clr = strip.Color(100, 50, 0);
            else if (victimType == 0)
              _clr = strip.Color(0, 100, 0);

            for (int i = 0; i < 5; i++) {
              showColor(_clr);
              delayWithSensorUpdate(500);
              strip.clear();
              strip.show();
              delayWithSensorUpdate(500);
            }

            moveMotor(100, 100);
            delayWithSensorUpdate(500);

            elapsedTime = 0;
            victimType = -1;
            victimX = -1;
            victimY = -1;
            break;
          }
        }

        SerialDebug.print("Status: ");
        SerialDebug.print(victimStatus);

        SerialDebug.print("\tElapsed: ");
        SerialDebug.print(elapsedTime);

        SerialDebug.print("\tX: ");
        SerialDebug.print(victimX);

        SerialDebug.print("\tY: ");
        SerialDebug.println(victimY);
      }
    }
  }
}