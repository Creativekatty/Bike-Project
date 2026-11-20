// Data wire test (build with env:pin_test) - Waveshare board.
// Holds the rear chain data pin (GPIO12) HIGH for 10 s, then LOW for 5 s,
// over and over. Measure with a multimeter between each ring's DIN and GND:
//   ~3.3 V while HIGH and ~0 V while LOW = the wire is good, look at the ring itself
//   0 V all the time                     = broken wire or solder joint
// The serial monitor says which state the pins are in right now.
#include <Arduino.h>

const int TEST_PINS[] = {12};
const char* TEST_NAMES[] = {"Rear chain (GPIO12)"};
const int TEST_COUNT = sizeof(TEST_PINS) / sizeof(TEST_PINS[0]);

const int SPEAKER_SWITCH_PIN = 39;  // held LOW so the buzzer stays off

const unsigned long HIGH_MS = 10000;
const unsigned long LOW_MS = 5000;

void setAll(int level) {
  for (int i = 0; i < TEST_COUNT; i++) digitalWrite(TEST_PINS[i], level);
}

void setup() {
  pinMode(SPEAKER_SWITCH_PIN, OUTPUT);
  digitalWrite(SPEAKER_SWITCH_PIN, LOW);

  Serial.begin(115200);
  delay(500);
  Serial.println("Data wire test - measure each ring's DIN against GND");
  for (int i = 0; i < TEST_COUNT; i++) {
    pinMode(TEST_PINS[i], OUTPUT);
    Serial.printf("  %s\n", TEST_NAMES[i]);
  }
}

void loop() {
  setAll(HIGH);
  Serial.println("Pins HIGH for 10 s -> expect ~3.3 V at DIN");
  delay(HIGH_MS);

  setAll(LOW);
  Serial.println("Pins LOW for 5 s  -> expect ~0 V at DIN");
  delay(LOW_MS);
}
