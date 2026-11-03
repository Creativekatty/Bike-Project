// Speaker test program (build with env:speaker_test)
// Plays each beep pattern in a loop and prints what is playing to the Serial Monitor.
#include <Arduino.h>
#include "speaker.h"

// Wait while still letting running beeps end on time
void wait(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    speakerUpdate();
    delay(1);
  }
}

void setup() {
  Serial.begin(115200);
  speakerBegin();
  Serial.println("Speaker test - buzzer switch on GPIO39");
}

void loop() {
  Serial.println("0) Clock tick-tock (turn-signal speed)");
  for (int i = 0; i < 10; i++) {
    speakerTick();
    wait(500);
    speakerTock();
    wait(500);
  }
  wait(1000);

  Serial.println("1) One long beep (1 s)");
  speakerBeep(1000);
  wait(2000);

  Serial.println("2) Three short beeps");
  for (int i = 0; i < 3; i++) {
    speakerBeep(100);
    wait(300);
  }
  wait(1000);

  Serial.println("3) Turn-signal tick-tock");
  for (int i = 0; i < 6; i++) {
    speakerTick();
    wait(500);
    speakerTock();
    wait(500);
  }
  wait(1000);

  Serial.println("4) Off beep");
  speakerOffBeep();
  wait(3000);
}
