<div align="center">

# 💖✨ HEY KATTY — READ ME FIRST ✨💖

### 🎀 Bike Project Status Update 🎀

**Last updated: Nov 16, 2026** 🗓️

![radar](https://img.shields.io/badge/radar-cooked%20🔥-ff1493)
![imu](https://img.shields.io/badge/IMU-needs%20a%20look%20👀-ff69b4)
![back lights](https://img.shields.io/badge/back%20lights-needs%20a%20look%20👀-ff69b4)
![lora](https://img.shields.io/badge/LoRa-half%20tested%20🌗-db7093)
![board](https://img.shields.io/badge/Waveshare-uploads%20work%20💅-c71585)

</div>

---

## 🌸 TL;DR (read just this if your brain is full)

| | What | Status |
|---|---|---|
| 🔴 | **Radar** | **Cooked.** Zero data on the Waveshare |
| 🟡 | **IMU** | Not connected properly — needs to be looked back at |
| 🟡 | **Back lights** | Need to be looked at (not tested, no code yet) |
| 🟡 | **LoRa** | Program runs, but the radio link isn't proven yet |
| ⚪ | **Front lights, buttons, speaker** | Soldered, **not tested** on the new wiring yet |
| 🟢 | **Waveshare board** | Alive! Uploads work on **COM9** |

> 💡 **Next tiny step:** check the radar's 5V and GND with a multimeter. That's it. Just that one thing. 💕

---

## 🔥 Radar — COOKED

**What happened:**
- 📭 Sent **0 bytes** on the Waveshare (radar TX → GPIO21, RX → GPIO1)
- 🔁 Swapped TX/RX in code → still **0 bytes**
- 🙅‍♀️ Didn't answer the setup command either

**What we know:**
- ✅ It **worked before** on the Freenove (radar TX → GPIO16, RX → GPIO17) and accepted the tuning settings

**To check (one at a time 🫶):**
- [ ] Is there **5V** between radar VIN and GND? (multimeter!)
- [ ] Is radar **GND** joined to the Waveshare **GND**?
- [ ] Are the wires on the pins labeled **IO21** and **IO1**?
- [ ] Plug it back into the **Freenove** (`radar_test_freenove`) → if it's silent there too, it's truly dead 💀 and needs replacing

---

## 👀 IMU (MPU-6050) — not connected properly

**What happened:**
- 🔍 The I2C scan **sees it** at address `0x68`...
- ❌ ...but **every read fails** (timeout)
- 🐢 Slowing the bus down didn't help
- 👻 A mystery address `0x51` showed up once, then vanished (noise?)

**To check:**
- [ ] Re-flow the **SDA (GPIO15)** and **SCL (GPIO16)** solder joints, both ends
- [ ] IMU **GND** joined to the Waveshare **GND**?
- [ ] IMU actually **powered** right now?
- [ ] Wires to the back rack are **long** → try it with **short wires** on the desk first
- [ ] ⚠️ If VCC is on **5V**: measure SDA/SCL — must read **~3.3V**, not 5V (5V can hurt the ESP32)

---

## 👀 Back lights — need to be looked at

- 🚫 **No code yet** for rear blinkers, brake lights, or the rear on/off
- 🧪 Wiring **not tested**
- 🔗 All 3 LED controller boards are tied to **GPIO45** → front and rear switch on/off **together** (a rear-only "day mode" would need a physical switch or a free pin)
- 🛑 Brake light idea: turns on when the IMU feels the bike **slowing down** or **stopped** → needs the IMU working first!

---

## 🌗 LoRa (E32-915T20D)

- ✅ **Freenove unit:** module answered and reported its settings (address 0, channel 6 ≈ 906 MHz, 9600 baud, 20 dBm)
- 🟡 **Bike unit (Waveshare):** program runs and sends `BIKE ping` every 3 s — but nobody was listening, so the radio link is **unproven**
- [ ] Run **both** units at once (`lora_test` on Freenove + `lora_bike_test` on Waveshare) and look for pings + acks
- 🔁 Silent? → swap the E32's TXD/RXD wires (that fixed it on the Freenove!)
- 📡 **Always attach the antenna before powering it**

---

## 📍 Pin Map — Waveshare (the bike)

| 🎀 Part | Signal | GPIO |
|---|---|---|
| 🔘 Buttons | left | **2** |
| | right (moved from 12) | **41** |
| | shared ground | **40** |
| 💡 Front lights | left ring data | **3** |
| | right ring data | **42** |
| 🔌 All 3 LED boards | trigger (tied together) | **45** |
| 🔊 Speaker board | trigger | **39** |
| 🌙 Back lights | rear-left blinker data | **4** |
| | rear-right blinker data | **5** |
| | brake lights data | **6** |
| 📡 Radar | radar TX → | **21** |
| | radar RX → | **1** |
| 📶 LoRa | E32 TXD → | **7** |
| | E32 RXD → | **8** |
| | M0, M1 | **GND** |
| 🧭 IMU | SDA / SCL / INT | **15 / 16 / 46** |

### 🚫 Don't use these pins

| GPIO | Why |
|---|---|
| 17, 18 (RX1/TX1) | 4G modem |
| 38 | onboard RGB LED |
| 43, 44 (TXD/RXD) | USB-serial (uploading!) |
| 0 | BOOT button |
| 47, 48 | not on our header |

### ⚠️ Heads up

- 📷 **Camera clash:** the camera uses GPIO **7–14** → it **can't run while LoRa uses 7/8**
- 💾 **SD card clash:** the TF card uses GPIO **4, 5, 6** → don't use the SD card (those are the back lights)
- 🥾 **Boot pins:** GPIO **3, 45, 46** — if the board won't start, unplug those wires first

---

## ⚡ Power Rules (please don't skip 🙏)

1. 🔧 **Set the 5V converter to 5.0V** with a multimeter **before** plugging anything in
2. ☝️ **One 5V source at a time** — unplug USB when running on the battery, or turn the battery off when uploading
3. 🔗 **Every GND joined together** (Waveshare, all boards, radar, LoRa, IMU)
4. 🔋 **Don't** put a cell in the Waveshare's own 18650 holder while using the converter
5. ✂️ **Soldered + heat-shrunk** splices on the power wire — no twisted joints
6. 📏 Thin wire + long runs = dim/flickery lights → thicker wire for the main power line

---

## 🛠️ How to Test Stuff

Pick the env in PlatformIO's bottom bar, then **Upload**. Close the **Arduino IDE** and any **Serial Monitor** first (they hog the port!).

| 🧪 Test | Env | Board / Port |
|---|---|---|
| Is the board alive? | `hello_waveshare` | Waveshare / COM9 |
| Find I2C devices | `i2c_scan` | Waveshare / COM9 |
| IMU | `imu_test` | Waveshare / COM9 |
| Radar | `radar_test` | Waveshare / COM9 |
| Radar (backup board) | `radar_test_freenove` | Freenove / COM8 |
| LoRa bike unit | `lora_bike_test` | Waveshare / COM9 |
| LoRa other unit | `lora_test` | Freenove / COM8 |
| Lights + buttons + speaker | `waveshare_esp32s3_sim7670g` | Waveshare / COM9 |

🖥️ **Radar screen:** open `radar_display.html` in **Chrome or Edge** → **Connect radar** → pick the port.

> 🪄 COM numbers can change when you switch USB ports — check **Device Manager** if an upload fails.

---

## ✅ To-Do (biggest first)

- [ ] 🔥 Find out if the radar is dead or just unpowered
- [ ] 🧭 Fix the IMU connection
- [ ] 🌙 Test the back lights wiring
- [ ] 💡 Test front lights, buttons + speaker on the new wiring
- [ ] 📶 Two-unit LoRa test
- [ ] 💻 Write the **combined program** (lights + brake + radar + LoRa), one piece at a time
- [ ] 🛑 Brake light logic from the IMU (needs riding tests to tune)
- [ ] 📷 Decide: camera **or** LoRa on 7/8?

## 🛍️ Shopping List

- [ ] 📡 SMA male → SMA female **bulkhead** cable, ~5 ft, **not RP-SMA** (antenna out of the metal box)
- [ ] 📷 Taidacent OV2640, **200 mm** cable, 24-pin
- [ ] 🧩 Small board for the radar node: Type-C D1 Mini ESP32 (code ready: `radar_node`)
- [ ] 🔌 Lever nuts or a terminal block for the power wire (optional but nice)

---

<div align="center">

### 💕 You've got this, Katty! One tiny step at a time. 💕

🌸 *Update this file whenever something changes* 🌸

</div>
