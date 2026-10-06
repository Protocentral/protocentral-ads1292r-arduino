// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// 08.MultiBoardClockSync
//
// Two ADS1292R Breakout v4 boards sampling in lock-step from one clock: four
// ECG channels with sample-aligned timing. 115200 baud, Serial Plotter.
//
// Hardware (v4 breakout):
//   - Board A (master): CLKSEL jumper JP1 = internal oscillator.
//     The library sets CLK_EN so A drives its 512 kHz clock out on CLK.
//   - Board B (slave):  CLKSEL jumper JP1 = external clock. Wire A.CLK -> B.CLK.
//   - SCK/MOSI/MISO shared; separate CS, DRDY and RST per board.
//   - START of both boards tied together to one pin, driven by this sketch.
//
// Board A must be configured first: board B has no clock (and does not
// answer on SPI) until A's clock output is on.
//
// Wiring: see the README (Hardware Setup section).
// For research and education only - not a medical device.

#include <SPI.h>
#include "Protocentral_ADS1292R.h"

#if defined(ARDUINO_ARCH_ESP32)
const int PIN_START = 14;
const int A_CS = 13, A_DRDY = 26, A_RESET = 27;
const int B_CS = 15, B_DRDY = 25, B_RESET = 33;
#else
const int PIN_START = 5;
const int A_CS = 7, A_DRDY = 6, A_RESET = 4;
const int B_CS = 10, B_DRDY = 9, B_RESET = 8;
#endif

// START is shared, so the sketch owns it (-1 here)
Protocentral_ADS1292R boardA(A_CS, A_DRDY, -1, A_RESET);
Protocentral_ADS1292R boardB(B_CS, B_DRDY, -1, B_RESET);

void halt(const __FlashStringHelper* msg, Protocentral_ADS1292R& dev) {
    Serial.print(msg);
    Serial.println((int)dev.lastError());
    while (1) delay(1000);
}

void setup() {
    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && millis() - t0 < 3000) delay(10);

    pinMode(PIN_START, OUTPUT);
    digitalWrite(PIN_START, LOW);

    ADS1292R_Config cfg;
    cfg.respiration = false;  // the respiration modulator is clocked separately
    cfg.autoStart   = false;  // conversions start together on the shared START pin

    ADS1292R_Config master = cfg;
    master.clockOutput = true;
    if (!boardA.begin(master)) halt(F("Board A (master) not found, error "), boardA);

    // SPI was already started by board A
    if (!boardB.begin(cfg, false)) halt(F("Board B (slave) not found - check CLK wiring and JP1, error "), boardB);

    // One START edge starts both converters on the same clock cycle
    digitalWrite(PIN_START, HIGH);
}

void loop() {
    // DRDY of both boards fall together; read A then B
    ADS1292R_Sample a, b;
    if (boardA.available() && boardA.read(a)) {
        unsigned long t0 = micros();
        while (!boardB.available()) {
            if (micros() - t0 > 2000) return;  // board B out of step
        }
        boardB.read(b);
        Serial.print(F("A_ecg:"));
        Serial.print(a.ecg);
        Serial.print(F(" B_ecg:"));
        Serial.println(b.ecg);
    }
}
