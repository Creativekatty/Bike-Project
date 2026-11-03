// MPU-6050 (GY-521) IMU test (build with env:imu_test)
// Prints acceleration, rotation and temperature twice a second, and counts
// "data ready" interrupts on the INT pin to check that wire.
#include <Arduino.h>
#include <Wire.h>

const int IMU_SDA_PIN = 15;
const int IMU_SCL_PIN = 16;
const int IMU_INT_PIN = 46;
const uint8_t IMU_ADDR = 0x68;  // 0x69 if the AD0 pin is tied high

// MPU-6050 registers
const uint8_t REG_SMPLRT_DIV = 0x19;
const uint8_t REG_CONFIG = 0x1A;
const uint8_t REG_GYRO_CONFIG = 0x1B;
const uint8_t REG_ACCEL_CONFIG = 0x1C;
const uint8_t REG_INT_PIN_CFG = 0x37;
const uint8_t REG_INT_ENABLE = 0x38;
const uint8_t REG_ACCEL_XOUT_H = 0x3B;
const uint8_t REG_PWR_MGMT_1 = 0x6B;
const uint8_t REG_WHO_AM_I = 0x75;

const float ACCEL_LSB_PER_G = 16384.0;  // +/-2 g range
const float GYRO_LSB_PER_DPS = 131.0;   // +/-250 deg/s range

volatile uint32_t interruptCount = 0;

void IRAM_ATTR onImuInterrupt() {
  interruptCount++;
}

void writeReg(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

uint8_t readReg(uint8_t reg) {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(IMU_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0xFF;
}

int16_t read16() {
  int16_t high = Wire.read();
  return (high << 8) | Wire.read();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("MPU-6050 test");

  Wire.begin(IMU_SDA_PIN, IMU_SCL_PIN);

  uint8_t whoAmI = readReg(REG_WHO_AM_I);
  if (whoAmI != 0x68) {
    Serial.printf("IMU not found (WHO_AM_I = 0x%02X, expected 0x68). Check SDA/SCL/power.\n", whoAmI);
    while (true) delay(1000);
  }
  Serial.println("IMU found");

  writeReg(REG_PWR_MGMT_1, 0x01);     // wake up, use gyro clock
  writeReg(REG_CONFIG, 0x03);         // 44 Hz low-pass filter, 1 kHz internal rate
  writeReg(REG_SMPLRT_DIV, 99);       // 1 kHz / (1 + 99) = 10 samples per second
  writeReg(REG_GYRO_CONFIG, 0x00);    // +/-250 deg/s
  writeReg(REG_ACCEL_CONFIG, 0x00);   // +/-2 g
  writeReg(REG_INT_PIN_CFG, 0x00);    // INT active-high push-pull (keeps GPIO46 low at boot)
  writeReg(REG_INT_ENABLE, 0x01);     // interrupt on every new sample

  pinMode(IMU_INT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(IMU_INT_PIN), onImuInterrupt, RISING);
}

void loop() {
  Wire.beginTransmission(IMU_ADDR);
  Wire.write(REG_ACCEL_XOUT_H);
  Wire.endTransmission(false);
  Wire.requestFrom(IMU_ADDR, (uint8_t)14);
  if (Wire.available() < 14) {
    Serial.println("Read failed");
    delay(500);
    return;
  }

  float ax = read16() / ACCEL_LSB_PER_G;
  float ay = read16() / ACCEL_LSB_PER_G;
  float az = read16() / ACCEL_LSB_PER_G;
  float tempC = read16() / 340.0 + 36.53;
  float gx = read16() / GYRO_LSB_PER_DPS;
  float gy = read16() / GYRO_LSB_PER_DPS;
  float gz = read16() / GYRO_LSB_PER_DPS;

  Serial.printf("Accel g: X %6.2f  Y %6.2f  Z %6.2f | Gyro deg/s: X %7.1f  Y %7.1f  Z %7.1f | %.1f C | INT count %lu\n",
                ax, ay, az, gx, gy, gz, tempC, (unsigned long)interruptCount);
  delay(500);
}
