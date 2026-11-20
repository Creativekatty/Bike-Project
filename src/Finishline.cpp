// Finishline - the finished bike program (build with env:Finishline)
//
// What it does:
//   - Left button  : start / stop the LEFT turn signal (front-left + rear-left blink)
//   - Right button : start / stop the RIGHT turn signal (front-right + rear-right blink)
//   - Both buttons : horn (sounds for as long as you hold them)
//   - While a turn signal blinks, the speaker clicks along with it
//   - Rear lights that aren't blinking are dim red (tail lights)...
//   - ...flash fast and then stay bright red when the bike slows down (brake lights)...
//   - ...and stay bright red while the bike is stopped
//   - Crash: if the moving bike tips more than 45 degrees to either side, every light
//     flashes amber fast (hazards) until the bike is upright again
//   - LoRa: listens for messages from the traffic device and prints them
//   - Radar: warns with a beep-beep-beep when a vehicle comes up from behind
//     (faster beeps the closer it gets)
//   - Idle: parked for 30 seconds -> every ring shows a moving rainbow
//
// IMPORTANT: keep the bike still for the first second after power-up. The program
// measures which way is "down" during that second.

#include <Adafruit_NeoPixel.h>
#include <Wire.h>

// ---------- Pins ----------
#define LEFT_BUTTON_PIN   41   // left button  (swapped with the right one so each button
#define RIGHT_BUTTON_PIN  2    // right button  controls the lights on its own side)
#define BUTTON_GND_PIN    40   // the buttons' shared ground wire (we hold it LOW)
#define SPEAKER_PIN       39   // speaker switch board (HIGH = sound on)
#define IMU_SDA_PIN       15   // IMU (MPU-6050) data wire
#define IMU_SCL_PIN       16   // IMU (MPU-6050) clock wire
#define LORA_RX_PIN       7    // LoRa module TXD -> this pin (we receive here)
#define LORA_TX_PIN       8    // LoRa module RXD -> this pin (we send here)
                               // LoRa M0 and M1 are wired to GND = normal send/receive mode
#define RADAR_RX_PIN      21   // radar TX -> this pin (we receive here)
#define RADAR_TX_PIN      1    // radar RX -> this pin (we send here)

// ---------- Settings you can change ----------
#define LEDS_PER_RING     7    // LEDs on one ring
#define BRIGHTNESS        50   // front rings: 0 (off) to 255 (full)
#define REAR_BRIGHTNESS   255  // rear rings: full power, so the brake light really stands out
#define BLINK_MS          500  // turn signal: on for this long, then off for this long
#define CLICK_MS          15   // how long each speaker click lasts
#define BUTTON_CHECK_MS   20   // how often the buttons are read (also ignores button bounce)
#define REFRESH_MS        1000 // how often the colors are sent again, in case a ring missed them

// ---------- Brake light settings ----------
// Which arrow on the IMU board points toward the FRONT of the bike?
// Pick one:  {1,0,0} = X arrow   {-1,0,0} = opposite the X arrow
//            {0,1,0} = Y arrow   {0,-1,0} = opposite the Y arrow
// It doesn't have to point exactly forward - the program corrects for the tilt.
// If the brake light comes on when you SPEED UP, flip the sign (1 <-> -1).
float FORWARD_ARROW[3] = {0, 1, 0};

#define IMU_CHECK_MS      20    // read the IMU 50 times a second
#define BRAKE_G           0.12  // slowing down harder than this (in g) = braking
#define BRAKE_HOLD_MS     600   // keep the brake light on this long after braking ends
#define STILL_G           0.02  // shaking less than this (in g) counts as "not moving"
#define STOPPED_MS        1500  // not moving for this long = stopped -> brake light on
#define FLASH_MS          70    // braking: each fast flash is on this long, then off this long
#define BRAKE_FLASH_MS    1200  // braking: flash for this long, then stay solid bright red

// ---------- Crash settings ----------
#define CRASH_ANGLE       45    // tipped over more than this many degrees to the side = crash
#define MOVING_RECENT_MS  2000  // only counts as a crash if the bike was moving this recently
#define CRASH_FLASH_MS    150   // crash hazards: on this long, then off this long
#define UPRIGHT_MS        2000  // bike upright again for this long = crash over

