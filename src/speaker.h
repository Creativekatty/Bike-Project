#pragma once

// SFM-27 active buzzer, powered through a MOSFET switch module.
// An active buzzer has one fixed tone: the code only turns it on and off.

void speakerBegin();

// Start a beep of the given length. Doesn't block - call speakerUpdate()
// every loop() so the beep ends on time.
void speakerBeep(unsigned long ms);
void speakerUpdate();

void speakerOn();
void speakerOff();

// Turn-signal click (tick = light on, tock = light off)
void speakerTick();
void speakerTock();

// Longer beep when the signals or hazards are switched off
void speakerOffBeep();

// Horn: full volume, sounds until turned off, overrides ticks and beeps
void speakerHorn(bool on);
