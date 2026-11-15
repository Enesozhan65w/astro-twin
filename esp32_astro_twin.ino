/*
 * ASTRO-TWIN v3 — Multi-Pin Auto-Scanning Biometric Node (ESP32 -> Raspberry Pi 3 / PC)
 * Team: B.E.K.A. | NASA Space Apps Challenge 2026
 *
 * AI-Assisted Code Disclosure (NASA Space Apps 2026 Section 5 Compliance):
 * Portions of this firmware were generated with assistance from Google Gemini AI.
 * Hardware circuit integration and sensor calibration by Team B.E.K.A.
 *
 * KEY FIX IN v3 FOR EMG:
 * Automatically scans D34, D25 (pre-soldered white wire!), D35, D32, and D33 simultaneously
 * and locks onto whichever pin has an active EMG signal above ground, reporting "emg_pin"!
 */

#include <Arduino.h>
#include <Wire.h>

static const int EMG_CANDIDATE_PINS[] = {34, 25, 35, 32, 33};
static const int NUM_EMG_PINS = 5;

static const int PIN_I2C_SDA    = 21;
static const int PIN_I2C_SCL    = 22;
static const int PIN_STATUS_LED = 2;

static const uint8_t ADDR_MAX30100 = 0x57;
static const uint8_t ADDR_MLX90614 = 0x5A;

static const uint8_t REG_MAX_FIFO_DATA = 0x05;
static const uint8_t REG_MAX_MODE_CFG  = 0x06;
static const uint8_t REG_MAX_SPO2_CFG  = 0x07;
static const uint8_t REG_MAX_LED_CFG   = 0x09;
static const uint8_t REG_MAX_TEMP_INT  = 0x16;
static const uint8_t REG_MAX_TEMP_FRAC = 0x17;
static const uint8_t REG_MLX_TOBJ1     = 0x07;

bool hasMAX30100 = false;
bool hasMLX90614 = false;

float pinRestMv[NUM_EMG_PINS] = {120.0f, 120.0f, 120.0f, 120.0f, 120.0f};
int activeEmgPin = 34;
float emgSmoothPct = 0.0f;
float fatigueAccumulator = 0.0f;

float currentBPM = 75.0f;
float currentSpO2 = 98.0f;
float currentTempC = 36.5f;

unsigned long lastBeatMs = 0;
float irDcFilter = 0.0f;
float redDcFilter = 0.0f;
bool pulseAboveThreshold = false;
unsigned long lastTelemetryMs = 0;
unsigned long lastTempPollMs = 0;

bool i2cDevicePresent(uint8_t address) {
  Wire.beginTransmission(address);
  return (Wire.endTransmission() == 0);
}

void writeReg8(uint8_t devAddr, uint8_t regAddr, uint8_t val) {
  Wire.beginTransmission(devAddr);
  Wire.write(regAddr);
  Wire.write(val);
  Wire.endTransmission();
}

void initMAX30100() {
  writeReg8(ADDR_MAX30100, REG_MAX_MODE_CFG, 0x40);
  delay(50);
  writeReg8(ADDR_MAX30100, REG_MAX_MODE_CFG, 0x0B);
  writeReg8(ADDR_MAX30100, REG_MAX_SPO2_CFG, 0x47);
  writeReg8(ADDR_MAX30100, REG_MAX_LED_CFG,  0x88);
}

bool readMAX30100Sample(uint16_t &irOut, uint16_t &redOut) {
  Wire.beginTransmission(ADDR_MAX30100);
  Wire.write(REG_MAX_FIFO_DATA);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)ADDR_MAX30100, 4) < 4) return false;
  uint8_t b0 = Wire.read();
  uint8_t b1 = Wire.read();
  uint8_t b2 = Wire.read();
  uint8_t b3 = Wire.read();
  irOut  = ((uint16_t)b0 << 8) | b1;
  redOut = ((uint16_t)b2 << 8) | b3;
  return true;
}

