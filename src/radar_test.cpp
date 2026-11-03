// HLK-LD2451 24 GHz vehicle radar test (build with env:radar_test)
// Reads the radar's UART frames and prints every vehicle target it sees.
//
// Wiring: radar VIN -> 5V, GND -> GND,
//         radar TX -> RADAR_RX_PIN, radar RX -> RADAR_TX_PIN,
//         radar OT1 (optional) -> RADAR_OT1_PIN
#include <Arduino.h>

#if defined(RADAR_FREENOVE)
// Freenove ESP32-S3 (env:radar_test_freenove): radar TX is wired to GPIO16, radar RX to GPIO17
const int RADAR_RX_PIN = 16;   // ESP32 receives here (radar TX)
const int RADAR_TX_PIN = 17;   // ESP32 sends here (radar RX)
#else
// Not GPIO17/18 (labeled TX1/RX1): the 4G modem uses those.
const int RADAR_RX_PIN = 21;   // ESP32 receives here (radar TX) - header pin IO21
const int RADAR_TX_PIN = 1;    // ESP32 sends here (radar RX) - header pin IO1
#endif
const int RADAR_OT1_PIN = -1;  // optional: HIGH while a vehicle is approaching (e.g. 41). -1 = not used
const unsigned long RADAR_BAUD = 115200;

// Frame: F4 F3 F2 F1 | length (2 bytes, little-endian) | payload | F8 F7 F6 F5
const uint8_t HEADER[4] = {0xF4, 0xF3, 0xF2, 0xF1};
const uint8_t FOOTER[4] = {0xF8, 0xF7, 0xF6, 0xF5};
const size_t MAX_PAYLOAD = 64;

// Payload layout (from the community libraries, not the official manual):
//   [0] number of targets, [1] alarm (1 = approaching vehicle),
//   then per target 5 bytes: angle (raw - 0x80 = degrees), distance (m),
//   direction (1 = approaching, 0 = moving away), speed (km/h), SNR
const size_t TARGET_BYTES = 5;

uint8_t recent[32];          // last raw bytes from the radar, for debugging
unsigned long recentCount = 0;

uint8_t payload[MAX_PAYLOAD];
size_t payloadLen = 0;

unsigned long framesReceived = 0;
unsigned long bytesReceived = 0;
unsigned long lastByteMs = 0;
unsigned long lastStatusMs = 0;
int lastOt1 = -1;

enum State { FIND_HEADER, READ_LENGTH, READ_PAYLOAD, READ_FOOTER };
State state = FIND_HEADER;
size_t matched = 0;     // bytes of header/footer matched so far
size_t lengthBytes = 0;
uint16_t expectedLen = 0;

void printFrame() {
  framesReceived++;

  // Raw bytes first, so the layout can be checked against what is printed below
  Serial.printf("Frame %lu raw:", framesReceived);
  for (size_t i = 0; i < payloadLen; i++) Serial.printf(" %02X", payload[i]);
  Serial.println();

  if (payloadLen < 2) return;
  uint8_t count = payload[0];
  uint8_t alarm = payload[1];
  Serial.printf("  targets: %u, alarm: %s\n", count, alarm ? "APPROACHING" : "none");

  for (uint8_t i = 0; i < count; i++) {
    size_t at = 2 + i * TARGET_BYTES;
    if (at + TARGET_BYTES > payloadLen) {
      Serial.println("  (frame shorter than expected, layout may differ)");
      break;
    }
    int angle = (int)payload[at] - 0x80;
    Serial.printf("  target %u: angle %d deg, distance %u m, %s, speed %u km/h, SNR %u\n",
                  i + 1, angle, payload[at + 1],
                  payload[at + 2] ? "approaching" : "moving away",
                  payload[at + 3], payload[at + 4]);
  }
}

