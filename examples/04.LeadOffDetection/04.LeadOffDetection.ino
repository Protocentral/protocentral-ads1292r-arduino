// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// 04.LeadOffDetection
//
// Reports which ADS1292R inputs have lost electrode contact, using the chip's
// DC lead-off comparators. The status arrives with every sample at no extra
// SPI cost. Unplug the cable or lift an electrode to see it change.
// 115200 baud.
//
// Wiring: see the README (Hardware Setup section).
// For research and education only - not a medical device.

#include <SPI.h>
#include "Protocentral_ADS1292R.h"

#if defined(ARDUINO_ARCH_ESP32)
const int PIN_CS = 13, PIN_DRDY = 26, PIN_START = 14, PIN_RESET = 27;
#else
const int PIN_CS = 7, PIN_DRDY = 6, PIN_START = 5, PIN_RESET = 4;  // ADS1292R Shield / Uno
#endif

Protocentral_ADS1292R ads(PIN_CS, PIN_DRDY, PIN_START, PIN_RESET);

uint8_t lastStatus = 0xFF;

void setup() {
    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && millis() - t0 < 3000) delay(10);

    if (!ads.begin()) {
        Serial.print(F("ADS1292R not found, error "));
        Serial.println((int)ads.lastError());
        while (1) delay(1000);
    }

    // Monitor both ECG inputs and the RLD electrode. 6 nA keeps the excitation
    // well below the ECG signal; use 22 nA or more for faster detection.
    ads.configureLeadOff(true, ADS1292R_IN2P | ADS1292R_IN2N, ADS1292R_LeadOffThreshold::Pct95,
                         ADS1292R_LeadOffCurrent::nA_6);
    ads.configureRLD(true, ADS1292R_IN2P | ADS1292R_IN2N, true);
}

void loop() {
    ADS1292R_Sample s;
    if (!ads.available() || !ads.read(s)) return;
    if (s.leadOff == lastStatus) return;
    lastStatus = s.leadOff;

    if (s.leadOff == 0) {
        Serial.println(F("All electrodes connected"));
        return;
    }
    // Electrode names follow the v4 breakout's jack wiring (see README)
    Serial.print(F("Lead off:"));
    if (s.leadOff & ADS1292R_ELECTRODE_RA) Serial.print(F(" RA"));
    if (s.leadOff & ADS1292R_ELECTRODE_LA) Serial.print(F(" LA"));
    if (s.leadOff & ADS1292R_ELECTRODE_RL) Serial.print(F(" RL"));
    Serial.println();
}
