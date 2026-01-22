# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Arduino library for the ProtoCentral ADS1292R ECG/Respiration shield and breakout boards. The ADS1292R is a 24-bit ADC from Texas Instruments used for biopotential measurement. This library enables ECG signal acquisition and respiration monitoring via impedance pneumography.

## Build and CI

This is an Arduino library with no local build commands. CI runs automatically on push/PR:

- **Compile Examples**: `.github/workflows/compile-examples.yml` - Compiles all examples across 25+ board targets (AVR, SAMD, ESP32, ESP8266, STM32, RP2040, nRF52840, Apollo3)
- **Arduino Lint**: `.github/workflows/main.yml` - Validates library structure and metadata

To manually verify, use Arduino IDE or `arduino-cli compile --fqbn <board> examples/<example>`.

## Architecture

### Source Files (src/)

**protocentral_ads1292r.h / .cpp** - Hardware driver
- `ADS1292R` class for SPI communication with the chip
- Configuration enums: `ADS1292R_SamplingRate`, `ADS1292R_Gain`, `ADS1292R_InputMux`, etc.
- `ADS1292R_Data` struct with `ecg`, `respiration`, `leadOff`, and `ok` fields
- Key methods:
  - `begin()` / `begin(sck, miso, mosi)` - Initialize with optional custom SPI pins
  - `beginECG()` / `beginECGWithRespiration()` - Convenience setup methods
  - `isDataReady()` - Check DRDY pin
  - `getData()` - Returns `ADS1292R_Data` struct

**protocentral_ecg_respiration_algorithm.h / .cpp** - Signal processing
- `ECGRespirationAlgorithm` class for filtering and vital signs computation
- `VitalSigns` struct with `heartRate`, `respirationRate`, and validity flags
- 161-tap FIR filters (40Hz LP for ECG, 2Hz LP for respiration)
- QRS detection algorithm for heart rate calculation
- Key method: `processSample(ecgSample, respSample)` returns `VitalSigns`

### Example Naming Convention

Examples follow the pattern `XX-description`:
- `01-ecg-stream` - Basic ECG streaming to Serial Plotter
- `02-ecg-plot-openview` - ECG/respiration to ProtoCentral OpenView GUI
- `03-ecg-respiration-vitals` - Heart rate and respiration rate computation

### Hardware Configuration

Default pin mapping (matching ADS1293 library for DRDY and CS):
| Pin | Arduino | Function |
|-----|---------|----------|
| DRDY | D2 | Data Ready (active low) |
| CS | D4 | Chip Select |
| START | D5 | Start conversions |
| PWDN/RESET | D6 | Power down / Reset |
| MOSI/MISO/SCK | D11/D12/D13 | SPI bus |

ESP32 custom SPI pins (18/19/23): Use `begin(sckPin, misoPin, mosiPin)`.

### SPI Configuration

- 1 MHz clock speed, MSB first, SPI Mode 1

### Memory Constraints

The `ECGRespirationAlgorithm` class uses ~1.3KB RAM for filter buffers. Arduino Uno/Nano may not support full respiration processing. For low-memory boards, use only `filterECG()` and `computeHeartRate()`.

### Code Style

- Class names: PascalCase (`ADS1292R`, `ECGRespirationAlgorithm`)
- File names: snake_case (`protocentral_ads1292r.h`)
- Enums: Scoped enums with `ADS1292R_` prefix
- Constants: SCREAMING_SNAKE_CASE with `ADS1292R_` prefix
