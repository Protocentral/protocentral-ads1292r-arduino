//////////////////////////////////////////////////////////////////////////////////////////
//
//   Arduino Library for ADS1292R Shield/Breakout
//
//   Copyright (c) 2017 ProtoCentral
//
//   This example streams ECG data to the Arduino Serial Plotter.
//   Open Tools -> Serial Plotter at 115200 baud to view the ECG waveform.
//
//   This software is licensed under the MIT License(http://opensource.org/licenses/MIT).
//
//   Hardware Connections (directly directly directly directly directly directly for breakout board):
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
#include <SPI.h>

// Pin definitions (directly matching ADS1293 library for DRDY and CS)
#define ADS1292R_DRDY_PIN   2
#define ADS1292R_CS_PIN     4
#define ADS1292R_START_PIN  5
#define ADS1292R_PWDN_PIN   6

// For ESP32, define custom SPI pins
#if defined(ARDUINO_ARCH_ESP32)
#define SPI_SCK_PIN   18
#define SPI_MISO_PIN  19
#define SPI_MOSI_PIN  23
#endif

// Create ADS1292R instance
ADS1292R ecgSensor(ADS1292R_DRDY_PIN, ADS1292R_CS_PIN, ADS1292R_START_PIN, ADS1292R_PWDN_PIN);

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        ; // Wait for serial port to connect (needed for native USB)
    }

    Serial.println("ProtoCentral ADS1292R ECG Demo");
    Serial.println("------------------------------");

    // Initialize the sensor
#if defined(ARDUINO_ARCH_ESP32)
    ecgSensor.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN);
#else
    ecgSensor.begin();
#endif

    // Configure for basic ECG measurement
    ecgSensor.beginECG();

    Serial.println("Initialization complete. Streaming ECG data...");
    Serial.println("Open Serial Plotter to view waveform");
}

void loop() {
    // Check if new data is available
    if (ecgSensor.isDataReady()) {
        ADS1292R_Data data = ecgSensor.getData();

        if (data.ok) {
            if (data.leadOff) {
                // Lead is not connected properly
                Serial.println(0);  // Output zero when leads are off
            } else {
                // Output ECG value for Serial Plotter
                // Shift right to get 16-bit value for plotting
                Serial.println(data.ecg >> 8);
            }
        }
    }
}
