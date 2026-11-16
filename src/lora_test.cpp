// Ebyte E32-915T20D LoRa test, used by both ends of the link:
//   env:lora_test       - EXTERNAL unit on the Freenove ESP32-S3 (sends "EXT ping N")
//   env:lora_bike_test  - BIKE unit on the Waveshare ESP32-S3-SIM7670G (sends "BIKE ping N")
//
// 1) Reads the module's settings to prove the wiring works (needs only one module).
// 2) Sends "<name> ping N" every few seconds and prints anything it receives.
// 3) When it hears the other unit's ping, it replies "<name> ack ..." so each side can
//    confirm the link works in both directions.
// 4) Anything you type in the Serial Monitor is sent over LoRa too.
//
// Wiring (E32 -> ESP32):  VCC -> 3V3 or 5V,  GND -> GND,
//   TXD -> LORA_RX_PIN,  RXD -> LORA_TX_PIN,
//   M0 -> LORA_M0_PIN,   M1 -> LORA_M1_PIN,  AUX -> LORA_AUX_PIN
// Attach the antenna BEFORE powering the module - transmitting without one can damage it.
#include <Arduino.h>

#ifdef LORA_BIKE
// Bike unit: Waveshare ESP32-S3-SIM7670G (env:lora_bike_test)
const char* MY_NAME = "BIKE";
const char* OTHER_NAME = "EXT";
// GPIO7/8 are also camera data lines (Y2/Y3): the camera can't run while LoRa uses them.
const int LORA_RX_PIN = 7;    // ESP32 receives here (E32 TXD)
const int LORA_TX_PIN = 8;    // ESP32 sends here (E32 RXD)
const int LORA_M0_PIN = -1;   // not wired to the ESP32: tie M0 and M1 to GND by hand
const int LORA_M1_PIN = -1;
const int LORA_AUX_PIN = -1;  // -1 if AUX is not wired
const unsigned long SEND_INTERVAL_MS = 3500;
#elif defined(LORA_ESP32_WROOM)
// External unit: classic ESP32 WROOM dev board (env:lora_esp32_wroom)
const char* MY_NAME = "EXT";
const char* OTHER_NAME = "BIKE";
const int LORA_RX_PIN = 16;   // ESP32 receives here (E32 TXD)
const int LORA_TX_PIN = 17;   // ESP32 sends here (E32 RXD)
const int LORA_M0_PIN = 25;
const int LORA_M1_PIN = 26;
const int LORA_AUX_PIN = 27;  // -1 if AUX is not wired
const unsigned long SEND_INTERVAL_MS = 3000;
#else
// External unit: Freenove ESP32-S3 (env:lora_test)
const char* MY_NAME = "EXT";
const char* OTHER_NAME = "BIKE";
const int LORA_RX_PIN = 18;   // ESP32 receives here (E32 TXD)
const int LORA_TX_PIN = 17;   // ESP32 sends here (E32 RXD)
const int LORA_M0_PIN = 47;   // -1 = not wired to the ESP32 (tie M0 and M1 to GND by hand)
const int LORA_M1_PIN = 48;   // GPIO48 also drives the board's onboard RGB LED
const int LORA_AUX_PIN = -1;  // -1 if AUX is not wired
const unsigned long SEND_INTERVAL_MS = 3000;
#endif

const unsigned long CONFIG_BAUD = 9600;  // config mode always uses 9600 8N1
const unsigned long RX_IDLE_MS = 100;    // a message ends after this much silence

const unsigned long BAUD_TABLE[] = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
const float AIR_RATE_KBPS[] = {0.3, 1.2, 2.4, 4.8, 9.6, 19.2, 38.4, 62.5};
const int POWER_DBM[] = {20, 17, 14, 10};

unsigned long loraBaud = CONFIG_BAUD;
unsigned long lastSendMs = 0;
unsigned long pingCount = 0;
unsigned long receivedCount = 0;

String rxBuffer;
unsigned long lastRxByteMs = 0;
String typedLine;

// AUX is LOW while the module is busy and HIGH when it is ready
bool waitAuxReady(unsigned long timeoutMs) {
  if (LORA_AUX_PIN < 0) {
    delay(100);
    return true;
  }
  unsigned long start = millis();
  while (digitalRead(LORA_AUX_PIN) == LOW) {
    if (millis() - start > timeoutMs) return false;
    delay(1);
  }
  delay(5);
  return true;
}

// M0 M1: 0 0 = normal, 1 1 = sleep / settings
void setMode(int m0, int m1) {
  if (LORA_M0_PIN >= 0) digitalWrite(LORA_M0_PIN, m0);
  if (LORA_M1_PIN >= 0) digitalWrite(LORA_M1_PIN, m1);
  delay(50);
  waitAuxReady(1000);
}

