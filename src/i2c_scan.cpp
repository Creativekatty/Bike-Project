// I2C scanner (build with env:i2c_scan)
// Lists every I2C device found on the board's I2C pins.
#include <Arduino.h>
#include <Wire.h>

void scan(int sda, int scl) {
  Wire.begin(sda, scl);
  Serial.printf("Scanning SDA=GPIO%d SCL=GPIO%d:", sda, scl);
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf(" 0x%02X", addr);
      found++;
    }
  }
  Serial.println(found ? "" : " nothing found");
  Wire.end();
}

void setup() {
  Serial.begin(115200);
  delay(500);
}

void loop() {
  scan(15, 16);  // V2 board I2C (battery gauge at 0x36)
  scan(3, 2);    // V1 board I2C
  Serial.println();
  delay(3000);
}
