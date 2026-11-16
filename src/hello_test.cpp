// Hello World test: prints a line every second so you can see the board is alive.
// Build with env:hello_waveshare (Waveshare) - the serial port is set in platformio.ini.
#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Hello World test");
}

void loop() {
  static unsigned long count = 0;
  Serial.printf("Hello World %lu (uptime %lu s)\n", ++count, millis() / 1000);
  delay(1000);
}