void printSettings(const uint8_t* r) {
  uint8_t sped = r[3], chan = r[4], option = r[5];
  Serial.printf("  raw: %02X %02X %02X %02X %02X %02X\n", r[0], r[1], r[2], r[3], r[4], r[5]);
  Serial.printf("  settings %s\n", r[0] == 0xC0 ? "(saved)" : "(not saved)");
  Serial.printf("  address: 0x%02X%02X\n", r[1], r[2]);
  Serial.printf("  UART speed: %lu baud\n", BAUD_TABLE[(sped >> 3) & 0x07]);
  Serial.printf("  air data rate: %.1f kbps\n", AIR_RATE_KBPS[sped & 0x07]);
  Serial.printf("  channel: %u (915 MHz model: about %u MHz)\n", chan & 0x1F, 900 + (chan & 0x1F));
  Serial.printf("  transmit power: %d dBm\n", POWER_DBM[option & 0x03]);
  Serial.printf("  mode: %s\n", (option & 0x80) ? "fixed" : "transparent");
}

// Reads the module's settings with the C1 C1 C1 command. Returns true on success.
bool readSettings() {
  if (LORA_M0_PIN < 0 || LORA_M1_PIN < 0) {
    Serial.println("M0/M1 not wired to the ESP32: skipping settings read, module stays in normal mode.");
    return false;
  }
  setMode(1, 1);  // settings mode
  Serial1.updateBaudRate(CONFIG_BAUD);
  while (Serial1.available()) Serial1.read();

  const uint8_t cmd[3] = {0xC1, 0xC1, 0xC1};
  Serial1.write(cmd, sizeof(cmd));

  uint8_t reply[6];
  size_t got = 0;
  unsigned long start = millis();
  while (got < sizeof(reply) && millis() - start < 1000) {
    if (Serial1.available()) reply[got++] = Serial1.read();
  }

  bool ok = (got == sizeof(reply) && (reply[0] == 0xC0 || reply[0] == 0xC2));
  if (ok) {
    Serial.println("E32 module answered:");
    printSettings(reply);
    loraBaud = BAUD_TABLE[(reply[3] >> 3) & 0x07];
  } else {
    Serial.printf("No valid answer from the E32 module (got %u bytes).\n", (unsigned)got);
    Serial.println("Check VCC/GND, TXD/RXD crossed, M0/M1 wires, and that the antenna is attached.");
  }

  setMode(0, 0);  // back to normal (send/receive) mode
  Serial1.updateBaudRate(loraBaud);
  return ok;
}

void sendMessage(const String& text) {
  if (!waitAuxReady(1000)) {
    Serial.println("TX skipped: module busy (AUX stayed low)");
    return;
  }
  Serial1.print(text);
  Serial1.print('\n');
  Serial.printf("TX: %s\n", text.c_str());
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("E32-915T20D LoRa test");

  if (LORA_M0_PIN >= 0) pinMode(LORA_M0_PIN, OUTPUT);
  if (LORA_M1_PIN >= 0) pinMode(LORA_M1_PIN, OUTPUT);
  if (LORA_AUX_PIN >= 0) pinMode(LORA_AUX_PIN, INPUT);
  Serial1.begin(CONFIG_BAUD, SERIAL_8N1, LORA_RX_PIN, LORA_TX_PIN);

  readSettings();
  Serial.println("Sending a ping every 3 s. Type text and press Enter to send it too.");
}

void loop() {
  // Messages from the other radio, ending after a short silence
  while (Serial1.available()) {
    rxBuffer += (char)Serial1.read();
    lastRxByteMs = millis();
  }
  if (rxBuffer.length() > 0 && millis() - lastRxByteMs > RX_IDLE_MS) {
    rxBuffer.trim();
    receivedCount++;
    Serial.printf("RX #%lu: %s\n", receivedCount, rxBuffer.c_str());

    // Acknowledge the other unit's pings so it can see the link works both ways.
    // Only pings are acknowledged, so two units never ack each other's acks.
    if (rxBuffer.startsWith(String(OTHER_NAME) + " ping")) {
      sendMessage(String(MY_NAME) + " ack " + rxBuffer);
    }
    rxBuffer = "";
  }

  // Lines typed in the Serial Monitor
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (typedLine.length() > 0) sendMessage(typedLine);
      typedLine = "";
    } else {
      typedLine += c;
    }
  }

  // Automatic ping
  if (millis() - lastSendMs >= SEND_INTERVAL_MS) {
    lastSendMs = millis();
    pingCount++;
    sendMessage(String(MY_NAME) + " ping " + String(pingCount));
    if (receivedCount == 0 && pingCount % 4 == 0) {
      Serial.printf("(nothing received yet - waiting for the %s unit)\n", OTHER_NAME);
    }
  }
}