// One byte at a time through a small state machine
void feed(uint8_t b) {
  switch (state) {
    case FIND_HEADER:
      if (b == HEADER[matched]) {
        matched++;
        if (matched == sizeof(HEADER)) {
          state = READ_LENGTH;
          lengthBytes = 0;
          expectedLen = 0;
        }
      } else {
        matched = (b == HEADER[0]) ? 1 : 0;
      }
      break;

    case READ_LENGTH:
      expectedLen |= (uint16_t)b << (8 * lengthBytes);  // little-endian
      if (++lengthBytes == 2) {
        if (expectedLen > MAX_PAYLOAD) {
          Serial.printf("Bad frame length %u, resyncing\n", expectedLen);
          state = FIND_HEADER;
          matched = 0;
        } else if (expectedLen == 0) {
          // Empty frame (nothing detected): go straight to the footer
          payloadLen = 0;
          state = READ_FOOTER;
          matched = 0;
        } else {
          payloadLen = 0;
          state = READ_PAYLOAD;
        }
      }
      break;

    case READ_PAYLOAD:
      payload[payloadLen++] = b;
      if (payloadLen == expectedLen) {
        state = READ_FOOTER;
        matched = 0;
      }
      break;

    case READ_FOOTER:
      if (b == FOOTER[matched]) {
        if (++matched == sizeof(FOOTER)) {
          printFrame();
          state = FIND_HEADER;
          matched = 0;
        }
      } else {
        Serial.println("Bad footer, resyncing");
        state = FIND_HEADER;
        matched = (b == HEADER[0]) ? 1 : 0;
      }
      break;
  }
}

// ---- Radar settings -------------------------------------------------------------------
// Tuned for a rear-facing bike radar: see far, follow people and vehicles both while they
// come closer and after they pass, react to slow movement. The command words and value
// layouts below are from memory of the community libraries, not the official manual, so
// every command's reply is printed ("cfg ...") - check for OK.
const bool APPLY_RADAR_CONFIG = true;
const uint8_t CFG_MAX_DISTANCE_M = 100;  // furthest distance to report
const uint8_t CFG_DIRECTION = 2;         // 0 = moving away only, 1 = approaching only, 2 = both
const uint8_t CFG_MIN_SPEED_KMH = 1;     // slowest movement to report (people walking)
const uint8_t CFG_NO_TARGET_DELAY_S = 2; // how long a lost target stays reported
const uint8_t CFG_TRIGGER_COUNT = 1;     // frames in a row before a target is reported (lower = faster)
const uint8_t CFG_SNR_LEVEL = 4;         // lower = more sensitive (more false alarms)

const uint8_t CMD_HEADER[4] = {0xFD, 0xFC, 0xFB, 0xFA};
const uint8_t CMD_FOOTER[4] = {0x04, 0x03, 0x02, 0x01};

// Sends one command frame and waits for the radar's reply. Returns true if the reply
// carries the matching command word and a zero status. Reply data goes to `data`.
bool radarCommand(const char* name, uint16_t cmd, const uint8_t* value, uint16_t valueLen,
                  uint8_t* data = nullptr, size_t* dataLen = nullptr) {
  while (Serial1.available()) Serial1.read();
  uint16_t len = 2 + valueLen;
  Serial1.write(CMD_HEADER, 4);
  Serial1.write((uint8_t)(len & 0xFF));
  Serial1.write((uint8_t)(len >> 8));
  Serial1.write((uint8_t)(cmd & 0xFF));
  Serial1.write((uint8_t)(cmd >> 8));
  if (valueLen) Serial1.write(value, valueLen);
  Serial1.write(CMD_FOOTER, 4);

  uint8_t buf[64];
  size_t n = 0;
  unsigned long start = millis();
  while (millis() - start < 600 && n < sizeof(buf)) {
    while (Serial1.available() && n < sizeof(buf)) buf[n++] = Serial1.read();
    delay(1);
  }

  Serial.printf("cfg %s: reply", name);
  for (size_t i = 0; i < n; i++) Serial.printf(" %02X", buf[i]);
  Serial.println();

  for (size_t i = 0; i + 10 <= n; i++) {
    if (memcmp(buf + i, CMD_HEADER, 4) != 0) continue;
    uint16_t rlen = buf[i + 4] | (buf[i + 5] << 8);
    if (rlen < 4 || i + 6 + rlen + 4 > n) continue;
    uint16_t rcmd = buf[i + 6] | (buf[i + 7] << 8);
    uint16_t status = buf[i + 8] | (buf[i + 9] << 8);
    bool ok = (rcmd == (cmd | 0x0100)) && status == 0;
    if (ok && data && dataLen) {
      *dataLen = rlen - 4;
      memcpy(data, buf + i + 10, *dataLen);
    }
    Serial.printf("cfg %s: %s (command %04X, status %u)\n", name, ok ? "OK" : "FAILED", rcmd, status);
    return ok;
  }
  Serial.printf("cfg %s: no valid reply\n", name);
  return false;
}

