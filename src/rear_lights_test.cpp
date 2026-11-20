// Rear lights test (build with env:rear_lights_test) - Waveshare board.
// Turns on the LED controller boards, then loops through each rear light so you can see
// which ones work: left blinker, right blinker, brake lights, then everything together.
// Every step is printed to the serial monitor.
//
// Wiring: rear-left blinker DIN -> GPIO5, rear-right blinker DIN -> GPIO4,
//         brake lights DIN -> GPIO6, rear LED board enable (TRIG/PWM) -> GPIO12
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

const int REAR_LEFT_DATA_PIN = 5;
const int REAR_RIGHT_DATA_PIN = 4;
const int BRAKE_DATA_PIN = 6;
const int REAR_ENABLE_PIN = 12;      // TRIG/PWM of the rear LED board (HIGH = power on)
const int SPEAKER_SWITCH_PIN = 39;   // speaker board trigger - held LOW so the speaker stays off

const int LEDS_PER_RING = 7;
const int BRAKE_LEDS = 2 * LEDS_PER_RING;  // both brake rings, chained or side by side
const uint8_t BRIGHTNESS = 80;             // 0-255, same as the front lights

const unsigned long BLINK_MS = 500;

Adafruit_NeoPixel rearLeft(LEDS_PER_RING, REAR_LEFT_DATA_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel rearRight(LEDS_PER_RING, REAR_RIGHT_DATA_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel brake(BRAKE_LEDS, BRAKE_DATA_PIN, NEO_GRB + NEO_KHZ800);

const uint32_t AMBER = Adafruit_NeoPixel::Color(255, 80, 0);
const uint32_t RED = Adafruit_NeoPixel::Color(255, 0, 0);
const uint32_t DIM_RED = Adafruit_NeoPixel::Color(60, 0, 0);  // tail light level

void show(Adafruit_NeoPixel& ring, uint32_t color) {
  ring.fill(color);
  ring.show();
}

void allOff() {
  show(rearLeft, 0);
  show(rearRight, 0);
  show(brake, 0);
}

void blink(Adafruit_NeoPixel& ring, const char* name, int times) {
  Serial.printf("%s blinker: blinking %d times\n", name, times);
  for (int i = 0; i < times; i++) {
    show(ring, AMBER);
    delay(BLINK_MS);
    show(ring, 0);
    delay(BLINK_MS);
  }
}

void setup() {
  // Speaker off first: a floating trigger can switch the speaker fully on and overload the supply
  pinMode(SPEAKER_SWITCH_PIN, OUTPUT);
  digitalWrite(SPEAKER_SWITCH_PIN, LOW);

  Serial.begin(115200);
  delay(500);
  Serial.println("Rear lights test");

  pinMode(REAR_ENABLE_PIN, OUTPUT);
  digitalWrite(REAR_ENABLE_PIN, HIGH);  // power the rear LED board
  Serial.println("Rear LED board switched ON (GPIO12 HIGH)");

  rearLeft.begin();
  rearRight.begin();
  brake.begin();
  rearLeft.setBrightness(BRIGHTNESS);
  rearRight.setBrightness(BRIGHTNESS);
  brake.setBrightness(BRIGHTNESS);
  allOff();
}

void loop() {
  blink(rearLeft, "Rear-left (GPIO5)", 3);
  blink(rearRight, "Rear-right (GPIO4)", 3);

  Serial.println("Brake lights (GPIO6): full red for 2 s");
  show(brake, RED);
  delay(2000);
  Serial.println("Brake lights (GPIO6): dim tail light for 2 s");
  show(brake, DIM_RED);
  delay(2000);

  Serial.println("All rear lights on for 3 s");
  show(rearLeft, AMBER);
  show(rearRight, AMBER);
  show(brake, RED);
  delay(3000);

  allOff();
  Serial.println("All off - starting again\n");
  delay(1000);
}
