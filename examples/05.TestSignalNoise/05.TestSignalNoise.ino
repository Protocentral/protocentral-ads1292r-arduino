// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// 05.TestSignalNoise
//
// Board self-test, no electrodes needed. 115200 baud.
//  1. Routes the internal 1 Hz test square wave (+/-VREF/2400 = +/-1.008 mV)
//     to both channels and reports the measured peak-to-peak amplitude.
//  2. Shorts both inputs and reports the noise floor in uV RMS.
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

const uint16_t SAMPLES = 500;  // 4 s at 125 SPS

Protocentral_ADS1292R ads(PIN_CS, PIN_DRDY, PIN_START, PIN_RESET);

void readSamples(uint16_t count, int32_t& min1, int32_t& max1, int32_t& min2, int32_t& max2,
                 float& rms1, float& rms2) {
    min1 = min2 = INT32_MAX;
    max1 = max2 = INT32_MIN;
    double sum1 = 0, sum2 = 0, sq1 = 0, sq2 = 0;

    // Discard the filter settling samples
    for (uint16_t i = 0; i < 8;) {
        ADS1292R_Sample s;
        if (ads.available() && ads.read(s)) i++;
    }

    for (uint16_t i = 0; i < count;) {
        ADS1292R_Sample s;
        if (!ads.available() || !ads.read(s)) continue;
        i++;
        min1 = min(min1, s.resp);
        max1 = max(max1, s.resp);
        min2 = min(min2, s.ecg);
        max2 = max(max2, s.ecg);
        sum1 += s.resp;
        sq1 += (double)s.resp * s.resp;
        sum2 += s.ecg;
        sq2 += (double)s.ecg * s.ecg;
    }
    double mean1 = sum1 / count, mean2 = sum2 / count;
    rms1 = sqrt(max(0.0, sq1 / count - mean1 * mean1));
    rms2 = sqrt(max(0.0, sq2 / count - mean2 * mean2));
}

void setup() {
    Serial.begin(115200);
    unsigned long t0 = millis();
    while (!Serial && millis() - t0 < 3000) delay(10);

    // Plain ECG configuration: no respiration modulation, no lead-off current
    ADS1292R_Config cfg;
    cfg.respiration = false;
    cfg.leadOff     = false;
    cfg.ch1Gain     = ADS1292R_Gain::X1;
    cfg.ch2Gain     = ADS1292R_Gain::X1;

    if (!ads.begin(cfg)) {
        Serial.print(F("ADS1292R not found, error "));
        Serial.println((int)ads.lastError());
        while (1) delay(1000);
    }

    int32_t min1, max1, min2, max2;
    float rms1, rms2;

    // ---- 1. Test signal --------------------------------------------------
    ads.setTestSignal(ADS1292R_TestSignal::Square1Hz);
    ads.setInputMux(ADS1292R_Channel::CH1, ADS1292R_Mux::TestSignal);
    ads.setInputMux(ADS1292R_Channel::CH2, ADS1292R_Mux::TestSignal);
    readSamples(SAMPLES, min1, max1, min2, max2, rms1, rms2);

    Serial.println(F("Test signal (expect ~2016 uVpp):"));
    Serial.print(F("  CH1 "));
    Serial.print(ads.countsToMicrovolts(max1 - min1, ADS1292R_Channel::CH1), 0);
    Serial.println(F(" uVpp"));
    Serial.print(F("  CH2 "));
    Serial.print(ads.countsToMicrovolts(max2 - min2, ADS1292R_Channel::CH2), 0);
    Serial.println(F(" uVpp"));

    // ---- 2. Noise floor, inputs shorted, ECG gain ---------------------------
    ads.setTestSignal(ADS1292R_TestSignal::Off);
    ads.setGain(ADS1292R_Channel::CH1, ADS1292R_Gain::X6);
    ads.setGain(ADS1292R_Channel::CH2, ADS1292R_Gain::X6);
    ads.setInputMux(ADS1292R_Channel::CH1, ADS1292R_Mux::Shorted);
    ads.setInputMux(ADS1292R_Channel::CH2, ADS1292R_Mux::Shorted);
    readSamples(SAMPLES, min1, max1, min2, max2, rms1, rms2);

    // RMS is fractional counts, so scale by the microvolts per count
    float uvPerCount1 = ads.countsToMicrovolts(1000000L, ADS1292R_Channel::CH1) / 1.0e6f;
    float uvPerCount2 = ads.countsToMicrovolts(1000000L, ADS1292R_Channel::CH2) / 1.0e6f;

    Serial.println(F("Noise, inputs shorted, gain 6, 125 SPS:"));
    Serial.print(F("  CH1 "));
    Serial.print(rms1 * uvPerCount1, 2);
    Serial.print(F(" uVrms  "));
    Serial.print(ads.countsToMicrovolts(max1 - min1, ADS1292R_Channel::CH1), 2);
    Serial.println(F(" uVpp"));
    Serial.print(F("  CH2 "));
    Serial.print(rms2 * uvPerCount2, 2);
    Serial.print(F(" uVrms  "));
    Serial.print(ads.countsToMicrovolts(max2 - min2, ADS1292R_Channel::CH2), 2);
    Serial.println(F(" uVpp"));
    Serial.println(F("Done."));
}

void loop() {}
