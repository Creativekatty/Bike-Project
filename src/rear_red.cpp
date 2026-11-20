
#include <Adafruit_NeoPixel.h>

#define NUM_LEDS 7

Adafruit_NeoPixel ring1(7, 12, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel ring2(7, 4, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel ring3(7, 5, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel ring4(7, 1, NEO_GRB + NEO_KHZ800);

void setup() {
  ring1.begin();
  ring2.begin();
  ring3.begin();
  ring4.begin();

  ring1.setBrightness(50);
  ring2.setBrightness(50);
  ring3.setBrightness(50);
  ring4.setBrightness(50);

  ring1.clear();
  ring2.clear();
  ring3.clear();
  ring4.clear();

  ring1.show();
  ring2.show();
  ring3.show();
  ring4.show();
}

void loop() {
  // Ring 1 - Red
  ring1.fill(ring1.Color(255, 0, 0));

  // Ring 2 - Green
  ring2.fill(ring2.Color(0, 255, 0));

  // Ring 3 - Blue
  ring3.fill(ring3.Color(0, 0, 255));

  // Ring 4 - Purple
  ring4.fill(ring4.Color(128, 0, 255));

  ring1.show();
  ring2.show();
  ring3.show();
  ring4.show();

  delay(1000);
}