// ---------- Idle (rainbow) settings ----------
#define IDLE_AFTER_MS     30000 // parked (no movement, no buttons) this long = rainbow idle mode
#define IDLE_FRAME_MS     30    // how often the rainbow moves (smaller = smoother)
#define IDLE_SPEED        20    // how fast the colors travel around (bigger = faster)
#define IDLE_GLOW         120   // rainbow brightness, 0-255

// ---------- Radar settings ----------
#define RADAR_BAUD        115200 // the radar's serial speed (factory setting)
#define ALERT_RANGE_M     30    // warn about vehicles closer than this (meters)
#define VEHICLE_MIN_KMH   10    // closing faster than this = vehicle (people walk ~5 km/h)
#define NEAR_M            10    // closer than this = fastest warning beeps
#define MID_M             20    // closer than this = medium warning beeps
#define RADAR_LOST_MS     1000  // no vehicle seen for this long = warning stops
#define ALERT_BEEP_MS     60    // each beep (and each gap) in the beep-beep-beep

// ---------- LoRa settings ----------
#define LORA_BAUD         9600  // the LoRa module's serial speed (factory setting)
#define LOOKING_PRINT_MS  5000  // print "looking for Traffic device" this often while nothing arrives

// ---------- LED rings ----------
// Adafruit_NeoPixel(number of LEDs, data pin, LED type)
// Front turn signals
Adafruit_NeoPixel frontLeft(LEDS_PER_RING, 3, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel frontRight(LEDS_PER_RING, 42, NEO_GRB + NEO_KHZ800);
// Rear left side
Adafruit_NeoPixel rearLeftA(LEDS_PER_RING, 4, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel rearLeftB(LEDS_PER_RING, 5, NEO_GRB + NEO_KHZ800);
// Rear right side
Adafruit_NeoPixel rearRightA(LEDS_PER_RING * 2, 12, NEO_GRB + NEO_KHZ800);  // two rings chained together
Adafruit_NeoPixel rearRightB(LEDS_PER_RING, 6, NEO_GRB + NEO_KHZ800);       // this ring is burnt out - replace it

// ---------- Colors ----------
uint32_t AMBER     = Adafruit_NeoPixel::Color(255, 80, 0);  // turn signal color
uint32_t TAIL_RED  = Adafruit_NeoPixel::Color(40, 0, 0);    // rear light while riding (dim)
uint32_t BRAKE_RED = Adafruit_NeoPixel::Color(255, 0, 0);   // rear light when braking / stopped (full bright)
uint32_t OFF       = 0;                                     // LEDs off

// ---------- Which turn signal is on ----------
#define SIGNAL_NONE   0
#define SIGNAL_LEFT   1
#define SIGNAL_RIGHT  2
int activeSignal = SIGNAL_NONE;

// ---------- Things the program remembers ----------
bool blinkOn = false;                 // is the turn signal lit right now?
unsigned long lastBlinkTime = 0;      // when the turn signal last switched on/off
bool clicking = false;                // is a speaker click playing?
unsigned long clickStartTime = 0;     // when the click started
bool hornOn = false;                  // is the horn sounding?
bool leftWasPressed = false;          // left button state last time we checked
bool rightWasPressed = false;         // right button state last time we checked
bool bothWerePressed = false;         // true after a horn press, until both buttons are let go
unsigned long lastButtonCheck = 0;    // when the buttons were last read
unsigned long lastRefresh = 0;        // when the colors were last sent again

// ---------- Brake light memory ----------
#define BRAKE_NONE     0              // riding normally -> dim tail light
#define BRAKE_BRAKING  1              // slowing down    -> fast flashing, then bright
#define BRAKE_STOPPED  2              // stopped         -> steady bright
int brakeState = BRAKE_NONE;
unsigned long brakeStartTime = 0;     // when the current braking started (for the flashing)
uint32_t lastRearColor = 0;           // rear color last sent (so we only resend on a change)

bool imuReady = false;                // did the IMU start up OK?
float gravity[3];                     // which way is "down" (measured at power-up)
float forward[3];                     // which way is "forward" (worked out from FORWARD_ARROW)
float down[3];                        // "down" at power-up, 1 long
float side[3];                        // which way is "sideways" (left/right)
float smoothGravity[3];               // "down" right now, smoothed so bumps don't count

// ---------- Crash memory ----------
bool crashed = false;                 // is the bike in crash mode?
unsigned long uprightSince = 0;       // when the bike was last tipped over (to see it's back up)
bool lastCrashFlash = false;          // crash flash state last sent to the lights

// ---------- Idle memory ----------
bool idleMode = false;                // is the rainbow showing?
unsigned long lastButtonTime = 0;     // when a button was last pressed (wakes the bike up)
unsigned long lastIdleFrame = 0;      // when the rainbow last moved

// ---------- Radar memory ----------
uint8_t radarData[64];                // the frame being received from the radar
int radarStep = 0;                    // where we are in reading a frame (see updateRadar)
int radarCount = 0;                   // bytes read in the current step
int radarLength = 0;                  // how many data bytes this frame has
bool vehicleAlert = false;            // is a vehicle coming up behind?
int vehicleDistance = 0;              // closest vehicle's distance (m)
int vehicleSpeed = 0;                 // closest vehicle's speed (km/h)
unsigned long lastVehicleTime = 0;    // when a vehicle was last seen
unsigned long alertStartTime = 0;     // when the warning beeps started
unsigned long lastVehiclePrint = 0;   // when the vehicle info was last printed

// ---------- LoRa memory ----------
String loraMessage = "";              // the message being received, letter by letter
unsigned long lastLoraTime = 0;       // when the last message arrived
unsigned long lastLookingPrint = 0;   // when "looking for Traffic device" was last printed
float slowing = 0;                    // how hard the bike is slowing down (smoothed, in g)
float shake = 1;                      // how much the bike is shaking (smoothed, in g)
unsigned long lastBrakeTime = 0;      // when we last felt the bike braking
unsigned long stillSince = 0;         // when the bike stopped shaking
unsigned long lastImuCheck = 0;       // when the IMU was last read
unsigned long lastImuPrint = 0;       // when the IMU numbers were last printed


// ===================== LED rings =====================

// Get one ring ready: start it, set the brightness, and turn it off
void setupRing(Adafruit_NeoPixel &ring, int brightness) {
  ring.begin();
  ring.setBrightness(brightness);
  ring.clear();
  ring.show();
}

// Light one ring in one color
void lightRing(Adafruit_NeoPixel &ring, uint32_t color) {
  ring.fill(color);   // set every LED on the ring to this color
  ring.show();        // send the colors to the ring
}

// The color the rear lights should be right now (when they aren't turn-signalling)
uint32_t rearColor() {
  if (brakeState == BRAKE_BRAKING) {
    unsigned long t = millis() - brakeStartTime;   // how long we've been braking
    if (t < BRAKE_FLASH_MS) {
      // Start of braking: flash fast to grab attention
      return ((t / FLASH_MS) % 2 == 0) ? BRAKE_RED : OFF;
    }
    return BRAKE_RED;  // then stay solid bright red
  }
  if (brakeState == BRAKE_STOPPED) return BRAKE_RED;  // stopped: steady bright red
  return TAIL_RED;                                    // riding: dim red
}

// Set the lights on one side of the bike (one front ring + two rear rings)
//   blinking = true  -> this side's turn signal is on: everything flashes amber
//   blinking = false -> front off, rear red (see rearColor above)
void showSide(Adafruit_NeoPixel &front, Adafruit_NeoPixel &rearA, Adafruit_NeoPixel &rearB, bool blinking) {
  if (blinking) {
    uint32_t color = blinkOn ? AMBER : OFF;  // "on" half of the blink = amber, "off" half = dark
    lightRing(front, color);
    lightRing(rearA, color);
    lightRing(rearB, color);
  } else {
    lightRing(front, OFF);
    lightRing(rearA, lastRearColor);
    lightRing(rearB, lastRearColor);
  }
}

// Paint one ring with a rainbow. "offset" shifts the colors so each ring looks different.
void rainbowRing(Adafruit_NeoPixel &ring, uint16_t offset) {
  int count = ring.numPixels();
  uint16_t start = millis() * IDLE_SPEED + offset;   // the colors drift as time passes
  for (int i = 0; i < count; i++) {
    // Spread one full rainbow around the ring (65536 = all the way round the color wheel)
    uint16_t hue = start + i * 65536L / count;
    ring.setPixelColor(i, ring.gamma32(ring.ColorHSV(hue, 255, IDLE_GLOW)));
  }
  ring.show();
}

// Idle mode: every ring shows a moving rainbow, each one a little out of step
void showIdle() {
  rainbowRing(frontLeft, 0);
  rainbowRing(frontRight, 10000);
  rainbowRing(rearLeftA, 20000);
  rainbowRing(rearLeftB, 30000);
  rainbowRing(rearRightA, 40000);
  rainbowRing(rearRightB, 50000);
}

// Crash hazards: is this the "on" half of the fast flash?
bool crashFlashOn() {
  return (millis() / CRASH_FLASH_MS) % 2 == 0;
}

// Update every light so it matches the turn signal and the brake light
void updateLights() {
  // Crash beats everything: every ring flashes amber together
  if (crashed) {
    lastCrashFlash = crashFlashOn();
    uint32_t color = lastCrashFlash ? AMBER : OFF;
    lightRing(frontLeft, color);
    lightRing(frontRight, color);
    lightRing(rearLeftA, color);
    lightRing(rearLeftB, color);
    lightRing(rearRightA, color);
    lightRing(rearRightB, color);
    return;
  }

  lastRearColor = rearColor();

  // Parked: show the rainbow instead
  if (idleMode) {
    showIdle();
    return;
  }
  showSide(frontLeft, rearLeftA, rearLeftB, activeSignal == SIGNAL_LEFT);
  showSide(frontRight, rearRightA, rearRightB, activeSignal == SIGNAL_RIGHT);
}


// ===================== Speaker =====================

// The speaker has three sounds, each with its own rhythm:
//   horn        = one long sound while both buttons are held
//   radar alert = beep-beep-beep, repeating faster as a vehicle gets closer
//   blinker     = a single short tick each time the turn signal flashes

// Start a short speaker click (the "tick" of the turn signal)
void startClick() {
  clicking = true;
  clickStartTime = millis();
}

// Stop the click once it has lasted CLICK_MS
void updateClick() {
  if (clicking && millis() - clickStartTime >= CLICK_MS) {
    clicking = false;
  }
}

bool radarBeepOn();  // defined in the radar section below

// Turn the speaker on if any of the three sounds wants it on
void updateSpeaker() {
  bool soundOn = hornOn || radarBeepOn() || clicking;
  digitalWrite(SPEAKER_PIN, soundOn ? HIGH : LOW);
}


// ===================== Buttons and turn signals =====================

// A turn signal button was pressed: turn that signal on, or off if it was already on
void pressSignal(int signal) {
  if (activeSignal == signal) {
    activeSignal = SIGNAL_NONE;  // pressed again -> turn it off
    Serial.println("Turn signal OFF");
  } else {
    activeSignal = signal;       // switch to this signal
    Serial.println(signal == SIGNAL_LEFT ? "LEFT signal ON" : "RIGHT signal ON");
  }

  // Start the blink fresh, with the light on
  blinkOn = true;
  lastBlinkTime = millis();
  updateLights();
  startClick();
}

// Read the buttons and decide what to do
void checkButtons() {
  // A pressed button pulls its pin LOW
  bool leftPressed = digitalRead(LEFT_BUTTON_PIN) == LOW;
  bool rightPressed = digitalRead(RIGHT_BUTTON_PIN) == LOW;

  // Any button press wakes the bike out of idle mode
  if (leftPressed || rightPressed) lastButtonTime = millis();

  // Both buttons held -> horn on
  if (leftPressed && rightPressed) {
    if (!hornOn) {
      hornOn = true;           // updateSpeaker() turns the sound on
      Serial.println("HORN");
    }
    bothWerePressed = true;  // so letting go doesn't also start a turn signal
  } else if (hornOn) {
    // One or both buttons let go -> horn off
    hornOn = false;
  }

  // A button that was just let go starts/stops its turn signal
  // (but not if it was part of a horn press)
  if (leftWasPressed && !leftPressed && !bothWerePressed) {
    pressSignal(SIGNAL_LEFT);
  }
  if (rightWasPressed && !rightPressed && !bothWerePressed) {
    pressSignal(SIGNAL_RIGHT);
  }

  // Once both buttons are let go, the horn press is over
  if (!leftPressed && !rightPressed) {
    bothWerePressed = false;
  }

  // Remember the buttons for next time
  leftWasPressed = leftPressed;
  rightWasPressed = rightPressed;
}

// Blink the active turn signal and click the speaker in time with it
void updateBlink() {
  if (activeSignal == SIGNAL_NONE) return;  // no signal on, nothing to blink

  if (millis() - lastBlinkTime >= BLINK_MS) {
    lastBlinkTime = millis();
    blinkOn = !blinkOn;     // flip between on and off
    updateLights();
    startClick();           // click on every change: tick ... tock
  }
}


// ===================== IMU and brake light =====================

// Read the IMU's accelerometer. Gives x, y, z in g (1 g = gravity). Returns false if it fails.
bool readAccel(float a[3]) {
  Wire.beginTransmission(0x68);  // the IMU's address
  Wire.write(0x3B);              // start at the accelerometer register
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(0x68, 6) != 6) return false;
  for (int i = 0; i < 3; i++) {
    int16_t raw = (Wire.read() << 8) | Wire.read();  // two bytes per axis
    a[i] = raw / 16384.0;                            // 16384 = 1 g on the +/-2 g range
  }
  return true;
}

// Dot product: how much of vector a points along vector b
float dot(float a[3], float b[3]) {
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// Make a vector 1 long (keeps its direction)
void makeLength1(float v[3]) {
  float length = sqrt(dot(v, v));
  for (int i = 0; i < 3; i++) v[i] /= length;
}

// Start the IMU and measure which way is "down" and "forward"
void setupImu() {
  Wire.begin(IMU_SDA_PIN, IMU_SCL_PIN);

  // Wake the IMU up (it starts asleep)
  Wire.beginTransmission(0x68);
  Wire.write(0x6B);  // power register
  Wire.write(0x01);  // awake, use its gyro clock
  if (Wire.endTransmission() != 0) {
    Serial.println("IMU not found - brake light disabled, rear lights stay dim red");
    return;
  }
  delay(100);

  // Measure gravity: average 50 readings over 1 second while the bike is still
  float sum[3] = {0, 0, 0};
  for (int n = 0; n < 50; n++) {
    float a[3];
    if (!readAccel(a)) {
      Serial.println("IMU read failed - brake light disabled");
      return;
    }
    for (int i = 0; i < 3; i++) sum[i] += a[i];
    delay(20);
  }
  for (int i = 0; i < 3; i++) gravity[i] = sum[i] / 50;

  // Down = gravity, made 1 long
  for (int i = 0; i < 3; i++) {
    down[i] = gravity[i];
    smoothGravity[i] = gravity[i];
  }
  makeLength1(down);

  // Forward = the chosen arrow, with the "down" part removed (so it lies flat)
  float along = dot(FORWARD_ARROW, down);
  for (int i = 0; i < 3; i++) forward[i] = FORWARD_ARROW[i] - along * down[i];
  makeLength1(forward);

  // Sideways = at right angles to both forward and down (a "cross product")
  side[0] = forward[1] * down[2] - forward[2] * down[1];
  side[1] = forward[2] * down[0] - forward[0] * down[2];
  side[2] = forward[0] * down[1] - forward[1] * down[0];

  imuReady = true;
  Serial.printf("IMU ready. Gravity x%.2f y%.2f z%.2f g\n", gravity[0], gravity[1], gravity[2]);
}

// Read the IMU and decide if the brake light should be on
void updateBrake() {
  if (!imuReady) return;

  float a[3];
  if (!readAccel(a)) return;  // skip this reading if it failed

  // Take gravity away -> what's left is the bike's own movement
  float moving[3];
  for (int i = 0; i < 3; i++) moving[i] = a[i] - gravity[i];

  // Slowing down = accelerating backwards. Smooth it so bumps don't count.
  float forwardAccel = dot(moving, forward);
  slowing = slowing * 0.8 + (-forwardAccel) * 0.2;

  // Shaking = how much the reading jumps around. A rolling bike always shakes a little.
  float jump = sqrt(dot(moving, moving));
  shake = shake * 0.9 + jump * 0.1;

  unsigned long now = millis();

  // Braking?
  if (slowing > BRAKE_G) lastBrakeTime = now;
  bool braking = (now - lastBrakeTime < BRAKE_HOLD_MS) && lastBrakeTime != 0;

  // Stopped? (no shaking for STOPPED_MS)
  if (shake > STILL_G) stillSince = now;
  bool stopped = (now - stillSince >= STOPPED_MS);

  // Work out the brake state: braking beats stopped, stopped beats riding
  int newState = BRAKE_NONE;
  if (braking) newState = BRAKE_BRAKING;
  else if (stopped) newState = BRAKE_STOPPED;

  if (newState != brakeState) {
    if (newState == BRAKE_BRAKING) brakeStartTime = now;  // start the flashing
    brakeState = newState;
    updateLights();
    if (brakeState == BRAKE_BRAKING) Serial.println("BRAKE light ON (braking - flashing)");
    else if (brakeState == BRAKE_STOPPED) Serial.println("BRAKE light ON (stopped)");
    else Serial.println("Brake light off");
  }

  // ----- Crash check -----
  // Smooth the reading so bumps don't look like tipping over
  for (int i = 0; i < 3; i++) smoothGravity[i] = smoothGravity[i] * 0.9 + a[i] * 0.1;

  // Sideways tilt angle: how far "down" has swung toward the side, in degrees
  float tilt = atan2(dot(smoothGravity, side), dot(smoothGravity, down)) * 180.0 / PI;
  bool tippedOver = fabs(tilt) > CRASH_ANGLE;              // either side
  bool movingRecently = (now - stillSince) < MOVING_RECENT_MS;

  if (!crashed && tippedOver && movingRecently) {
    crashed = true;
    uprightSince = now;
    Serial.printf("bike crashed (tilted %.0f degrees)\n", tilt);
    updateLights();
  }
  if (crashed) {
    if (tippedOver) uprightSince = now;           // still on its side
    if (now - uprightSince >= UPRIGHT_MS) {       // upright long enough -> crash over
      crashed = false;
      Serial.println("Bike upright again - crash lights off");
      updateLights();
    }
  }

  // Send the new colors whenever a flash changes (brake flashing or crash hazards)
  if (crashed ? (crashFlashOn() != lastCrashFlash) : (rearColor() != lastRearColor)) {
    updateLights();
  }

  // Print the numbers once a second, to help tune BRAKE_G and STILL_G
  if (now - lastImuPrint >= 1000) {
    lastImuPrint = now;
    Serial.printf("IMU: slowing %.2f g, shake %.3f g\n", slowing, shake);
  }
}


// ===================== Idle mode (rainbow when parked) =====================

// Turn the rainbow on after the bike has been parked a while, and off as soon as anything happens
void updateIdle() {
  unsigned long now = millis();

  // Parked = no shaking and no button presses for IDLE_AFTER_MS.
  // Never idle while signalling, after a crash, or while a vehicle is coming.
  bool parked = imuReady
             && now - stillSince >= IDLE_AFTER_MS
             && now - lastButtonTime >= IDLE_AFTER_MS
             && activeSignal == SIGNAL_NONE
             && !crashed
             && !vehicleAlert;

  if (parked != idleMode) {
    idleMode = parked;
    Serial.println(idleMode ? "Idle mode ON - rainbow" : "Idle mode OFF");
    updateLights();
  }

  // Move the rainbow along
  if (idleMode && now - lastIdleFrame >= IDLE_FRAME_MS) {
    lastIdleFrame = now;
    showIdle();
  }
}


// ===================== Radar (vehicles behind the bike) =====================
//
// The radar sends "frames" like this:
//   F4 F3 F2 F1 | length (2 bytes) | data | F8 F7 F6 F5
// Data: [0] number of targets, [1] alarm, then 5 bytes per target:
//   angle, distance (m), direction (1 = coming closer), speed (km/h), signal strength
// (This layout comes from community radar libraries, not the official manual.)

// Should the radar warning beep be sounding right now?
bool radarBeepOn() {
  if (!vehicleAlert) return false;

  // Closer vehicle = shorter pause between the beep-beep-beeps
  int pause = 1000;                              // far away
  if (vehicleDistance <= MID_M) pause = 500;     // getting close
  if (vehicleDistance <= NEAR_M) pause = 150;    // very close

  // One round = 3 beeps with gaps, then the pause
  unsigned long beeps = 6 * ALERT_BEEP_MS;
  unsigned long t = (millis() - alertStartTime) % (beeps + pause);
  if (t >= beeps) return false;                  // in the pause
  return (t / ALERT_BEEP_MS) % 2 == 0;           // beep, gap, beep, gap, beep, gap
}

// Start listening to the radar
void setupRadar() {
  Serial2.begin(RADAR_BAUD, SERIAL_8N1, RADAR_RX_PIN, RADAR_TX_PIN);
}

// A full frame arrived: look for vehicles coming closer
void handleRadarFrame() {
  if (radarLength < 2) return;              // empty frame = nothing seen
  int targets = radarData[0];

  int closest = 999;
  int closestSpeed = 0;
  for (int i = 0; i < targets; i++) {
    int at = 2 + i * 5;                     // where this target's 5 bytes start
    if (at + 5 > radarLength) break;        // frame too short - stop
    int distance = radarData[at + 1];
    bool comingCloser = radarData[at + 2] == 1;
    int speed = radarData[at + 3];

    // A vehicle = coming closer, fast enough, and within range
    if (comingCloser && speed >= VEHICLE_MIN_KMH && distance <= ALERT_RANGE_M && distance < closest) {
      closest = distance;
      closestSpeed = speed;
    }
  }

  if (closest == 999) return;               // no vehicle in this frame

  vehicleDistance = closest;
  vehicleSpeed = closestSpeed;
  lastVehicleTime = millis();
  if (!vehicleAlert) {
    vehicleAlert = true;
    alertStartTime = millis();
    Serial.printf("VEHICLE behind! %d m, %d km/h\n", vehicleDistance, vehicleSpeed);
  }
}

// Read the radar's bytes one at a time and put frames together
void updateRadar() {
  while (Serial2.available()) {
    uint8_t b = Serial2.read();

    if (radarStep == 0) {            // looking for the start: F4 F3 F2 F1
      uint8_t start[4] = {0xF4, 0xF3, 0xF2, 0xF1};
      radarCount = (b == start[radarCount]) ? radarCount + 1 : (b == 0xF4 ? 1 : 0);
      if (radarCount == 4) { radarStep = 1; radarCount = 0; radarLength = 0; }
    } else if (radarStep == 1) {     // the 2-byte length
      radarLength |= b << (8 * radarCount);
      if (++radarCount == 2) {
        radarCount = 0;
        if (radarLength > (int)sizeof(radarData)) radarStep = 0;  // nonsense - start over
        else radarStep = (radarLength == 0) ? 3 : 2;
      }
    } else if (radarStep == 2) {     // the data bytes
      radarData[radarCount++] = b;
      if (radarCount == radarLength) { radarStep = 3; radarCount = 0; }
    } else {                         // the end: F8 F7 F6 F5
      uint8_t end[4] = {0xF8, 0xF7, 0xF6, 0xF5};
      if (b == end[radarCount]) {
        if (++radarCount == 4) {     // whole frame received
          handleRadarFrame();
          radarStep = 0;
          radarCount = 0;
        }
      } else {
        radarStep = 0;               // broken frame - start over
        radarCount = 0;
      }
    }
  }

  // Vehicle gone (not seen for a while) -> stop the warning
  unsigned long now = millis();
  if (vehicleAlert && now - lastVehicleTime > RADAR_LOST_MS) {
    vehicleAlert = false;
    Serial.println("Vehicle gone");
  }

  // While warning, print the vehicle's distance once a second
  if (vehicleAlert && now - lastVehiclePrint >= 1000) {
    lastVehiclePrint = now;
    Serial.printf("VEHICLE behind: %d m, %d km/h\n", vehicleDistance, vehicleSpeed);
  }
}


// ===================== LoRa (traffic device) =====================

// Start listening on the LoRa module
void setupLora() {
  Serial1.begin(LORA_BAUD, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN);
  Serial.println("looking for Traffic device");
  lastLookingPrint = millis();
}

// Collect any letters that arrived; print each full message (messages end with a new line)
void updateLora() {
  while (Serial1.available()) {
    char c = Serial1.read();
    if (c == '\n' || c == '\r') {
      if (loraMessage.length() > 0) {
        Serial.println("Traffic device: " + loraMessage);
        loraMessage = "";
        lastLoraTime = millis();
      }
    } else if (loraMessage.length() < 100) {
      loraMessage += c;  // add this letter to the message
    }
  }

  // Nothing heard for a while -> remind that we're still listening
  unsigned long now = millis();
  if (now - lastLoraTime >= LOOKING_PRINT_MS && now - lastLookingPrint >= LOOKING_PRINT_MS) {
    lastLookingPrint = now;
    Serial.println("looking for Traffic device");
  }
}


// ===================== Start and main loop =====================

void setup() {
  // Speaker off first, so it doesn't sound while the program starts
  pinMode(SPEAKER_PIN, OUTPUT);
  digitalWrite(SPEAKER_PIN, LOW);

  Serial.begin(115200);

  // Buttons: one leg on the button pin, the other on BUTTON_GND_PIN (held LOW)
  pinMode(BUTTON_GND_PIN, OUTPUT);
  digitalWrite(BUTTON_GND_PIN, LOW);
  pinMode(LEFT_BUTTON_PIN, INPUT_PULLUP);   // reads HIGH until the button is pressed
  pinMode(RIGHT_BUTTON_PIN, INPUT_PULLUP);

  // Get every ring ready
  setupRing(frontLeft, BRIGHTNESS);
  setupRing(frontRight, BRIGHTNESS);
  setupRing(rearLeftA, REAR_BRIGHTNESS);
  setupRing(rearLeftB, REAR_BRIGHTNESS);
  setupRing(rearRightA, REAR_BRIGHTNESS);
  setupRing(rearRightB, REAR_BRIGHTNESS);

  // Front off, rear red
  updateLights();

  // IMU: keep the bike still during this second
  setupImu();

  // LoRa: start listening for the traffic device
  setupLora();

  // Radar: start watching for vehicles behind
  setupRadar();

  Serial.println("Finishline ready: left/right button = turn signal, both = horn");
}

void loop() {
  // Read the buttons every BUTTON_CHECK_MS
  if (millis() - lastButtonCheck >= BUTTON_CHECK_MS) {
    lastButtonCheck = millis();
    checkButtons();
  }

  // Read the IMU every IMU_CHECK_MS
  if (millis() - lastImuCheck >= IMU_CHECK_MS) {
    lastImuCheck = millis();
    updateBrake();
  }

  updateBlink();   // blink the turn signal
  updateClick();   // end each speaker click on time
  updateLora();    // listen for the traffic device
  updateRadar();   // watch for vehicles behind
  updateSpeaker(); // play whichever sound is needed (horn, radar warning, or blinker tick)
  updateIdle();    // rainbow when parked

  // Send the colors again now and then, in case a ring missed them
  if (millis() - lastRefresh >= REFRESH_MS) {
    lastRefresh = millis();
    updateLights();
  }
}
