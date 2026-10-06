// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// 03.HeartRateRespRate
//
// Computes heart rate (QRS detection on CH2) and respiration rate (impedance
// pneumography on CH1) on the microcontroller and prints them once per second
// at 115200 baud. Allow ~10 s after attaching electrodes for both to lock.
//
// Fits on an Arduino Uno (about 1.2 KB of RAM in total).
//
// Wiring: see the README (Hardware Setup section).
// For research and education only - not a medical device.

#include <SPI.h>
#include "Protocentral_ADS1292R.h"
#include "Protocentral_ADS1292R_Algorithms.h"

#if defined(ARDUINO_ARCH_ESP32)
const int PIN_CS = 13, PIN_DRDY = 26, PIN_START = 14, PIN_RESET = 27;
#else
const int PIN_CS = 7, PIN_DRDY = 6, PIN_START = 5, PIN_RESET = 4;  // ADS1292R Shield / Uno
#endif

Protocentral_ADS1292R ads(PIN_CS, PIN_DRDY, PIN_START, PIN_RESET);
ADS1292R_Algorithms algo;

unsigned long lastPrint = 0;
bool leadOff = false;

void setup() {
    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && millis() - t0 < 3000) delay(10);

    if (!ads.begin()) {
        Serial.print(F("ADS1292R not found, error "));
        Serial.println((int)ads.lastError());
        while (1) delay(1000);
    }
    if (!algo.begin(ads.getSampleRate())) {
        Serial.println(F("Algorithms need 125 SPS"));
        while (1) delay(1000);
    }
    Serial.println(F("Attach electrodes and stay still..."));
}

void loop() {
    ADS1292R_Sample s;
    if (ads.available() && ads.read(s)) {
        if (s.leadOff) {
            if (!leadOff) algo.reset();
            leadOff = true;
        } else {
            leadOff = false;
            algo.processSample(s.ecg, s.resp);
        }
    }

    if (millis() - lastPrint >= 1000) {
        lastPrint = millis();
        if (leadOff) {
            Serial.println(F("Lead off - check electrodes"));
        } else {
            Serial.print(F("Heart rate: "));
            Serial.print(algo.heartRate());
            Serial.print(F(" bpm   Respiration rate: "));
            Serial.print(algo.respirationRate());
            Serial.println(F(" br/min"));
        }
    }
}
