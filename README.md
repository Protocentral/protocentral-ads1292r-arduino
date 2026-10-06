# ProtoCentral ADS1292R ECG and Respiration Arduino Library

[![Arduino Lint](https://github.com/Protocentral/protocentral-ads1292r-arduino/actions/workflows/arduino-lint.yml/badge.svg)](https://github.com/Protocentral/protocentral-ads1292r-arduino/actions/workflows/arduino-lint.yml)
[![Compile Examples](https://github.com/Protocentral/protocentral-ads1292r-arduino/actions/workflows/compile-examples.yml/badge.svg)](https://github.com/Protocentral/protocentral-ads1292r-arduino/actions/workflows/compile-examples.yml)

Arduino driver for the Texas Instruments ADS1292R ECG and respiration front-end, as used on the ProtoCentral ADS1292R Breakout (v4) and ADS1292R Shield.

## Don't have one? [Buy the breakout here](https://protocentral.com/product/ads1292r-ecg-respiration-breakout-kit/) or [the shield here](https://protocentral.com/product/ads1292r-ecg-respiration-shield-for-arduino-v2/)

![ADS1292R breakout](assets/ads1292r_breakout.jpg)

The ADS1292R is a two-channel, 24-bit analog front-end. On ProtoCentral boards:
- **Channel 2** records a single-lead ECG.
- **Channel 1** measures respiration by *impedance pneumography*: a small high-frequency current through the same two chest electrodes picks up the change in chest impedance as you breathe.

A third right-leg (RL) electrode reduces common-mode noise.

> **For research and education only - not a medical device.**

## Features

- ECG and respiration at **125 SPS to 8 kSPS**, with PGA gain 1-12 per channel
- **Lead-off detection** per electrode (DC or AC), reported with every sample at no extra SPI cost
- **Respiration control:**
  - 32 or 64 kHz modulation
  - demodulation phase in 11.25° steps
  - offset calibration
- **Self-test and diagnostics:**
  - internal 1 Hz test signal
  - shorted-input noise measurement
  - on-die **temperature** sensor
  - **AVDD / DVDD supply** monitoring
- **Right-leg drive** with selectable sources and RLD lead-off sensing
- **v4 hardware features:**
  - CLK output for **multi-board clock sync**
  - **GPIO1/GPIO2** on test pads
- **Polled or interrupt-driven** data ready, with a missed-sample counter
- **Bus friendly:**
  - SPI transactions on any `SPIClass`
  - automatic SDATAC/RDATAC handling, so registers can be changed while streaming
  - register writes verified by read-back
- **Optional algorithms** (`ADS1292R_Algorithms`):
  - fixed-point heart rate and respiration rate
  - ~1 KB RAM, so ECG + respiration fit on an Arduino Uno
- Works on AVR, Renesas (Uno R4), mbed (Nano 33 BLE), ESP32 and RP2040, from **3.3 V or 5 V** hosts

## Installation

### Arduino Library Manager (recommended)

1. Open **Sketch → Include Library → Manage Libraries**.
2. Search for **ProtoCentral ADS1292R**.
3. Install **ProtoCentral ADS1292R ECG and Respiration boards library**.

### Manual installation

Download this repository as a ZIP, then use **Sketch → Include Library → Add .ZIP Library**.

## Hardware Setup

### Breakout v4 host header (J4, 1×10 female socket)

| J4 pin | Function | Arduino Uno | ESP32 DevKit | Uno R4 / Nano 33 BLE |
|---|---|---|---|---|
| CS    | SPI chip select | D7 | GPIO13 | D7 |
| MOSI  | SPI data in | D11 | GPIO23 | D11 |
| SCK   | SPI clock | D13 | GPIO18 | D13 |
| MISO  | SPI data out | D12 | GPIO19 | D12 |
| DRDY  | Data ready (active low) | D6 | GPIO26 | D6 |
| START | Start conversions | D5 | GPIO14 | D5 |
| RST   | Power-down / reset (labelled PWDN/RESET on earlier boards) | D4 | GPIO27 | D4 |
| CLK   | Clock in/out, for multi-board sync. Leave open normally | - | - | - |
| VCC   | 3.3 V or 5 V (on-board 3.3 V regulator and TXB0108 level shifter) | 5V | 3V3 | 5V / 3V3 |
| GND   | Ground | GND | GND | GND |

**Pin notes**
- The **ADS1292R Shield** plugs straight onto an Uno and uses the same D4–D7 pins.
- To use **DRDY interrupts** on an Uno, move DRDY to D2 or D3 (example 07).
- **START and RST are optional** in the constructor (pass `-1`). Without them the library uses the START/STOP and RESET opcodes, and START must then be tied low.

**CLKSEL jumper (JP1)** selects the internal oscillator or an external clock on CLK. Use the internal oscillator for a single board; see example 08 for multi-board sync.

### Electrodes

Plug the electrode cable into the 3.5 mm jack. Place RA and LA on either side of the chest (or on each wrist), and RL on the lower right abdomen or right leg.

Respiration uses the same RA/LA electrodes. Chest placement gives a much larger respiration signal than limb placement.

## Quick Start

```cpp
#include <SPI.h>
#include "Protocentral_ADS1292R.h"

Protocentral_ADS1292R ads(7 /*CS*/, 6 /*DRDY*/, 5 /*START*/, 4 /*RST*/);

void setup() {
    Serial.begin(115200);
    if (!ads.begin()) {           // reset, check ID, ECG + respiration at 125 SPS
        Serial.println("ADS1292R not found");
        while (1);
    }
}

void loop() {
    ADS1292R_Sample s;
    if (ads.available() && ads.read(s)) {
        Serial.print(s.ecg);
        Serial.print(' ');
        Serial.println(s.resp);   // s.leadOff != 0 when an electrode is off
    }
}
```

### Heart rate and respiration rate

```cpp
#include "Protocentral_ADS1292R_Algorithms.h"
ADS1292R_Algorithms algo;               // in setup(): algo.begin(ads.getSampleRate());

algo.processSample(s.ecg, s.resp);      // every sample, at 125 SPS
algo.heartRate();                       // bpm, 0 until locked
algo.respirationRate();                 // breaths/min, 0 until locked
algo.filteredEcg();                     // 0.16-40 Hz band-passed ECG, int16
```

## Default configuration

`begin()` with no arguments applies the ProtoCentral ECG + respiration profile:
- 125 SPS
- CH1 respiration at gain 4, 32 kHz modulation, 135° phase (TI's recommended setting)
- CH2 ECG at gain 12
- 2.42 V internal reference
- RLD driven from CH2
- DC lead-off sensing on both ECG inputs at 6 nA

To change any of these, pass an `ADS1292R_Config` to `begin()`:

```cpp
ADS1292R_Config cfg;
cfg.dataRate    = ADS1292R_DataRate::SPS_500;
cfg.ch2Gain     = ADS1292R_Gain::X6;
cfg.respiration = false;
ads.begin(cfg);
```

You can also call the setters below at any time, including while streaming.

## API Reference

### `Protocentral_ADS1292R`

| Method | Description |
|---|---|
| `Protocentral_ADS1292R(cs, drdy, start = -1, reset = -1, SPIClass& spi = SPI)` | Constructor. `-1` = pin not wired |
| `bool begin(bool startSPI = true)` | Reset, verify ID `0x73`, apply the default profile, start streaming |
| `bool begin(const ADS1292R_Config&, bool startSPI = true)` | Same, with a custom configuration |
| `bool begin(sck, miso, mosi, config)` | ESP32 only: custom SPI pins |
| `void end()` | Stop, standby, release the interrupt |
| `ADS1292R_Status lastError()` | Reason for the last failure |
| `bool isConnected()` / `uint8_t readID()` | Presence check / raw ID register |
| `getLibraryVersion()` | Library version string |

**Data**

| Method | Description |
|---|---|
| `bool available()` | A new sample is ready |
| `bool read(ADS1292R_Sample&)` | Read `ecg`, `resp` (signed 24-bit counts), `leadOff` flags and `gpio` levels |
| `bool enableDataReadyInterrupt()` / `disableDataReadyInterrupt()` | Latch DRDY with an interrupt |
| `uint32_t missedSamples()` | Samples overwritten before being read (interrupt mode) |
| `float countsToMicrovolts(counts, channel)` | Convert at the channel's current gain and reference |

**Conversion and channels**

| Method | Description |
|---|---|
| `start()` / `stop()` / `standby()` / `wakeup()` | Conversion and power control |
| `setDataRate(ADS1292R_DataRate)` / `getSampleRate()` | 125-8000 SPS |
| `setSingleShot(bool)` | One conversion per `start()` |
| `setGain(channel, ADS1292R_Gain)` / `getGain(channel)` | PGA gain 1, 2, 3, 4, 6, 8, 12 |
| `setInputMux(channel, ADS1292R_Mux)` | Electrode, shorted, test signal, temperature, supply, RLD... |
| `setChannelPowerDown(channel, bool)` | Power a channel down or back up |
| `setReference(ADS1292R_Reference)` | 2.42 V or 4.033 V (AVDD ≥ 4.4 V) |
| `setTestSignal(ADS1292R_TestSignal)` | Off, DC, or 1 Hz square, ±VREF/2400 |

**ECG features**

| Method | Description |
|---|---|
| `configureLeadOff(enable, sense, threshold, current, ac)` | Lead-off comparators and excitation |
| `readLeadOffStatus()` | LOFF_STAT register (also in every sample) |
| `configureRespiration(enable, freq, phase)` | Respiration modulation and demodulation on CH1 |
| `runOffsetCalibration()` | OFFSETCAL. Re-runs automatically after `setGain()` |
| `configureRLD(enable, sources, senseLeadOff)` | Right-leg drive |

**Board features**

| Method | Description |
|---|---|
| `setClockOutput(bool)` | Drive the oscillator out on CLK (multi-board master) |
| `setGPIOMode(gpio, output)` / `writeGPIO(gpio, level)` / `readGPIO(gpio)` | GPIO1/GPIO2 |
| `float readTemperature()` | Die temperature, °C. Briefly borrows CH1 |
| `float readAnalogSupply()` / `readDigitalSupply()` | AVDD / DVDD in volts. Briefly borrow a channel |

**Registers**

| Method | Description |
|---|---|
| `readRegister(reg)` / `writeRegister(reg, value)` / `printRegisters(Stream&)` | Raw access (writes are masked and verified) |

Channels are `ADS1292R_Channel::CH1` / `CH2`, also available as `Resp` / `Ecg`. Lead-off and RLD source bits are `ADS1292R_IN1P`, `ADS1292R_IN1N`, `ADS1292R_IN2P`, `ADS1292R_IN2N` and `ADS1292R_RLD`.

### `ADS1292R_Algorithms`

| Method | Description |
|---|---|
| `bool begin(sampleRate = 125)` | Reset. Returns false unless the rate is 125 SPS |
| `reset()` | Clear all state (e.g. after lead-off) |
| `processSample(ecg, resp)` / `processEcg(ecg)` / `processResp(resp)` | Feed raw counts from `read()` |
| `heartRate()` / `respirationRate()` | Rates in bpm / breaths per minute |
| `filteredEcg()` / `filteredResp()` | Filtered waveforms (int16) |

## Examples

| # | Sketch | Description |
|---|---|---|
| 01 | `01.ECGRespSerialPlotter` | ECG and respiration in the Serial Plotter |
| 02 | `02.OpenView` | Stream to ProtoCentral OpenView, with heart rate and respiration rate |
| 03 | `03.HeartRateRespRate` | Heart rate and respiration rate printed once per second (fits on Uno) |
| 04 | `04.LeadOffDetection` | Which electrode is off |
| 05 | `05.TestSignalNoise` | Self-test: internal test signal amplitude and shorted-input noise floor |
| 06 | `06.TemperatureSupply` | Die temperature, AVDD and DVDD |
| 07 | `07.InterruptDriven` | 500 SPS with an interrupt on DRDY |
| 08 | `08.MultiBoardClockSync` | Two v4 boards on one clock, sample-aligned |
| 09 | `09.GPIOTestPads` | Drive and read GPIO1/GPIO2 |
| 10 | `10.Diagnostics` | Version, chip ID, register dump, data-path check |

## Streaming to OpenView

1. Upload `02.OpenView`.
2. Open [ProtoCentral OpenView 2](https://github.com/Protocentral/protocentral_openview2).
3. Select **ADS1292R Breakout**, then pick the serial port.

The sketch sends 125 packets per second at 57600 baud. Each packet carries filtered ECG, respiration, heart rate and respiration rate.

![Streaming output](assets/output.png)

## Troubleshooting

**`begin()` fails with `SpiError`**
- Check the CS, SCK, MOSI and MISO wiring and the power.
- On the v4 breakout, check that JP1 selects the **internal** clock unless an external clock is wired to CLK. With no clock the chip does not answer.

**`begin()` fails with `VerifyFailed`**
- Registers are not reading back correctly. Shorten the wires, or lower the SPI clock with `#define ADS1292R_SPI_CLOCK 500000` before the include.

**No samples (`10.Diagnostics` shows 0)**
- Check the DRDY wiring, and either wire START or tie it low.

**Flat or noisy ECG**
- Run `05.TestSignalNoise` first. If the board passes, the problem is electrode contact or placement.
- Fresh gel electrodes on clean skin make a large difference.

**Heart rate stays 0**
- The detector needs about 5 s of clean signal to lock.
- Stay still, and make sure `leadOff` is 0.

**Respiration rate stays 0**
- Respiration needs about 10 s and a breathing signal of sufficient amplitude.
- Chest placement works much better than wrists.

## Migrating from 1.x

Version 2.0 is a ground-up rewrite. The old API has been removed.

| 1.x | 2.0 |
|---|---|
| `#include "protocentralAds1292r.h"` | `#include "Protocentral_ADS1292R.h"` |
| `ads1292r ADS1292R;` | `Protocentral_ADS1292R ads(CS, DRDY, START, RST);` |
| `SPI.begin(); SPI.beginTransaction(...)` + `pinMode(...)` in the sketch | Handled by `ads.begin()` |
| `ADS1292R.ads1292Init(CS, PWDN, START)` | `ads.begin()` (returns `bool`) |
| `getAds1292EcgAndRespirationSamples(DRDY, CS, &values)` | `if (ads.available() && ads.read(s))` |
| `values.sDaqVals[1]` / `sDaqVals[0]` | `s.ecg` / `s.resp` |
| `values.leadoffDetected` (never set in 1.x) | `s.leadOff != 0` |
| `#include "ecgRespirationAlgo.h"`, `ecg_respiration_algorithm` | `#include "Protocentral_ADS1292R_Algorithms.h"`, `ADS1292R_Algorithms` |
| `ECG_ProcessCurrSample` + `QRS_Algorithm_Interface` | `algo.processEcg(s.ecg)`, `algo.filteredEcg()`, `algo.heartRate()` |

Waveform amplitudes and the OpenView packet layout differ from 1.x. See [CHANGELOG.md](CHANGELOG.md) for details.

## References

- [ADS1292R datasheet (TI SBAS502)](https://www.ti.com/lit/ds/symlink/ads1292r.pdf)
- [ProtoCentral OpenView 2](https://github.com/Protocentral/protocentral_openview2)

## License

- **Software:** MIT
- **Hardware:** CERN-OHL-P v2 (Breakout v4); CC BY-SA 4.0 (earlier boards)
- **Documentation:** CC BY-SA 4.0

See [LICENSE.md](LICENSE.md).

## Support

Open an issue on [GitHub](https://github.com/Protocentral/protocentral-ads1292r-arduino/issues) or email support@protocentral.com.
