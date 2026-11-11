void testI2C()
{
    byte error, address;
    int nDevices = 0;
    while (1)
    {
        // SerialUSB.println("Scanning for I2C");
        delay(10);
        for (address = 0x01; address < 0x7f; address++)
        {
            Wire.beginTransmission(address);
            error = Wire.endTransmission();
            if (error == 0)
            {
                // SerialUSB.print("I2C 0x");
                // SerialUSB.println(address, HEX);
                nDevices++;
            }
            else if (error != 2)
            {
                // SerialDebug.printf("Error %d at address 0x%02X\n", error, address);
            }
            delay(10);
        }
        if (nDevices == 0)
        {
            // SerialUSB.println("No I2C");
        }
        delay(1000);
    }
}

void error(String _txt){
    SerialDebug.println(_txt);
    while(1){
        
    }
}
