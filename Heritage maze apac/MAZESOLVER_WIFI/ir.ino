void interpretIR(bool print) {
  // int x = analogRead(A4);
  // int y = analogRead(A5);

  // long z = (x + y) / 2;
  // calibrated_ir = alpha_ir * (calibrated_ir) + (1 - alpha_ir) * z;
  // if (print) {
  //   SerialUSB.print(int((offset_ir - z) / 20));
  //   SerialUSB.print(",");
  //   SerialUSB.println(int((offset_ir - calibrated_ir) / 20));
  // } else {
  //   if (round((offset_ir - calibrated_ir) / 20) < -100) {  // HAZARD ZONE -30
  //     moveMotor(0, 0);
  //     delay(500);
  //     MoveStrightToDistance(-85);
  //     turnBot(90, RIGHT);
  //     delay(500);
  //     // turnBot(90, RIGHT);
  //     // delay(500);
  //   }
  //   if (round((offset_ir - calibrated_ir) / 20) > 25) {  // PUDDLE -20
  //     MoveStrightToDistance(210);
  //     for (int i = 0; i < 5; i++) {
  //       digitalWrite(LED13, HIGH);
  //       delay(500);
  //       digitalWrite(LED13, LOW);
  //       delay(500);
  //     }
  //     MoveStrightToDistance(110);
  //     delay(500);
  //   } //end
  // }
}