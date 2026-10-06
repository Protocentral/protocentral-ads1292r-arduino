// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// 07.InterruptDriven
//
// Runs the ADS1292R at 500 SPS with DRDY latched by an interrupt, so no sample
// is lost while loop() is busy. Prints ECG for the Serial Plotter, plus a
// once-per-second count of samples received and missed. 115200 baud.
//
// DRDY must be on an interrupt-capable pin. On an Uno that is D2 or D3 - the
// ADS1292R Shield's D6 is not, so jumper DRDY to D2 for this example.
//
// Wiring: see the README (Hardware Setup section).
// For research and education only - not a medical device.

#include <SPI.h>
#include "Protocentral_ADS1292R.h"

#if defined(ARDUINO_ARCH_ESP32)
const int PIN_CS = 13, PIN_DRDY = 26, PIN_START = 14, PIN_RESET = 27;
#else
const int PIN_CS = 7, PIN_DRDY = 2, PIN_START = 5, PIN_RESET = 4;
#endif

Protocentral_ADS1292R ads(PIN_CS, PIN_DRDY, PIN_START, PIN_RESET);

uint32_t received = 0;
unsigned long lastReport = 0;

void setup() {
    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && millis() - t0 < 3000) delay(10);

    ADS1292R_Config cfg;
    cfg.dataRate    = ADS1292R_DataRate::SPS_500;
    cfg.respiration = false;

    if (!ads.begin(cfg)) {
        Serial.print(F("ADS1292R not found, error "));
        Serial.println((int)ads.lastError());
        while (1) delay(1000);
    }
    if (!ads.enableDataReadyInterrupt()) {
        Serial.println(F("DRDY pin is not interrupt-capable on this board"));
        while (1) delay(1000);
    }
}

void loop() {
    ADS1292R_Sample s;
    if (ads.available() && ads.read(s)) {
        received++;
        Serial.print(F("ecg:"));
        Serial.println(s.ecg);
    }

    if (millis() - lastReport >= 1000) {
        lastReport = millis();
        Serial.print(F("received:"));
        Serial.print(received);
        Serial.print(F(" missed:"));
        Serial.println(ads.missedSamples());
        received = 0;
    }
}
