#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <string.h>
#include "speaker.h"

// Buttons wired between their GPIO and GPIO40 (uses the internal pull-ups;
// GPIO40 is driven LOW to act as the buttons' ground)
const int LEFT_BUTTON_PIN = 2;
const int RIGHT_BUTTON_PIN = 41;  // was 12, which is a camera data pin
const int BUTTON_GND_PIN = 40;
// TRIG/PWM input of the MOSFET switch module (kept ON so the rings have power)
const int SWITCH_PIN = 45;
// DIN (data in) of each WS2812B ring
const int LEFT_RING_DATA_PIN = 3;
const int RIGHT_RING_DATA_PIN = 42;
// Rear LED controller inputs; all three boards share the LED power switch above.
const int REAR_LEFT_LIGHT_PIN = 4;
const int REAR_RIGHT_LIGHT_PIN = 5;
const int REAR_BRAKE_LIGHT_PIN = 6;

const int LEDS_PER_RING = 7;
const uint8_t BRIGHTNESS = 80;  // 0-255

const unsigned long DEBOUNCE_MS = 50;
const unsigned long BLINK_MS = 500;  // on/off time of the blink
const unsigned long HOLD_MS = 800;   // hold either button this long for hazards

// Both buttons together: hold = horn, two quick taps = hazards
const unsigned long COMBO_TAP_MS = 300;    // both pressed shorter than this counts as a tap
const unsigned long DOUBLE_TAP_MS = 600;   // max time between the two taps

