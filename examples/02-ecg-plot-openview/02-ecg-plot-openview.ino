//////////////////////////////////////////////////////////////////////////////////////////
//
//   Arduino Library for ADS1292R Shield/Breakout
//
//   Copyright (c) 2017 ProtoCentral
//
//   This example streams ECG and respiration data to ProtoCentral OpenView GUI.
//   Download OpenView from: https://github.com/Protocentral/protocentral_openview
//
//   This software is licensed under the MIT License(http://opensource.org/licenses/MIT).
//
//   Hardware Connections (for breakout board):
//   |ADS1292R Pin | Arduino Pin | Description        |
//   |-------------|-------------|--------------------|
//   | VDD         | +5V         | Power Supply       |
//   | GND         | GND         | Ground             |
//   | MISO        | D12         | SPI MISO           |
//   | MOSI        | D11         | SPI MOSI           |
//   | SCK         | D13         | SPI Clock          |
//   | CS          | D4          | Chip Select        |
//   | DRDY        | D2          | Data Ready         |
//   | START       | D5          | Start Conversion   |
//   | PWDN/RESET  | D6          | Power Down/Reset   |
//
/////////////////////////////////////////////////////////////////////////////////////////

#include "protocentral_ads1292r.h"
#include "protocentral_ecg_respiration_algorithm.h"
#include <SPI.h>

// Pin definitions (matching ADS1293 library for DRDY and CS)
#define ADS1292R_DRDY_PIN   2
#define ADS1292R_CS_PIN     4
#define ADS1292R_START_PIN  5
#define ADS1292R_PWDN_PIN   6

// For ESP32
#if defined(ARDUINO_ARCH_ESP32)
#define SPI_SCK_PIN   18
#define SPI_MISO_PIN  19
#define SPI_MOSI_PIN  23
#endif

// OpenView packet protocol
#define CES_CMDIF_PKT_START_1   0x0A
#define CES_CMDIF_PKT_START_2   0xFA
#define CES_CMDIF_TYPE_DATA     0x02
#define CES_CMDIF_PKT_STOP      0x0B
#define DATA_LEN                9

// Create instances
ADS1292R ecgSensor(ADS1292R_DRDY_PIN, ADS1292R_CS_PIN, ADS1292R_START_PIN, ADS1292R_PWDN_PIN);
ECGRespirationAlgorithm ecgAlgo;

// Simple exponential smoothing filter for respiration
int32_t respFiltered = 0;
bool respFilterInit = false;

// Packet buffers
const uint8_t packetHeader[5] = {CES_CMDIF_PKT_START_1, CES_CMDIF_PKT_START_2, DATA_LEN, 0, CES_CMDIF_TYPE_DATA};
const uint8_t packetFooter[2] = {0, CES_CMDIF_PKT_STOP};
uint8_t dataPacket[DATA_LEN];

void sendDataToOpenView(int16_t ecgFiltered, int16_t respWave, uint8_t heartRate, uint8_t respRate) {
    dataPacket[0] = ecgFiltered & 0xFF;
    dataPacket[1] = (ecgFiltered >> 8) & 0xFF;
    dataPacket[2] = respWave & 0xFF;
    dataPacket[3] = (respWave >> 8) & 0xFF;
    dataPacket[4] = respRate;
    dataPacket[5] = 0;
    dataPacket[6] = heartRate;
    dataPacket[7] = 0;
    dataPacket[8] = 0;

    // Send header
    Serial.write(packetHeader, 5);
    // Send data
    Serial.write(dataPacket, DATA_LEN);
    // Send footer
    Serial.write(packetFooter, 2);
}

void setup() {
    delay(2000);

    Serial.begin(57600);

#if defined(ARDUINO_ARCH_ESP32)
    ecgSensor.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN);
#else
    ecgSensor.begin();
#endif

    // Enable ECG with respiration measurement
    ecgSensor.beginECGWithRespiration();

    // Reset the algorithm
    ecgAlgo.reset();
}

void loop() {
    if (ecgSensor.isDataReady()) {
        ADS1292R_Data data = ecgSensor.getData();

        if (data.ok) {
            // Scale 24-bit data to 16-bit
            // ECG: shift by 6 for good resolution
            int32_t ecgScaled = data.ecg >> 6;

            // Respiration: use stronger exponential smoothing to eliminate staircase
            // Initialize filter on first sample
            if (!respFilterInit) {
                respFiltered = data.respiration;
                respFilterInit = true;
            }
            // Exponential smoothing: new = 0.0625 * sample + 0.9375 * old (alpha = 1/16)
            // Very strong smoothing for the slow respiration signal
            respFiltered = (data.respiration >> 4) + (respFiltered - (respFiltered >> 4));

            // Scale to 16-bit (shift by 4 for good range)
            int32_t respScaled = respFiltered >> 4;

            // Clamp to 16-bit range
            if (ecgScaled > 32767) ecgScaled = 32767;
            if (ecgScaled < -32768) ecgScaled = -32768;
            if (respScaled > 32767) respScaled = 32767;
            if (respScaled < -32768) respScaled = -32768;

            int16_t ecgSample = (int16_t)ecgScaled;
            int16_t respSample = (int16_t)respScaled;

            if (!data.leadOff) {
                // Send ECG and respiration to OpenView
                sendDataToOpenView(ecgSample, respSample, 0, 0);
            } else {
                // Lead off - send zeros
                sendDataToOpenView(0, 0, 0, 0);
            }
        }
    }
}
