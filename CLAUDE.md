# CLAUDE.md

Guidance for working on the ProtoCentral ADS1292R Arduino library.

## Layout

- `src/Protocentral_ADS1292R.{h,cpp}`: the driver. The register map, all `ADS1292R_*` macros and the enums live in the header.
- `src/Protocentral_ADS1292R_Algorithms.{h,cpp}`: optional heart-rate and respiration-rate processing. The driver header must **not** include it, so sketches that do not use it pay nothing.
- `examples/NN.Name/NN.Name.ino`: numbered examples. The `.ino` basename must match the folder (an Arduino requirement).

## Build

There is no host-side test harness. Run `scripts/build.sh [fqbn]`, which uses arduino-cli with `--library` pointing at this checkout and compiles every example on the CI board list.

CI runs `arduino-lint` in `update` mode with `specification` compliance (the library is already in the Library Manager index; `strict` would fail on the legacy `name=`) and the same 9-board compile matrix. Each matrix entry carries its **whole** `platforms:` block: emitting an empty `source-url:` for an Arduino-official core crashes `compile-sketches`.

Hardware validation still requires a physical board. Run `10.Diagnostics` and `05.TestSignalNoise` first.

## Rules

- **Keep three places in sync** when changing the public API: the headers, `keywords.txt`, and the API tables in `README.md`. The version lives in `library.properties` and `ADS1292R_LIBRARY_VERSION`.
- **The Library Manager `name=`** in `library.properties` must never change, or existing installs stop receiving updates.
- **The OpenView wire format** in `02.OpenView` is shared with the `ads1292r` descriptor in `protocentral_openview` (`lib/boards/descriptors/ads1292r.dart`): 57600 baud, pktType 2, 8-byte payload ECG/RESP/HR/RR as int16 LE, 125 Hz. Change one side only together with the other, in the same release. OpenView sketches print nothing but packets.
- **RDATAC/SDATAC.** Registers cannot be read or written in RDATAC mode, and other opcodes are ignored there.
  - All register access goes through `updateRegister()` / `readRegister()`, which drop out of RDATAC and resume.
  - Never call `rawReadRegister()` / `rawWriteRegister()` / `sendCommand()` from a public method without the same wrapping.
- **Reserved bits** are forced by `maskRegister()`. Keep that table aligned with the datasheet (SBAS502, Table 14).
- **Default register values** reproduce the 1.x profile (CH1 resp gain 4, 32 kHz, 135°; CH2 ECG gain 12; RLD from CH2), plus DC lead-off on CH2.
  - Do not change the defaults without re-verifying on a real v4 board.
  - v4 changed the respiration input network, and the respiration amplitude depends on gain and phase.
  - v4 netlist: RA (jack ring) goes through 51 kΩ to IN2P, LA (tip) through 51 kΩ to IN2N, and RL (sleeve) to RLDINV/RLDOUT. CH1 is AC-coupled from LA/RA (100 nF) and biased by 10 MΩ dividers.
  - So CH2 = RA − LA, which is inverted Lead I. Lead-off on CH2 is not affected by the 10 MΩ resistors on IN1.
- **The algorithms are tuned for 125 SPS.** The FIR coefficients and all timing constants assume it.
  - `RESP_MIN_AMPLITUDE` is in detector units, which are 32x the mean of the DC-blocked respiration counts.
  - The QRS windows (16-sample envelope, 15 + 20 maxima/refractory) were tuned on synthetic ECG to track 40-200 bpm.
  - Very tall T waves (over ~0.6 of R) can still double-count.
  - Re-validate on a subject or simulator after touching any constant.
- **Library code must not print.** Only `printRegisters()` writes to a `Stream`, and only when asked.
- **Portability.** `architectures=*`, so guard platform-specific code (`ARDUINO_ARCH_ESP32`, `__AVR__`). Every example must keep fitting on an Uno.
