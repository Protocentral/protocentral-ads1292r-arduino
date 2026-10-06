// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// 02.OpenView
//
// Streams filtered ECG, respiration, heart rate and respiration rate to
// ProtoCentral OpenView (https://github.com/Protocentral/protocentral_openview2).
// In OpenView, choose the "ADS1292R Breakout" board and this serial port.
//
// Wire format (must match OpenView's ads1292r descriptor - change both together):
//   57600 baud, 125 packets/s
//   0x0A 0xFA <len=8> 0x00 <type=0x02> | ECG int16 | RESP int16 | HR int16 | RR int16 | 0x00 0x0B
//   (all little-endian)
//
// This sketch prints nothing but packets - any other text would corrupt the stream.
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

const uint8_t PKT_START_1  = 0x0A;
const uint8_t PKT_START_2  = 0xFA;
const uint8_t PKT_TYPE     = 0x02;
const uint8_t PKT_STOP_1   = 0x00;
const uint8_t PKT_STOP_2   = 0x0B;
const uint8_t PAYLOAD_LEN  = 8;

Protocentral_ADS1292R ads(PIN_CS, PIN_DRDY, PIN_START, PIN_RESET);
ADS1292R_Algorithms algo;

static void putInt16(uint8_t* p, int16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}

static void sendPacket(int16_t ecg, int16_t resp, int16_t hr, int16_t rr) {
    uint8_t pkt[5 + PAYLOAD_LEN + 2] = {PKT_START_1, PKT_START_2, PAYLOAD_LEN, 0x00, PKT_TYPE};
    putInt16(&pkt[5], ecg);
    putInt16(&pkt[7], resp);
    putInt16(&pkt[9], hr);
    putInt16(&pkt[11], rr);
    pkt[13] = PKT_STOP_1;
    pkt[14] = PKT_STOP_2;
    Serial.write(pkt, sizeof(pkt));
}

void setup() {
    Serial.begin(57600);

    if (!ads.begin()) {
        // No text allowed on the stream: blink the LED instead
#ifdef LED_BUILTIN
        pinMode(LED_BUILTIN, OUTPUT);
        while (1) {
            digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
            delay(200);
        }
#else
        while (1) delay(1000);
#endif
    }
    algo.begin(ads.getSampleRate());
}

void loop() {
    ADS1292R_Sample s;
    if (!ads.available() || !ads.read(s)) return;

    if (s.leadOff) {
        // Electrode off: flat-line the traces and restart the detectors
        algo.reset();
        sendPacket(0, 0, 0, 0);
        return;
    }

    algo.processSample(s.ecg, s.resp);
    sendPacket(algo.filteredEcg(), algo.filteredResp(), algo.heartRate(), algo.respirationRate());
}
