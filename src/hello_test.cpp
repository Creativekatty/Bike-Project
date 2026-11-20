// Hello World test: prints a line every second so you can see the board is alive.
// Build with env:hello_waveshare (Waveshare) - the serial port is set in platformio.ini.
#include <Arduino.h>

const int SPEAKER_SWITCH_PIN = 39;  // speaker board trigger

void setup() {
  // Keep the speaker OFF
  pinMode(SPEAKER_SWITCH_PIN, OUTPUT);
  digitalWrite(SPEAKER_SWITCH_PIN, LOW);

  Serial.begin(115200);
  delay(500);
  Serial.println("Hello World test");
}

void loop() {
  static unsigned long count = 0;
  Serial.printf("Hello World %lu (uptime %lu s)\n", ++count, millis() / 1000);
  delay(1000);
}