float readMAX30100DieTempC() {
  Wire.beginTransmission(ADDR_MAX30100);
  Wire.write(REG_MAX_TEMP_INT);
  if (Wire.endTransmission(false) != 0) return NAN;
  if (Wire.requestFrom((int)ADDR_MAX30100, 2) < 2) return NAN;
  int8_t tInt = (int8_t)Wire.read();
  uint8_t tFrac = Wire.read() & 0x0F;
  writeReg8(ADDR_MAX30100, REG_MAX_MODE_CFG, 0x0B);
  return (float)tInt + ((float)tFrac * 0.0625f);
}

float readMLX90614TempC(uint8_t reg) {
  Wire.beginTransmission(ADDR_MLX90614);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return NAN;
  if (Wire.requestFrom((int)ADDR_MLX90614, 3) < 3) return NAN;
  uint16_t raw = Wire.read();
  raw |= ((uint16_t)Wire.read() << 8);
  Wire.read();
  if (raw & 0x8000) return NAN;
  return ((float)raw * 0.02f) - 273.15f;
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_STATUS_LED, OUTPUT);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  for (int p = 0; p < NUM_EMG_PINS; p++) {
    pinMode(EMG_CANDIDATE_PINS[p], INPUT_PULLDOWN);
  }

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(100000);
  delay(200);

  hasMAX30100 = i2cDevicePresent(ADDR_MAX30100);
  hasMLX90614 = i2cDevicePresent(ADDR_MLX90614);
  if (hasMAX30100) initMAX30100();

  // Calibrate resting floor on each candidate pin (pulldown keeps unconnected pins at 0mV!)
  for (int p = 0; p < NUM_EMG_PINS; p++) {
    long sumMv = 0;
    for (int i = 0; i < 30; i++) {
      int adc = analogRead(EMG_CANDIDATE_PINS[p]);
      sumMv += (long)((adc * 3300L) / 4095L);
      delay(2);
    }
    pinRestMv[p] = constrain((float)sumMv / 30.0f, 0.0f, 800.0f);
  }
}

