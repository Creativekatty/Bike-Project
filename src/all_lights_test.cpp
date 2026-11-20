// All lights test (build with env:all_lights_test) - Waveshare board.
// Turns on the LED controller boards, then lights each LED group on its own so you can
// see which ones work, then everything together. Every step is printed to the serial monitor.
//
// Wiring: front-left DIN -> GPIO3, front-right DIN -> GPIO42,
//         rear-left blinker DIN -> GPIO5, rear-right blinker DIN -> GPIO12,
//         brake lights DIN -> GPIO6. Rings wired straight to 5V/GND (no MOSFET boards).
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

const int FRONT_LEFT_DATA_PIN = 3;
const int FRONT_RIGHT_DATA_PIN = 42;
const int REAR_LEFT_DATA_PIN = 5;
const int REAR_RIGHT_DATA_PIN = 12;  // moved from GPIO4, wired straight to the ring (no MOSFET board)
const int BRAKE_DATA_PIN = 6;
const int LED_ENABLE_PIN = -1;       // controller boards' trigger: -1 = not wired
const int SPEAKER_SWITCH_PIN = 39;   // held LOW so the buzzer stays off

const int LEDS_PER_RING = 7;
const int BRAKE_LEDS = 2 * LEDS_PER_RING;  // both brake rings, chained or side by side
// GPIO5 -> DIN of the first ring, its DOUT -> DIN of the second ring. The data for the
// second ring passes through the first, so one strip of both rings' LEDs drives the chain.
const int REAR_LEFT_LEDS = 2 * LEDS_PER_RING;
const uint8_t BRIGHTNESS = 80;             // 0-255, same as the main program

const unsigned long STEP_MS = 2000;

struct Light {
  const char* name;
  Adafruit_NeoPixel strip;
  uint32_t color;
};

Light lights[] = {
  {"Front-left (GPIO3)", Adafruit_NeoPixel(LEDS_PER_RING, FRONT_LEFT_DATA_PIN, NEO_GRB + NEO_KHZ800),
   Adafruit_NeoPixel::Color(255, 80, 0)},
  {"Front-right (GPIO42)", Adafruit_NeoPixel(LEDS_PER_RING, FRONT_RIGHT_DATA_PIN, NEO_GRB + NEO_KHZ800),
   Adafruit_NeoPixel::Color(255, 80, 0)},
#ifndef FRONT_ONLY  // env:front_lights_test lights only the two front rings
  {"Rear-left chain (GPIO5)", Adafruit_NeoPixel(REAR_LEFT_LEDS, REAR_LEFT_DATA_PIN, NEO_GRB + NEO_KHZ800),
   Adafruit_NeoPixel::Color(255, 80, 0)},
  {"Rear-right (GPIO12)", Adafruit_NeoPixel(BRAKE_LEDS, REAR_RIGHT_DATA_PIN, NEO_GRB + NEO_KHZ800),
   Adafruit_NeoPixel::Color(255, 80, 0)},
  {"Brake lights (GPIO6)", Adafruit_NeoPixel(BRAKE_LEDS, BRAKE_DATA_PIN, NEO_GRB + NEO_KHZ800),
   Adafruit_NeoPixel::Color(255, 0, 0)},
#endif
};
const int LIGHT_COUNT = sizeof(lights) / sizeof(lights[0]);

void allOff() {
  for (Light& l : lights) {
    l.strip.clear();
    l.strip.show();
  }
}

void setup() {
  pinMode(SPEAKER_SWITCH_PIN, OUTPUT);
  digitalWrite(SPEAKER_SWITCH_PIN, LOW);

  Serial.begin(115200);
  delay(500);
  Serial.println("All lights test");

  for (Light& l : lights) {
    l.strip.begin();
    l.strip.setBrightness(BRIGHTNESS);
  }
  allOff();

  if (LED_ENABLE_PIN >= 0) {
    pinMode(LED_ENABLE_PIN, OUTPUT);
    digitalWrite(LED_ENABLE_PIN, HIGH);  // power the LED boards
    Serial.printf("LED boards switched ON (GPIO%d HIGH)\n", LED_ENABLE_PIN);
  }
}

void loop() {
#ifdef ALL_ON
  // env:all_lights_on - every light stays on; the data is resent once a second
  for (Light& l : lights) {
    l.strip.fill(l.color);
    l.strip.show();
  }
  Serial.println("ALL lights ON (GPIO3, 42, 5, 12, 6)");
  delay(1000);
  return;
#endif
  for (Light& l : lights) {
    Serial.printf("%s: ON\n", l.name);
    l.strip.fill(l.color);
    l.strip.show();
    delay(STEP_MS);
    l.strip.clear();
    l.strip.show();
  }

  Serial.println("ALL lights ON");
  for (Light& l : lights) {
    l.strip.fill(l.color);
    l.strip.show();
  }
  delay(STEP_MS);

  allOff();
  Serial.println("All off - starting again\n");
  delay(1000);
}