void configureRadar() {
  Serial.println("Configuring radar...");
  const uint8_t enable[2] = {0x01, 0x00};
  if (!radarCommand("enable config", 0x00FF, enable, sizeof(enable))) {
    Serial.println("Radar did not accept configuration mode; leaving its settings as they are.");
    return;
  }
  const uint8_t detection[4] = {CFG_MAX_DISTANCE_M, CFG_DIRECTION, CFG_MIN_SPEED_KMH, CFG_NO_TARGET_DELAY_S};
  radarCommand("detection params", 0x0002, detection, sizeof(detection));
  const uint8_t sensitivity[4] = {CFG_TRIGGER_COUNT, CFG_SNR_LEVEL, 0x00, 0x00};
  radarCommand("sensitivity", 0x0003, sensitivity, sizeof(sensitivity));

  // Read both back so we can see what the radar actually stored
  uint8_t data[16];
  size_t dataLen = 0;
  if (radarCommand("read detection", 0x0012, nullptr, 0, data, &dataLen)) {
    Serial.printf("  detection now: max %u m, direction %u, min speed %u km/h, delay %u s\n",
                  data[0], data[1], data[2], data[3]);
  }
  if (radarCommand("read sensitivity", 0x0013, nullptr, 0, data, &dataLen)) {
    Serial.printf("  sensitivity now: trigger count %u, SNR level %u\n", data[0], data[1]);
  }
  radarCommand("end config", 0x00FE, nullptr, 0);
  while (Serial1.available()) Serial1.read();
  state = FIND_HEADER;
  matched = 0;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial1.begin(RADAR_BAUD, SERIAL_8N1, RADAR_RX_PIN, RADAR_TX_PIN);
  if (RADAR_OT1_PIN >= 0) pinMode(RADAR_OT1_PIN, INPUT);
  Serial.printf("LD2451 radar test - radar TX on GPIO%d, radar RX on GPIO%d\n",
                RADAR_RX_PIN, RADAR_TX_PIN);
  if (APPLY_RADAR_CONFIG) configureRadar();
  Serial.println("Move something toward the radar (a person walking or a car)...");
}

void loop() {
  while (Serial1.available()) {
    lastByteMs = millis();
    bytesReceived++;
    uint8_t b = (uint8_t)Serial1.read();
    recent[recentCount++ % sizeof(recent)] = b;
    feed(b);
  }

  // Report when the OT1 output changes
  if (RADAR_OT1_PIN >= 0) {
    int ot1 = digitalRead(RADAR_OT1_PIN);
    if (ot1 != lastOt1) {
      lastOt1 = ot1;
      Serial.printf("OT1 = %s\n", ot1 ? "HIGH (approaching target)" : "LOW");
    }
  }

  // Status line once a second, so you can see the program is running
  if (millis() - lastStatusMs >= 1000) {
    lastStatusMs = millis();
    bool heardRecently = bytesReceived > 0 && millis() - lastByteMs < 2000;
    Serial.printf("[%lus] bytes: %lu, frames: %lu, OT1: %s -> %s\n",
                  millis() / 1000, bytesReceived, framesReceived,
                  RADAR_OT1_PIN < 0 ? "n/a" : (digitalRead(RADAR_OT1_PIN) ? "HIGH" : "LOW"),
                  heardRecently ? "radar is talking"
                                : "no data from radar (check 5V, GND, TX/RX crossed)");
    if (recentCount > 0) {
      Serial.print("  last bytes:");
      size_t n = recentCount < sizeof(recent) ? recentCount : sizeof(recent);
      for (size_t i = 0; i < n; i++) {
        Serial.printf(" %02X", recent[(recentCount - n + i) % sizeof(recent)]);
      }
      Serial.println();
    }
  }
}
