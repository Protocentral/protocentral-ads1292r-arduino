//////////////////////////////////////////////////////////////////////////////////////////
//
//   Arduino Library for ADS1292R Shield/Breakout
//
//   Copyright (c) 2017 ProtoCentral
//   Heartrate and respiration computation based on original code from Texas Instruments
//
//   This example computes heart rate and respiration rate from ECG/respiration signals.
//   Note: Full computation requires sufficient RAM - not compatible with Arduino Uno/Nano.
//         For low-memory boards, disable respiration processing.
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

// Create instances
ADS1292R ecgSensor(ADS1292R_DRDY_PIN, ADS1292R_CS_PIN, ADS1292R_START_PIN, ADS1292R_PWDN_PIN);
ECGRespirationAlgorithm ecgAlgo;

// Timing for serial output (don't flood the serial port)
unsigned long lastPrintTime = 0;
const unsigned long PRINT_INTERVAL_MS = 1000;  // Print every 1 second

// Store latest vital signs
VitalSigns currentVitals;

// Respiration smoothing filter
int32_t respFiltered = 0;
bool respFilterInit = false;

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        ; // Wait for serial port
    }

    delay(2000);

    Serial.println("ProtoCentral ADS1292R ECG + Respiration Demo");
    Serial.println("--------------------------------------------");
    Serial.println();

#if defined(ARDUINO_ARCH_ESP32)
    Serial.println("Platform: ESP32");
    ecgSensor.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN);
#else
    Serial.println("Platform: Arduino");
    ecgSensor.begin();
#endif

    // Enable ECG with respiration measurement
    ecgSensor.beginECGWithRespiration();

    // Reset the algorithm state
    ecgAlgo.reset();

    Serial.println("Initialization complete.");
    Serial.println("Attach electrodes and wait for readings...");
    Serial.println();
}

void loop() {
    if (ecgSensor.isDataReady()) {
        ADS1292R_Data data = ecgSensor.getData();

        if (data.ok) {
            // Scale 24-bit data to 16-bit with better resolution
            int32_t ecgScaled = data.ecg >> 6;

            // Respiration: use exponential smoothing to reduce noise
            if (!respFilterInit) {
                respFiltered = data.respiration;
                respFilterInit = true;
            }
            // Exponential smoothing: new = 0.0625 * sample + 0.9375 * old
            respFiltered = (data.respiration >> 4) + (respFiltered - (respFiltered >> 4));
            int32_t respScaled = respFiltered >> 4;

            // Clamp to 16-bit range
            if (ecgScaled > 32767) ecgScaled = 32767;
            if (ecgScaled < -32768) ecgScaled = -32768;
            if (respScaled > 32767) respScaled = 32767;
            if (respScaled < -32768) respScaled = -32768;

            int16_t ecgSample = (int16_t)ecgScaled;
            int16_t respSample = (int16_t)respScaled;

            if (!data.leadOff) {
                // Process samples and compute vital signs
                currentVitals = ecgAlgo.processSample(ecgSample, respSample);
            }

            // Print status periodically
            if (millis() - lastPrintTime >= PRINT_INTERVAL_MS) {
                lastPrintTime = millis();

                if (data.leadOff) {
                    Serial.println("WARNING: ECG leads not connected!");
                    Serial.println("Please ensure electrodes are properly attached.");
                } else {
                    Serial.print("Heart Rate: ");
                    if (currentVitals.heartRateValid) {
                        Serial.print(currentVitals.heartRate);
                        Serial.println(" BPM");
                    } else {
                        Serial.println("-- BPM (calculating...)");
                    }

                    Serial.print("Respiration Rate: ");
                    if (currentVitals.respirationValid) {
                        Serial.print(currentVitals.respirationRate);
                        Serial.println(" breaths/min");
                    } else {
                        Serial.println("-- breaths/min (calculating...)");
                    }

                    Serial.println();
                }
            }
        }
    }
}
