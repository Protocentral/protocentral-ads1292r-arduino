// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// 10.Diagnostics
//
// First thing to run when something does not work. 115200 baud.
// Prints the library version, the chip ID, a full register dump, and checks
// that DRDY toggles and that samples arrive with a valid status word.
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

const __FlashStringHelper* statusText(ADS1292R_Status st) {
    switch (st) {
        case ADS1292R_Status::Ok:              return F("OK");
        case ADS1292R_Status::NotInitialized:  return F("not initialised");
        case ADS1292R_Status::SpiError:        return F("no SPI response - check CS/SCK/MOSI/MISO, power and the CLKSEL jumper");
        case ADS1292R_Status::WrongChipId:     return F("unexpected chip ID - not an ADS1292R");
        case ADS1292R_Status::VerifyFailed:    return F("register read-back mismatch - check SPI wiring and signal integrity");
        case ADS1292R_Status::InvalidArgument: return F("invalid argument");
        case ADS1292R_Status::NotInterruptPin: return F("DRDY pin has no interrupt");
        case ADS1292R_Status::Timeout:         return F("timeout waiting for DRDY - check DRDY and START wiring");
    }
    return F("unknown");
}

void setup() {
    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && millis() - t0 < 3000) delay(10);

    Serial.print(F("ProtoCentral ADS1292R library v"));
    Serial.println(Protocentral_ADS1292R::getLibraryVersion());

    bool ok = ads.begin();
    Serial.print(F("begin(): "));
    Serial.println(statusText(ads.lastError()));

    Serial.print(F("Chip ID: 0x"));
    Serial.print(ads.readID(), HEX);
    Serial.println(F(" (expected 0x73)"));
    if (!ok) return;

    Serial.println(F("\nRegisters:"));
    ads.printRegisters(Serial);

    Serial.print(F("\nSample rate: "));
    Serial.print(ads.getSampleRate());
    Serial.println(F(" SPS"));

    // Count samples over one second
    uint16_t count = 0, bad = 0;
    t0 = millis();
    while (millis() - t0 < 1000) {
        ADS1292R_Sample s;
        if (ads.available()) {
            if (ads.read(s)) count++;
            else bad++;
        }
    }
    Serial.print(F("Samples in 1 s: "));
    Serial.print(count);
    Serial.print(F("  (bad status words: "));
    Serial.print(bad);
    Serial.println(F(")"));
    Serial.println(count > 0 ? F("Data path OK") : F("No data - check DRDY and START wiring"));

    Serial.print(F("Die temperature: "));
    Serial.print(ads.readTemperature(), 1);
    Serial.println(F(" C"));
}

void loop() {}
