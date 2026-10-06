// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// 06.TemperatureSupply
//
// Reads the ADS1292R's on-die temperature sensor and its analog (AVDD) and
// digital (DVDD) supplies every 2 seconds. 115200 baud.
//
// Each measurement briefly borrows a channel through the input multiplexer
// (about 100 ms at 125 SPS), so ECG/respiration samples are paused meanwhile.
// The die runs a little warmer than the board because of self-heating.
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

void loop() {
    Serial.print(F("Die temperature: "));
    Serial.print(ads.readTemperature(), 1);
    Serial.print(F(" C   AVDD: "));
    Serial.print(ads.readAnalogSupply(), 3);
    Serial.print(F(" V   DVDD: "));
    Serial.print(ads.readDigitalSupply(), 3);
    Serial.println(F(" V"));
    delay(2000);
}