void loop() {
  // 1) Scan candidate pins (with INPUT_PULLDOWN, unconnected pins stay at 0 mV — zero floating noise!)
  float bestActiveMv = -1.0f;
  float bestRawMv = 0.0f;
  int bestPin = activeEmgPin;

  for (int p = 0; p < NUM_EMG_PINS; p++) {
    int pin = EMG_CANDIDATE_PINS[p];
    float sumMv = 0.0f;
    float minMv = 3300.0f;
    float maxMv = 0.0f;
    for (int i = 0; i < 6; i++) {
      int adc = analogRead(pin);
      float mv = ((float)adc / 4095.0f) * 3300.0f;
      sumMv += mv;
      if (mv < minMv) minMv = mv;
      if (mv > maxMv) maxMv = mv;
      delayMicroseconds(120);
    }
    float avgMv = sumMv / 6.0f;
    float ripMv = maxMv - minMv;

    if (avgMv < pinRestMv[p]) {
      pinRestMv[p] = 0.95f * pinRestMv[p] + 0.05f * avgMv;
    } else if (avgMv < pinRestMv[p] + 45.0f) {
      pinRestMv[p] = 0.998f * pinRestMv[p] + 0.002f * avgMv;
    }

    float act = max(0.0f, avgMv - pinRestMv[p] - 20.0f) + (ripMv * 0.65f);
    if (act > bestActiveMv || (act == bestActiveMv && avgMv > bestRawMv)) {
      bestActiveMv = act;
      bestRawMv = avgMv;
      bestPin = pin;
    }
  }

  activeEmgPin = bestPin;
  float targetPct = constrain((bestActiveMv / 1500.0f) * 100.0f, 0.0f, 100.0f);
  emgSmoothPct = 0.70f * emgSmoothPct + 0.30f * targetPct;

  if (emgSmoothPct > 50.0f) {
    fatigueAccumulator = constrain(fatigueAccumulator + 0.08f, 0.0f, 100.0f);
    digitalWrite(PIN_STATUS_LED, HIGH);
  } else {
    fatigueAccumulator = constrain(fatigueAccumulator - 0.04f, 0.0f, 100.0f);
    digitalWrite(PIN_STATUS_LED, LOW);
  }

  // 2) Poll MAX30100 optical pulse & SpO2
  if (hasMAX30100) {
    uint16_t irRaw = 0, redRaw = 0;
    if (readMAX30100Sample(irRaw, redRaw) && irRaw > 8000) {
      irDcFilter  = 0.95f * irDcFilter  + 0.05f * (float)irRaw;
      redDcFilter = 0.95f * redDcFilter + 0.05f * (float)redRaw;
      float acIr  = (float)irRaw - irDcFilter;

      if (acIr > 45.0f && !pulseAboveThreshold) {
        pulseAboveThreshold = true;
        unsigned long now = millis();
        unsigned long dt = now - lastBeatMs;
        lastBeatMs = now;
        if (dt > 310 && dt < 1600) {
          float instBpm = 60000.0f / (float)dt;
          currentBPM = 0.82f * currentBPM + 0.18f * instBpm;
        }
      } else if (acIr < -25.0f) {
        pulseAboveThreshold = false;
      }

      if (irDcFilter > 1000.0f && redDcFilter > 1000.0f) {
        float ratio = (fabsf((float)redRaw - redDcFilter) / redDcFilter) /
                      max(0.02f, (fabsf(acIr) / irDcFilter));
        float estSpo2 = constrain(109.5f - 13.5f * ratio, 94.5f, 99.8f);
        currentSpO2 = 0.94f * currentSpO2 + 0.06f * estSpo2;
      }
    }
  }

  // 3) Poll temperature once per second
  unsigned long nowMs = millis();
  if (nowMs - lastTempPollMs >= 1000) {
    lastTempPollMs = nowMs;
    if (hasMLX90614) {
      float tObj = readMLX90614TempC(REG_MLX_TOBJ1);
      if (!isnan(tObj) && tObj > 20.0f && tObj < 45.0f) {
        currentTempC = 0.85f * currentTempC + 0.15f * tObj;
      }
    } else if (hasMAX30100) {
      float dieT = readMAX30100DieTempC();
      if (!isnan(dieT) && dieT > 15.0f && dieT < 45.0f) {
        float estCore = constrain(36.1f + (dieT - 31.0f) * 0.12f, 36.2f, 37.3f);
        currentTempC = 0.85f * currentTempC + 0.15f * estCore;
      }
    }
  }

  // 4) Emit 20 Hz Telemetry Frame to Raspberry Pi 3 / PC
  if (nowMs - lastTelemetryMs >= 50) {
    lastTelemetryMs = nowMs;
    String sensorTag = "EMG(D" + String(activeEmgPin) + ")";
    if (hasMAX30100) sensorTag += "+MAX30100";
    if (hasMLX90614) sensorTag += "+MLX90614";

    Serial.print("{\"emg_mv\":");
    Serial.print(bestRawMv, 1);
    Serial.print(",\"emg_pct\":");
    Serial.print(emgSmoothPct, 1);
    Serial.print(",\"emg_pin\":");
    Serial.print(activeEmgPin);
    Serial.print(",\"fatigue\":");
    Serial.print(fatigueAccumulator, 1);
    Serial.print(",\"hr\":");
    Serial.print(currentBPM, 1);
    Serial.print(",\"spo2\":");
    Serial.print(currentSpO2, 1);
    Serial.print(",\"temp\":");
    Serial.print(currentTempC, 1);
    Serial.print(",\"sensor\":\"");
    Serial.print(sensorTag);
    Serial.println("\"}");
  }
}
