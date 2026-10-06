// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// 01.ECGRespSerialPlotter
//
// Streams raw ECG and respiration from the ADS1292R to the Arduino Serial
// Plotter (Tools > Serial Plotter, 115200 baud). The respiration trace is a
// slow wave; the ECG trace shows the heartbeat once electrodes are attached.
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

void setup() {
    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && millis() - t0 < 3000) delay(10);

    if (!ads.begin()) {
        Serial.print(F("ADS1292R not found, error "));
        Serial.println((int)ads.lastError());
        while (1) delay(1000);
    }
}

// Raw counts sit on a large electrode offset; subtracting a slow running
// average keeps both traces centred in the plotter
int32_t ecgBaseline = 0, respBaseline = 0;

void loop() {
    ADS1292R_Sample s;
    if (ads.available() && ads.read(s)) {
        ecgBaseline  += (s.ecg - ecgBaseline) / 64;
        respBaseline += (s.resp - respBaseline) / 256;

        Serial.print(F("ecg:"));
        Serial.print(s.ecg - ecgBaseline);
        Serial.print(F(" resp:"));
        Serial.println(s.resp - respBaseline);
    }
}
