// Radar node: HLK-LD2451 radar -> Ebyte E32-915T20D LoRa, on a small ESP32 WROOM board
// (env:radar_node). Reads the radar's UART frames and sends a short text line over LoRa
// whenever a vehicle is seen (and a heartbeat every few seconds so the receiver knows
// the node is alive). The line looks like:   R,<targets>,<approaching 0/1>,<m>,<km/h>
// where <m> and <km/h> belong to the closest target (0,0 when there are none).
//
// Wiring:
//   Radar:  VIN -> 5V, GND -> GND, TX -> RADAR_RX_PIN, RX -> RADAR_TX_PIN
//   E32:    VCC -> 3V3, GND -> GND, TXD -> LORA_RX_PIN, RXD -> LORA_TX_PIN,
//           M0 -> LORA_M0_PIN, M1 -> LORA_M1_PIN, AUX not used
// Attach the E32 antenna BEFORE powering the module.
#include <Arduino.h>

const int RADAR_RX_PIN = 16;   // ESP32 receives here (radar TX)
const int RADAR_TX_PIN = 17;   // ESP32 sends here (radar RX)
const int LORA_RX_PIN = 18;    // ESP32 receives here (E32 TXD)
const int LORA_TX_PIN = 19;    // ESP32 sends here (E32 RXD)
const int LORA_M0_PIN = 25;
const int LORA_M1_PIN = 26;

const unsigned long RADAR_BAUD = 115200;
const unsigned long LORA_BAUD = 9600;          // module default
const unsigned long SEND_MIN_GAP_MS = 500;     // at most 2 messages a second while tracking
const unsigned long HEARTBEAT_MS = 5000;

HardwareSerial& radarSerial = Serial2;
HardwareSerial& loraSerial = Serial1;

// Frame: F4 F3 F2 F1 | length (2 bytes, little-endian) | payload | F8 F7 F6 F5
const uint8_t HEADER[4] = {0xF4, 0xF3, 0xF2, 0xF1};
const uint8_t FOOTER[4] = {0xF8, 0xF7, 0xF6, 0xF5};
const size_t MAX_PAYLOAD = 64;

// Payload layout (same as radar_test.cpp): [0] targets, [1] alarm,
// then per target 5 bytes: angle, distance (m), direction (1 = approaching), speed (km/h), SNR
const size_t TARGET_BYTES = 5;

uint8_t payload[MAX_PAYLOAD];
size_t payloadLen = 0;

enum State { FIND_HEADER, READ_LENGTH, READ_PAYLOAD, READ_FOOTER };
State state = FIND_HEADER;
size_t matched = 0;
size_t lengthBytes = 0;
uint16_t expectedLen = 0;

unsigned long framesReceived = 0;
unsigned long lastSendMs = 0;

void sendLora(const String& line) {
  loraSerial.print(line);
  loraSerial.print('\n');
  Serial.printf("LoRa TX: %s\n", line.c_str());
  lastSendMs = millis();
}

void handleFrame() {
  framesReceived++;
  if (payloadLen < 2) return;
  uint8_t count = payload[0];
  uint8_t alarm = payload[1];

  // Closest target
  uint8_t bestDist = 0, bestSpeed = 0;
  bool haveBest = false;
  for (uint8_t i = 0; i < count; i++) {
    size_t at = 2 + i * TARGET_BYTES;
    if (at + TARGET_BYTES > payloadLen) break;
    uint8_t dist = payload[at + 1];
    if (!haveBest || dist < bestDist) {
      bestDist = dist;
      bestSpeed = payload[at + 3];
      haveBest = true;
    }
  }

  bool tracking = count > 0;
  if (tracking && millis() - lastSendMs >= SEND_MIN_GAP_MS) {
    sendLora("R," + String(count) + "," + String(alarm ? 1 : 0) + "," + String(bestDist) + "," +
             String(bestSpeed));
  }
}

void feed(uint8_t b) {
  switch (state) {
    case FIND_HEADER:
      if (b == HEADER[matched]) {
        if (++matched == sizeof(HEADER)) {
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
        if (expectedLen == 0 || expectedLen > MAX_PAYLOAD) {
          state = FIND_HEADER;
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
          handleFrame();
          state = FIND_HEADER;
          matched = 0;
        }
      } else {
        state = FIND_HEADER;
        matched = (b == HEADER[0]) ? 1 : 0;
      }
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("Radar node: LD2451 radar -> E32 LoRa");

  // M0 = M1 = LOW is normal send/receive mode
  pinMode(LORA_M0_PIN, OUTPUT);
  pinMode(LORA_M1_PIN, OUTPUT);
  digitalWrite(LORA_M0_PIN, LOW);
  digitalWrite(LORA_M1_PIN, LOW);

  radarSerial.begin(RADAR_BAUD, SERIAL_8N1, RADAR_RX_PIN, RADAR_TX_PIN);
  loraSerial.begin(LORA_BAUD, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN);
  delay(200);  // let the E32 settle in normal mode
  sendLora("R,boot");
}

void loop() {
  while (radarSerial.available()) feed((uint8_t)radarSerial.read());

  if (millis() - lastSendMs >= HEARTBEAT_MS) {
    sendLora("R,alive," + String(framesReceived));
  }
}
