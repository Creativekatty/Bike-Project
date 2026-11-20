// Buttons + speaker test (build with env:button_speaker_test) - Waveshare board.
// Every press of either button plays a turn-signal "blinking" sound on the speaker:
// tick (light on) ... tock (light off), repeated BLINKS times. A new press restarts it.
// Each press is printed to the serial monitor. The LEDs are left alone.
//
// Wiring: left button GPIO2, right button GPIO41, both buttons' other leg on GPIO40
//         (driven LOW as their ground). Speaker board trigger on GPIO39 (see speaker.cpp).
#include <Arduino.h>
#include "speaker.h"

const int LEFT_BUTTON_PIN = 2;
const int RIGHT_BUTTON_PIN = 41;
const int BUTTON_GND_PIN = 40;

const unsigned long DEBOUNCE_MS = 50;
const unsigned long BLINK_MS = 500;  // same rhythm as the turn signals
const int BLINKS = 3;                // tick-tock pairs per press

struct Button {
  const char* name;
  int pin;
  int state;
  int lastReading;
  unsigned long lastChangeMs;
};

Button buttons[] = {
  {"Left", LEFT_BUTTON_PIN, HIGH, HIGH, 0},
  {"Right", RIGHT_BUTTON_PIN, HIGH, HIGH, 0},
};

int ticksLeft = 0;            // tick + tock sounds still to play
unsigned long lastSoundMs = 0;
unsigned long pressCount = 0;

void startBlinking(const char* name) {
  pressCount++;
  Serial.printf("%s button pressed (press #%lu) -> blinking sound\n", name, pressCount);
  ticksLeft = BLINKS * 2;
  lastSoundMs = millis() - BLINK_MS;  // play the first tick straight away
}

// Returns true once per press (button pulls the pin LOW)
bool pressed(Button& b) {
  int reading = digitalRead(b.pin);
  if (reading != b.lastReading) {
    b.lastReading = reading;
    b.lastChangeMs = millis();
  }
  if (millis() - b.lastChangeMs >= DEBOUNCE_MS && reading != b.state) {
    b.state = reading;
    return b.state == LOW;
  }
  return false;
}

void setup() {
  speakerBegin();  // speaker off until a button is pressed

  Serial.begin(115200);
  delay(500);

  pinMode(BUTTON_GND_PIN, OUTPUT);
  digitalWrite(BUTTON_GND_PIN, LOW);
  for (Button& b : buttons) pinMode(b.pin, INPUT_PULLUP);

  Serial.println("Buttons + speaker test - press either button");
}

void loop() {
  for (Button& b : buttons) {
    if (pressed(b)) startBlinking(b.name);
  }

  if (ticksLeft > 0 && millis() - lastSoundMs >= BLINK_MS) {
    lastSoundMs = millis();
    // Even = tick (light on), odd = tock (light off)
    if (ticksLeft % 2 == 0) speakerTick();
    else speakerTock();
    ticksLeft--;
  }

  speakerUpdate();  // ends each tick/tock on time
}
