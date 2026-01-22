//////////////////////////////////////////////////////////////////////////////////////////
//
//   Arduino Library for ADS1292R Shield/Breakout
//
//   Copyright (c) 2017 ProtoCentral
//
//   Debug example - prints raw values to Serial Monitor for debugging
//
//   This software is licensed under the MIT License(http://opensource.org/licenses/MIT).
//
//   Hardware Connections (for breakout board):
//   |ADS1292R Pin | Arduino Pin | Description        |
//   |-------------|-------------|--------------------
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
#include <SPI.h>

// Pin definitions
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

// Create sensor instance
ADS1292R ecgSensor(ADS1292R_DRDY_PIN, ADS1292R_CS_PIN, ADS1292R_START_PIN, ADS1292R_PWDN_PIN);

unsigned long sampleCount = 0;
unsigned long lastPrint = 0;

void setup() {
    delay(2000);

    Serial.begin(115200);
    while (!Serial) {
        delay(10);
    }

    Serial.println("ADS1292R Debug Example");
    Serial.println("======================");

#if defined(ARDUINO_ARCH_ESP32)
    ecgSensor.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN);
#else
    ecgSensor.begin();
#endif

    // Enable ECG with respiration measurement
    ecgSensor.beginECGWithRespiration();

    // Read and print device ID
    uint8_t deviceId = ecgSensor.readDeviceID();
    Serial.print("Device ID: 0x");
    Serial.println(deviceId, HEX);
    Serial.println();

    Serial.println("Sample#, ECG_raw24, ECG_16(>>6), Resp_raw24, Resp_16(>>4), LeadOff, Status");
    Serial.println("------------------------------------------------------------------------");

    lastPrint = millis();
}

void loop() {
    if (ecgSensor.isDataReady()) {
        ADS1292R_Data data = ecgSensor.getData();

        if (data.ok) {
            sampleCount++;

            // Convert 24-bit to 16-bit with better resolution
            // ECG: >> 6 for better resolution
            int32_t ecgScaled = data.ecg >> 6;
            if (ecgScaled > 32767) ecgScaled = 32767;
            if (ecgScaled < -32768) ecgScaled = -32768;
            int16_t ecg16 = (int16_t)ecgScaled;

            // Respiration: >> 4 for better resolution
            int32_t respScaled = data.respiration >> 4;
            if (respScaled > 32767) respScaled = 32767;
            if (respScaled < -32768) respScaled = -32768;
            int16_t resp16 = (int16_t)respScaled;

            // Print every 25th sample (5 times per second at 125 SPS)
            if (sampleCount % 25 == 0) {
                Serial.print(sampleCount);
                Serial.print(", ");
                Serial.print(data.ecg);
                Serial.print(", ");
                Serial.print(ecg16);
                Serial.print(", ");
                Serial.print(data.respiration);
                Serial.print(", ");
                Serial.print(resp16);
                Serial.print(", ");
                Serial.print(data.leadOff ? "YES" : "NO");
                Serial.print(", 0x");
                Serial.println(data.status, HEX);
            }

            // Print sample rate every 5 seconds
            if (millis() - lastPrint >= 5000) {
                float sps = sampleCount / ((millis() - lastPrint) / 1000.0);
                Serial.print("\n>>> Sample rate: ");
                Serial.print(sps, 1);
                Serial.println(" SPS\n");

                sampleCount = 0;
                lastPrint = millis();
            }
        }
    }
}