Adafruit_NeoPixel leftRing(LEDS_PER_RING, LEFT_RING_DATA_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel rightRing(LEDS_PER_RING, RIGHT_RING_DATA_PIN, NEO_GRB + NEO_KHZ800);

enum Mode { OFF, LEFT, RIGHT, HAZARDS };
const char* MODE_NAMES[] = {"off", "left signal", "right signal", "hazards"};

Mode mode = OFF;
bool blinkOn = false;
bool frontLightsOn = false;
bool rearLightsOn = false;
unsigned long lastBlinkMs = 0;
char commandBuffer[32];
size_t commandLength = 0;

struct Button {
  const char* name;
  int pin;
  Mode signal;  // what a short press turns on
  int lastReading;
  int state;
  unsigned long lastChangeMs;
  unsigned long pressedMs;
  bool holdHandled;
  bool usedInCombo;  // pressed together with the other button - no single-button action
};

Button leftButton = {"Left button", LEFT_BUTTON_PIN, LEFT, HIGH, HIGH, 0, 0, false, false};
Button rightButton = {"Right button", RIGHT_BUTTON_PIN, RIGHT, HIGH, HIGH, 0, 0, false, false};

bool comboActive = false;
unsigned long comboStartMs = 0;
unsigned long lastComboTapMs = 0;  // 0 = no recent tap

void showRings() {
  uint32_t red = Adafruit_NeoPixel::Color(255, 0, 0);
  uint32_t yellow = Adafruit_NeoPixel::Color(255, 80, 0);  // orange-amber, like car hazards
  uint32_t white = frontLightsOn ? Adafruit_NeoPixel::Color(255, 255, 255) : 0;
  uint32_t left = white;
  uint32_t right = white;

  if (mode == HAZARDS) {
    if (blinkOn) left = right = yellow;
  } else if (mode == LEFT && blinkOn) {
    left = red;
  } else if (mode == RIGHT && blinkOn) {
    right = red;
  }
  leftRing.fill(left);
  rightRing.fill(right);
  leftRing.show();
  rightRing.show();
}

void showRearLights() {
  digitalWrite(REAR_LEFT_LIGHT_PIN, rearLightsOn ? HIGH : LOW);
  digitalWrite(REAR_RIGHT_LIGHT_PIN, rearLightsOn ? HIGH : LOW);
  digitalWrite(REAR_BRAKE_LIGHT_PIN, rearLightsOn ? HIGH : LOW);
}

bool parseLightCommand(const char* line, const char* prefix, bool& state) {
  size_t prefixLength = strlen(prefix);
  if (strncmp(line, prefix, prefixLength) != 0) return false;
  const char* value = line + prefixLength;
  if (strcmp(value, "1") == 0) state = true;
  else if (strcmp(value, "0") == 0) state = false;
  else return false;
  return true;
}

void handleSerialCommand(char* line) {
  if (strcmp(line, "BIKE?") == 0) {
    Serial.printf("BIKE STATUS FRONT=%d REAR=%d\n", frontLightsOn, rearLightsOn);
    return;
  }
  if (strcmp(line, "BIKE ALERT") == 0) {
    speakerBeep(180);
    Serial.println("BIKE OK ALERT");
    return;
  }
  if (parseLightCommand(line, "BIKE FRONT ", frontLightsOn)) {
    showRings();
    Serial.printf("BIKE OK FRONT=%d\n", frontLightsOn);
    return;
  }
  if (parseLightCommand(line, "BIKE REAR ", rearLightsOn)) {
    showRearLights();
    Serial.printf("BIKE OK REAR=%d\n", rearLightsOn);
    return;
  }
  if (strncmp(line, "BIKE FRONT ", 11) == 0 || strncmp(line, "BIKE REAR ", 10) == 0) {
    Serial.println("BIKE ERR Light state must be 0 or 1");
    return;
  }
  if (strncmp(line, "BIKE", 4) == 0) Serial.println("BIKE ERR Unknown command");
}

void readSerialCommands() {
  while (Serial.available()) {
    char ch = static_cast<char>(Serial.read());
    if (ch == '\r') continue;
    if (ch == '\n') {
      commandBuffer[commandLength] = '\0';
      handleSerialCommand(commandBuffer);
      commandLength = 0;
    } else if (commandLength < sizeof(commandBuffer) - 1) {
      commandBuffer[commandLength++] = ch;
    } else {
      commandLength = 0;
      Serial.println("BIKE ERR Command too long");
    }
  }
}

// Pressing the same mode again turns it off, otherwise switch to the new one
void toggleMode(Mode newMode, const char* source) {
  mode = (mode == newMode) ? OFF : newMode;
  blinkOn = true;
  lastBlinkMs = millis();
  showRings();

  // Tick when a signal starts, beep when everything turns off
  if (mode == OFF) {
    speakerOffBeep();
  } else {
    speakerTick();
  }

  Serial.printf("%s: %s\n", source, MODE_NAMES[mode]);
}

void updateButton(Button& b, const Button& other) {
  int reading = digitalRead(b.pin);

  if (reading != b.lastReading) {
    b.lastChangeMs = millis();
    b.lastReading = reading;
  }

  // Debounced press / release (button pulls the pin LOW)
  if (millis() - b.lastChangeMs > DEBOUNCE_MS && reading != b.state) {
    b.state = reading;
    if (b.state == LOW) {
      b.pressedMs = millis();
      b.holdHandled = false;
    } else {
      if (!b.holdHandled && !b.usedInCombo) {
        toggleMode(b.signal, b.name);  // short press, acted on at release
      }
      b.usedInCombo = false;
    }
  }

  // Holding just this button toggles the hazards
  if (b.state == LOW && other.state == HIGH && !b.holdHandled && !b.usedInCombo &&
      millis() - b.pressedMs >= HOLD_MS) {
    b.holdHandled = true;
    toggleMode(HAZARDS, b.name);
  }
}

// Both buttons down = horn. Two quick taps of both = hazards.
void updateCombo() {
  bool bothDown = (leftButton.state == LOW && rightButton.state == LOW);

  if (bothDown && !comboActive) {
    comboActive = true;
    comboStartMs = millis();
    leftButton.usedInCombo = true;
    rightButton.usedInCombo = true;
    speakerHorn(true);
    Serial.println("Both buttons: horn");
  }

  if (!bothDown && comboActive) {
    comboActive = false;
    speakerHorn(false);

    if (millis() - comboStartMs < COMBO_TAP_MS) {
      if (lastComboTapMs != 0 && millis() - lastComboTapMs < DOUBLE_TAP_MS) {
        lastComboTapMs = 0;
        toggleMode(HAZARDS, "Both buttons double tap");
      } else {
        lastComboTapMs = millis();
      }
    } else {
      lastComboTapMs = 0;  // a long honk isn't a tap
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_GND_PIN, OUTPUT);
  digitalWrite(BUTTON_GND_PIN, LOW);
  pinMode(LEFT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(RIGHT_BUTTON_PIN, INPUT_PULLUP);
  pinMode(SWITCH_PIN, OUTPUT);
  digitalWrite(SWITCH_PIN, HIGH);
  pinMode(REAR_LEFT_LIGHT_PIN, OUTPUT);
  pinMode(REAR_RIGHT_LIGHT_PIN, OUTPUT);
  pinMode(REAR_BRAKE_LIGHT_PIN, OUTPUT);
  showRearLights();

  leftRing.begin();
  leftRing.setBrightness(BRIGHTNESS);
  rightRing.begin();
  rightRing.setBrightness(BRIGHTNESS);
  showRings();
  speakerBegin();

  Serial.println("Ready - left/right button = signal, hold one = hazards, both = horn, double tap both = hazards");
}

void loop() {
  readSerialCommands();
  updateButton(leftButton, rightButton);
  updateButton(rightButton, leftButton);
  updateCombo();

  // Blink the active ring(s)
  if (mode != OFF && millis() - lastBlinkMs >= BLINK_MS) {
    lastBlinkMs = millis();
    blinkOn = !blinkOn;
    showRings();

    // Clock-style tick when the light flashes on, tock when it goes off
    if (blinkOn) {
      speakerTick();
    } else {
      speakerTock();
    }
  }

  // Ends each tick/tock/beep on time
  speakerUpdate();
}
