void enableISR() {
  // attachInterrupt(digitalPinToInterrupt(enc1), updateEncoder, CHANGE);
  // attachInterrupt(digitalPinToInterrupt(enc2), updateEncoder, CHANGE);
}

void disableISR() {
  // detachInterrupt(digitalPinToInterrupt(enc1));
  // detachInterrupt(digitalPinToInterrupt(enc2));
}

void resetEncoder() {
  // disableISR();
  // encoderPos = 0;
  // enableISR();
}

void updateEncoder() {
  // int MSB = digitalRead(enc1);  // Most significant bit
  // int LSB = digitalRead(enc2);  // Least significant bit

  // int encoded = (MSB << 1) | LSB;          // Converting the 2 pin value to single number
  // int sum = (lastEncoded << 2) | encoded;  // Shifting and adding current encoded value

  // if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderPos++;
  // if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderPos--;

  // lastEncoded = encoded;  // Store this value for next time
}