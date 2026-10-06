// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// 09.GPIOTestPads
//
// Drives the ADS1292R's GPIO1 as a 1 Hz square wave and reads GPIO2 as an
// input. On the v4 breakout both are on test pads TP3. Probe GPIO1 with a
// meter or LED, or link GPIO1 to GPIO2 to see the input follow. 115200 baud.
//
// The GPIO levels are also reported in every sample (ADS1292R_Sample::gpio).
// GPIOs are unavailable when respiration uses an external clock (respExternalClock).
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

bool level = false;

void setup() {
    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && millis() - t0 < 3000) delay(10);

    if (!ads.begin()) {
        Serial.print(F("ADS1292R not found, error "));
        Serial.println((int)ads.lastError());
        while (1) delay(1000);
    }
    ads.setGPIOMode(1, true);   // GPIO1 output
    ads.setGPIOMode(2, false);  // GPIO2 input
}

void loop() {
    level = !level;
    ads.writeGPIO(1, level);
    delay(10);

    Serial.print(F("GPIO1 out: "));
    Serial.print(level);
    Serial.print(F("   GPIO2 in: "));
    Serial.println(ads.readGPIO(2));
    delay(490);
}
