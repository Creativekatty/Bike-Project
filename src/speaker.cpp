#include <Arduino.h>
#include "speaker.h"

// TRIG/PWM input of the MOSFET switch module that powers the buzzer
const int SPEAKER_SWITCH_PIN = 39;

// Volume 1-100 %. The buzzer is PWM'd at 20 kHz (the module's max, and above
// hearing) so it sees a lower average voltage and sounds quieter.
const int VOLUME_PERCENT = 30;
const int PWM_FREQ_HZ = 20000;
const int PWM_RESOLUTION_BITS = 8;
const int PWM_CHANNEL = 0;  // only used on Arduino core 2.x

// Clock-style click: very short pulses sound like a tick instead of a beep.
// The tock is quieter so the pair sounds like "tick... tock".
const unsigned long TICK_MS = 4;
const unsigned long TOCK_MS = 4;
const int TICK_VOLUME_PERCENT = VOLUME_PERCENT;
const int TOCK_VOLUME_PERCENT = VOLUME_PERCENT / 2;

const unsigned long OFF_BEEP_MS = 150;

// The horn is loud on purpose and overrides ticks/beeps while it sounds
const int HORN_VOLUME_PERCENT = 100;

bool hornOn = false;
bool beeping = false;
unsigned long beepStartMs = 0;
unsigned long beepLengthMs = 0;

static void writeDuty(uint32_t duty) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(SPEAKER_SWITCH_PIN, duty);
#else
  ledcWrite(PWM_CHANNEL, duty);
#endif
}

void speakerBegin() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(SPEAKER_SWITCH_PIN, PWM_FREQ_HZ, PWM_RESOLUTION_BITS);
#else
  ledcSetup(PWM_CHANNEL, PWM_FREQ_HZ, PWM_RESOLUTION_BITS);
  ledcAttachPin(SPEAKER_SWITCH_PIN, PWM_CHANNEL);
#endif
  speakerOff();
}

static void speakerOnAt(int volumePercent) {
  const uint32_t maxDuty = (1 << PWM_RESOLUTION_BITS) - 1;
  writeDuty(maxDuty * volumePercent / 100);
}

void speakerOn() {
  speakerOnAt(VOLUME_PERCENT);
}

void speakerOff() {
  writeDuty(0);
  beeping = false;
}

static void beepAt(unsigned long ms, int volumePercent) {
  if (hornOn) return;
  speakerOnAt(volumePercent);
  beeping = true;
  beepStartMs = millis();
  beepLengthMs = ms;
}

void speakerBeep(unsigned long ms) {
  beepAt(ms, VOLUME_PERCENT);
}

void speakerUpdate() {
  if (beeping && millis() - beepStartMs >= beepLengthMs) {
    speakerOff();
  }
}

void speakerTick() {
  beepAt(TICK_MS, TICK_VOLUME_PERCENT);
}

void speakerTock() {
  beepAt(TOCK_MS, TOCK_VOLUME_PERCENT);
}

void speakerOffBeep() {
  speakerBeep(OFF_BEEP_MS);
}

void speakerHorn(bool on) {
  if (on == hornOn) return;
  hornOn = on;
  if (on) {
    beeping = false;
    speakerOnAt(HORN_VOLUME_PERCENT);
  } else {
    speakerOff();
  }
}
