# Changelog

All notable changes to this library are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/).

## [2.0.0] - 2026-10-06

Ground-up rewrite. **Breaking:** the 1.x API is removed. See "Migrating from 1.x" in the README.

### Added
- `Protocentral_ADS1292R` driver class:
  - injected `SPIClass`, with an `SPISettings` transaction around every access
  - pins stored at construction; START and RST optional
  - `bool begin()` with chip-ID check and an `ADS1292R_Status` from `lastError()`
- `ADS1292R_Config` to set the complete device configuration in one call.
- Setters work while streaming: the driver handles SDATAC/RDATAC around each register access and verifies every write by reading it back.
- **Data path:**
  - data rates up to 8 kSPS
  - single-shot mode
  - standby and wakeup
  - `countsToMicrovolts()`
- **Channel control:**
  - PGA gain and input mux per channel
  - channel power-down
  - 2.42 / 4.033 V reference
  - internal test signal
- **ECG features:**
  - lead-off detection: thresholds, currents, AC/DC, per-electrode flags in every sample
  - right-leg drive configuration
  - respiration frequency and phase configuration
  - offset calibration
- **On-chip measurements:** die temperature, AVDD and DVDD.
- **v4 breakout support:** CLK output for multi-board clock sync, and GPIO1/GPIO2 control.
- **Interrupt mode:** optional DRDY interrupt with a missed-sample counter.
- `ADS1292R_ELECTRODE_RA` / `_LA` / `_RL` lead-off constants, mapped from the v4 netlist.
- **Examples 01-10:** Serial Plotter, OpenView, heart and respiration rate, lead-off, self-test/noise, temperature/supply, interrupt-driven, multi-board sync, GPIO, diagnostics.
- **Repo tooling:** `keywords.txt`, `CHANGELOG.md`, Arduino Lint and a 9-board compile workflow, and `scripts/build.sh`.

### Changed
- **Heart-rate and respiration processing** moved to the optional `ADS1292R_Algorithms` class:
  - state is per-instance, no longer file-scope globals
  - coefficients are in PROGMEM on AVR
  - single-copy FIR buffers
  - ECG + respiration now fit on an Arduino Uno (about 1.2 KB RAM)
- **ECG resolution:** the ECG is DC-blocked in 32-bit at higher resolution (24-bit >> 4 instead of >> 10), so the filtered ECG amplitude is about 64x larger than in 1.x.
- **OpenView packet** now matches the OpenView `ads1292r` descriptor: an 8-byte payload in the order ECG, RESP, HR, RR. 1.x sent 9 bytes with RR before HR.
- **Initialisation is faster:** it uses datasheet timing instead of ~1.1 s of fixed delays plus a 2 s sketch delay, and each register write no longer takes ~8 ms.
- **Lead-off detection is on by default** for the ECG electrodes.
- **QRS detector windows shortened.** The envelope, maxima and refractory windows were 32/25/30 samples and are now 16/15/20.
  - Heart rate now tracks up to ~200 bpm. 1.x halved any rate above ~136 bpm.
  - Rates are rounded instead of truncated.
- **Respiration rate range** is now 6-187 breaths/min (was ~11-187).

### Fixed
- Lead-off was never reported: the 1.x init powered the lead-off comparators down.
- The respiration sample was assembled with 16-bit shifts on AVR and then wrapped to its low 16 bits.
- **Respiration rate:**
  - it read ~20% low (`6000/period` instead of `7500/period` at 125 SPS)
  - the falling-edge test was a copy of the rising-edge test
  - the amplitude window never reset, so one artefact disabled detection
  - a 16-bit truncation corrupted the smoothing sum
  - the start-up window counter was incremented twice per sample
- **QRS detector:** a possible divide-by-zero, and the FIR history off-by-one (wrapped at 160 of 161 taps).
- **Header pollution:** the 1.x headers leaked unprefixed global names (`i`, `j`, `Record`, `START`, `STOP`, `TRUE`, `FILTERORDER`…) and `RREG`/`WREG` macros with trailing semicolons.
- **Comments:** the register comments now state the actual values (gain 4/12, respiration phase 135°).

### Removed
- `protocentralAds1292r.h` / `ads1292r` class and `ecgRespirationAlgo.h` / `ecg_respiration_algorithm`.

## [1.1.0] - 2020

- Last release of the original API.
