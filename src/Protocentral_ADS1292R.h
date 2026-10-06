// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2017-2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// Arduino driver for the TI ADS1292R two-channel 24-bit ECG + respiration AFE,
// as used on the ProtoCentral ADS1292R Breakout (v4) and ADS1292R Shield.
//
// For research and education only - not a medical device.
//
// https://github.com/Protocentral/protocentral-ads1292r-arduino

#ifndef PROTOCENTRAL_ADS1292R_H
#define PROTOCENTRAL_ADS1292R_H

#include <Arduino.h>
#include <SPI.h>

#define ADS1292R_LIBRARY_VERSION "2.0.0"

// ============================================================================
// Compile-time configuration (override with -D or #define before #include)
// ============================================================================

#ifndef ADS1292R_SPI_CLOCK
#define ADS1292R_SPI_CLOCK 1000000UL  ///< SPI clock in Hz. 1 MHz is safe through the TXB0108 level shifter
#endif

#ifndef ADS1292R_POWERUP_MS
#define ADS1292R_POWERUP_MS 600       ///< Minimum ms after MCU boot before the first SPI access (tPOR = 2^18 tCLK = 512 ms)
#endif

#define ADS1292R_MAX_IRQ_INSTANCES 2  ///< Driver instances that may use DRDY interrupts at the same time (one ISR trampoline each)

// ============================================================================
// SPI opcodes (datasheet SBAS502, Table 13)
// ============================================================================

#define ADS1292R_CMD_WAKEUP    0x02  ///< Wake up from standby
#define ADS1292R_CMD_STANDBY   0x04  ///< Enter standby
#define ADS1292R_CMD_RESET     0x06  ///< Reset the device
#define ADS1292R_CMD_START     0x08  ///< Start or restart conversions
#define ADS1292R_CMD_STOP      0x0A  ///< Stop conversions
#define ADS1292R_CMD_OFFSETCAL 0x1A  ///< Channel offset calibration
#define ADS1292R_CMD_RDATAC    0x10  ///< Enable read-data-continuous mode
#define ADS1292R_CMD_SDATAC    0x11  ///< Stop read-data-continuous mode
#define ADS1292R_CMD_RDATA     0x12  ///< Read one sample by command
#define ADS1292R_CMD_RREG      0x20  ///< Read register (OR with address)
#define ADS1292R_CMD_WREG      0x40  ///< Write register (OR with address)

// ============================================================================
// Register map
// ============================================================================

#define ADS1292R_REG_ID        0x00
#define ADS1292R_REG_CONFIG1   0x01
#define ADS1292R_REG_CONFIG2   0x02
#define ADS1292R_REG_LOFF      0x03
#define ADS1292R_REG_CH1SET    0x04
#define ADS1292R_REG_CH2SET    0x05
#define ADS1292R_REG_RLD_SENS  0x06
#define ADS1292R_REG_LOFF_SENS 0x07
#define ADS1292R_REG_LOFF_STAT 0x08
#define ADS1292R_REG_RESP1     0x09
#define ADS1292R_REG_RESP2     0x0A
#define ADS1292R_REG_GPIO      0x0B
#define ADS1292R_NUM_REGS      12

#define ADS1292R_DEVICE_ID     0x73  ///< ID register value for the ADS1292R

// CONFIG1
#define ADS1292R_CONFIG1_SINGLE_SHOT  0x80
#define ADS1292R_CONFIG1_DR_MASK      0x07

// CONFIG2
#define ADS1292R_CONFIG2_FIXED        0x80  ///< Bit 7 must be 1
#define ADS1292R_CONFIG2_PDB_LOFF_COMP 0x40
#define ADS1292R_CONFIG2_PDB_REFBUF   0x20
#define ADS1292R_CONFIG2_VREF_4V      0x10
#define ADS1292R_CONFIG2_CLK_EN       0x08
#define ADS1292R_CONFIG2_INT_TEST     0x02
#define ADS1292R_CONFIG2_TEST_FREQ    0x01

// LOFF
#define ADS1292R_LOFF_FIXED           0x10  ///< Bit 4 must be 1
#define ADS1292R_LOFF_FLEAD_OFF       0x01

// CHnSET
#define ADS1292R_CHSET_PD             0x80
#define ADS1292R_CHSET_GAIN_MASK      0x70
#define ADS1292R_CHSET_MUX_MASK       0x0F

// RLD_SENS
#define ADS1292R_RLD_SENS_PDB_RLD     0x20
#define ADS1292R_RLD_SENS_LOFF_SENS   0x10

// RLD_SENS source bits / LOFF_SENS electrode bits / LOFF_STAT bits share one layout
#define ADS1292R_IN1P 0x01  ///< Channel 1 positive input
#define ADS1292R_IN1N 0x02  ///< Channel 1 negative input
#define ADS1292R_IN2P 0x04  ///< Channel 2 positive input
#define ADS1292R_IN2N 0x08  ///< Channel 2 negative input
#define ADS1292R_RLD  0x10  ///< RLD electrode (LOFF_STAT only)

// Electrode names for the ProtoCentral ADS1292R Breakout v4 (from its netlist):
// jack ring RA -> IN2P, jack tip LA -> IN2N, sleeve RL -> right-leg drive.
// CH2 therefore records RA - LA; negate it for conventional Lead I polarity.
#define ADS1292R_ELECTRODE_RA ADS1292R_IN2P  ///< Right-arm electrode lead-off flag
#define ADS1292R_ELECTRODE_LA ADS1292R_IN2N  ///< Left-arm electrode lead-off flag
#define ADS1292R_ELECTRODE_RL ADS1292R_RLD   ///< Right-leg electrode lead-off flag (needs RLD lead-off sensing)

// LOFF_STAT
#define ADS1292R_LOFF_STAT_CLK_DIV    0x40

// RESP1
#define ADS1292R_RESP1_DEMOD_EN       0x80
#define ADS1292R_RESP1_MOD_EN         0x40
#define ADS1292R_RESP1_FIXED          0x02  ///< Bit 1 must be 1
#define ADS1292R_RESP1_RESP_CTRL      0x01
#define ADS1292R_RESP_PHASE_DEFAULT   12    ///< 135 deg at 32 kHz - TI's recommended setting with gain 3-4

// RESP2
#define ADS1292R_RESP2_CALIB_ON       0x80
#define ADS1292R_RESP2_RESP_FREQ      0x04
#define ADS1292R_RESP2_RLDREF_INT     0x02
#define ADS1292R_RESP2_FIXED          0x01  ///< Bit 0 must be 1

// ============================================================================
// Enumerations (values are the register bit patterns)
// ============================================================================

/// Error codes reported by lastError().
enum class ADS1292R_Status : uint8_t {
    Ok = 0,           ///< No error
    NotInitialized,   ///< begin() has not succeeded yet
    SpiError,         ///< No response from the chip (ID read 0x00/0xFF or corrupt status word)
    WrongChipId,      ///< A device answered, but it is not an ADS1292R
    VerifyFailed,     ///< Register read-back did not match what was written
    InvalidArgument,  ///< Out-of-range parameter
    NotInterruptPin,  ///< DRDY pin cannot be used with attachInterrupt() on this board
    Timeout           ///< DRDY did not assert in time
};

/// Output data rate, shared by both channels (CONFIG1 DR[2:0]).
enum class ADS1292R_DataRate : uint8_t {
    SPS_125  = 0,
    SPS_250  = 1,
    SPS_500  = 2,
    SPS_1000 = 3,
    SPS_2000 = 4,
    SPS_4000 = 5,
    SPS_8000 = 6
};

/// Channel selector. On ProtoCentral boards CH1 carries respiration and CH2 carries ECG.
enum class ADS1292R_Channel : uint8_t {
    CH1  = 0,
    CH2  = 1,
    Resp = 0,  ///< Alias for CH1
    Ecg  = 1   ///< Alias for CH2
};

/// PGA gain (CHnSET GAIN[2:0]).
enum class ADS1292R_Gain : uint8_t {
    X6  = 0,
    X1  = 1,
    X2  = 2,
    X3  = 3,
    X4  = 4,
    X8  = 5,
    X12 = 6
};

/// Channel input multiplexer (CHnSET MUX[3:0]).
enum class ADS1292R_Mux : uint8_t {
    Normal      = 0x0,  ///< Electrode input
    Shorted     = 0x1,  ///< Inputs shorted - offset / noise measurement
    RldMeasure  = 0x2,  ///< RLD_MEASURE
    Supply      = 0x3,  ///< CH1: (AVDD+AVSS)/2, CH2: DVDD/4. Use gain 1
    Temperature = 0x4,  ///< On-die temperature sensor
    TestSignal  = 0x5,  ///< Internal test signal (see setTestSignal())
    RldDrp      = 0x6,  ///< Positive input connected to RLDIN
    RldDrm      = 0x7,  ///< Negative input connected to RLDIN
    RldDrpm     = 0x8,  ///< Both inputs connected to RLDIN
    In3         = 0x9   ///< Route IN3P/IN3N to this channel
};

/// Reference voltage (CONFIG2 VREF_4V). 4.033 V requires AVDD >= 4.4 V.
enum class ADS1292R_Reference : uint8_t {
    V2_42  = 0,
    V4_033 = 1
};

/// Internal test signal (CONFIG2 INT_TEST / TEST_FREQ). Amplitude is +/-VREF/2400.
enum class ADS1292R_TestSignal : uint8_t {
    Off       = 0x0,
    Dc        = 0x2,
    Square1Hz = 0x3
};

/// Lead-off comparator threshold, positive side (negative side is the mirror image).
enum class ADS1292R_LeadOffThreshold : uint8_t {
    Pct95   = 0,
    Pct92_5 = 1,
    Pct90   = 2,
    Pct87_5 = 3,
    Pct85   = 4,
    Pct80   = 5,
    Pct75   = 6,
    Pct70   = 7
};

/// Lead-off excitation current (LOFF ILEAD_OFF[1:0]).
enum class ADS1292R_LeadOffCurrent : uint8_t {
    nA_6  = 0,
    nA_22 = 1,
    uA_6  = 2,
    uA_22 = 3
};

/// Respiration modulation frequency (RESP2 RESP_FREQ).
enum class ADS1292R_RespFreq : uint8_t {
    KHz_32 = 0,
    KHz_64 = 1
};

// ============================================================================
// Data types
// ============================================================================

/// One conversion result.
struct ADS1292R_Sample {
    int32_t resp;     ///< Channel 1 (respiration on ProtoCentral boards), signed 24-bit counts
    int32_t ecg;      ///< Channel 2 (ECG), signed 24-bit counts
    uint8_t leadOff;  ///< Lead-off flags: ADS1292R_IN1P | IN1N | IN2P | IN2N | RLD. 0 = all connected
    uint8_t gpio;     ///< GPIO2:GPIO1 pin levels at conversion time
};

/// Full device configuration applied by begin(). Defaults reproduce the
/// ProtoCentral ECG + respiration profile (125 SPS, respiration on CH1 at
/// gain 4 / 32 kHz / 135 deg, ECG on CH2 at gain 12, RLD from CH2) with DC
/// lead-off detection on the ECG electrodes.
struct ADS1292R_Config {
    ADS1292R_DataRate dataRate = ADS1292R_DataRate::SPS_125;

    ADS1292R_Gain ch1Gain = ADS1292R_Gain::X4;
    ADS1292R_Mux  ch1Mux  = ADS1292R_Mux::Normal;
    ADS1292R_Gain ch2Gain = ADS1292R_Gain::X12;
    ADS1292R_Mux  ch2Mux  = ADS1292R_Mux::Normal;

    ADS1292R_Reference  reference  = ADS1292R_Reference::V2_42;
    ADS1292R_TestSignal testSignal = ADS1292R_TestSignal::Off;

    bool              respiration       = true;
    ADS1292R_RespFreq respFreq          = ADS1292R_RespFreq::KHz_32;
    uint8_t           respPhase         = ADS1292R_RESP_PHASE_DEFAULT;  ///< 0-15 (32 kHz, 11.25 deg steps) or 0-7 (64 kHz, 22.5 deg steps)
    bool              respExternalClock = false;  ///< RESP_CTRL: modulation clock on GPIO1/GPIO2 (disables GPIO use)

    bool                      leadOff          = true;
    ADS1292R_LeadOffThreshold leadOffThreshold = ADS1292R_LeadOffThreshold::Pct95;
    ADS1292R_LeadOffCurrent   leadOffCurrent   = ADS1292R_LeadOffCurrent::nA_6;
    uint8_t                   leadOffSense     = ADS1292R_IN2P | ADS1292R_IN2N;  ///< Electrodes to monitor
    bool                      leadOffAC        = false;

    bool    rld          = true;
    uint8_t rldSources   = ADS1292R_IN2P | ADS1292R_IN2N;  ///< Inputs that drive the RLD amplifier
    bool    rldLeadOff   = false;  ///< Also sense whether the RLD electrode is connected

    bool clockOutput = false;  ///< CLK_EN: drive the internal oscillator out on CLK (multi-board master)
    bool autoStart   = true;   ///< Enter RDATAC and start conversions at the end of begin()
};

// ============================================================================
// Driver class
// ============================================================================

class Protocentral_ADS1292R {
public:
    /**
     * @param csPin     Chip select
     * @param drdyPin   Data ready (active low)
     * @param startPin  START pin, or -1 if not wired (START/STOP opcodes are used; tie START low)
     * @param resetPin  PWDN/RESET pin (labelled RST on the v4 breakout), or -1 to use the RESET opcode
     * @param spi       SPI bus
     */
    Protocentral_ADS1292R(uint8_t csPin, uint8_t drdyPin, int8_t startPin = -1, int8_t resetPin = -1,
                          SPIClass& spi = SPI);

    // ---- Initialisation ----------------------------------------------------

    /** @brief Reset the chip, check its ID and apply the default ECG + respiration profile. */
    bool begin(bool startSPI = true);

    /** @brief Reset the chip, check its ID and apply @p config. */
    bool begin(const ADS1292R_Config& config, bool startSPI = true);

#if defined(ARDUINO_ARCH_ESP32)
    /** @brief ESP32 only: start SPI on custom pins, then begin(config). */
    bool begin(int8_t sck, int8_t miso, int8_t mosi, const ADS1292R_Config& config = ADS1292R_Config());
#endif

    /** @brief Stop conversions, put the chip in standby and release the DRDY interrupt. */
    void end();

    bool    isConnected();
    uint8_t readID();
    ADS1292R_Status lastError() const { return _lastError; }
    static const char* getLibraryVersion() { return ADS1292R_LIBRARY_VERSION; }

    // ---- Conversion control ------------------------------------------------

    void start();    ///< Start (or restart) conversions; in single-shot mode, trigger one conversion
    void stop();     ///< Stop conversions
    void standby();  ///< Low-power standby (registers retained)
    void wakeup();   ///< Leave standby

    bool setDataRate(ADS1292R_DataRate rate);
    ADS1292R_DataRate getDataRate() const;
    uint16_t getSampleRate() const;  ///< Samples per second
    bool setSingleShot(bool enable);

    // ---- Reading data ------------------------------------------------------

    /** @brief True when a new sample is waiting (DRDY low, or the DRDY interrupt fired). */
    bool available();

    /** @brief Read the latest sample. Call when available() is true. */
    bool read(ADS1292R_Sample& sample);

    /** @brief Latch DRDY with an interrupt instead of polling. Fails if the pin has no interrupt. */
    bool enableDataReadyInterrupt();
    void disableDataReadyInterrupt();

    /** @brief Number of samples that arrived before the previous one was read (interrupt mode only). */
    uint32_t missedSamples() const { return _missed; }

    // ---- Channels, reference and test signal -------------------------------

    bool setGain(ADS1292R_Channel ch, ADS1292R_Gain gain);
    ADS1292R_Gain getGain(ADS1292R_Channel ch) const;
    bool setInputMux(ADS1292R_Channel ch, ADS1292R_Mux mux);
    bool setChannelPowerDown(ADS1292R_Channel ch, bool powerDown);
    bool setReference(ADS1292R_Reference ref);
    bool setTestSignal(ADS1292R_TestSignal signal);

    // ---- Lead-off detection ------------------------------------------------

    /**
     * @brief Configure lead-off detection.
     * @param enable     Power the comparators and excitation current
     * @param sense      Electrodes to monitor (ADS1292R_IN1P | IN1N | IN2P | IN2N)
     * @param threshold  Comparator threshold
     * @param current    Excitation current
     * @param ac         AC lead-off at fDR/4 instead of DC
     */
    bool configureLeadOff(bool enable, uint8_t sense = ADS1292R_IN2P | ADS1292R_IN2N,
                          ADS1292R_LeadOffThreshold threshold = ADS1292R_LeadOffThreshold::Pct95,
                          ADS1292R_LeadOffCurrent current = ADS1292R_LeadOffCurrent::nA_6, bool ac = false);

    /** @brief Read the LOFF_STAT register directly (the same flags arrive with every sample). */
    uint8_t readLeadOffStatus();

    // ---- Respiration -------------------------------------------------------

    /**
     * @brief Configure the on-chip respiration impedance circuit on CH1.
     * @param phase  Demodulation phase code (see ADS1292R_Config::respPhase)
     */
    bool configureRespiration(bool enable, ADS1292R_RespFreq freq = ADS1292R_RespFreq::KHz_32,
                              uint8_t phase = ADS1292R_RESP_PHASE_DEFAULT);

    /** @brief Cancel channel offsets (OFFSETCAL). Re-runs automatically after setGain() once used. */
    bool runOffsetCalibration();

    // ---- Right-leg drive ---------------------------------------------------

    bool configureRLD(bool enable, uint8_t sources = ADS1292R_IN2P | ADS1292R_IN2N, bool senseLeadOff = false);

    // ---- Clock and GPIO (v4 breakout: CLK on header, GPIO1/2 on TP3) -------

    /** @brief Drive the internal oscillator out on CLK, to clock other boards (multi-board sync). */
    bool setClockOutput(bool enable);

    /** @brief Set GPIO1 or GPIO2 as output (true) or input (false). */
    bool setGPIOMode(uint8_t gpio, bool output);
    bool writeGPIO(uint8_t gpio, bool level);
    int  readGPIO(uint8_t gpio);  ///< 0/1, or -1 on error

    // ---- Units and on-chip measurements ------------------------------------

    /** @brief Convert counts to microvolts at the channel's current gain and reference. */
    float countsToMicrovolts(int32_t counts, ADS1292R_Channel ch) const;

    /** @brief On-die temperature in deg C. Briefly borrows CH1 (blocks ~100 ms at 125 SPS). */
    float readTemperature();

    /** @brief Analog supply AVDD in volts, measured on CH1. Briefly borrows CH1. */
    float readAnalogSupply();

    /** @brief Digital supply DVDD in volts, measured on CH2. Briefly borrows CH2. */
    float readDigitalSupply();

    // ---- Raw register access -----------------------------------------------

    uint8_t readRegister(uint8_t reg);
    bool    writeRegister(uint8_t reg, uint8_t value);
    void    printRegisters(Stream& out = Serial);

private:
    uint8_t   _csPin;
    uint8_t   _drdyPin;
    int8_t    _startPin;
    int8_t    _resetPin;
    SPIClass* _spi;
    SPISettings _spiSettings;

    uint8_t _regs[ADS1292R_NUM_REGS];  // shadow copy of the register file
    bool    _rdatac;                   // chip is in read-data-continuous mode
    bool    _running;                  // conversions started
    bool    _offsetCal;                // offset calibration in use - re-run on gain change
    bool    _initialized;
    ADS1292R_Status _lastError;

    volatile bool     _drdyFlag;
    volatile uint32_t _missed;
    int8_t            _irqSlot;

    void    sendCommand(uint8_t cmd);
    uint8_t rawReadRegister(uint8_t reg);
    void    rawWriteRegister(uint8_t reg, uint8_t value);
    bool    updateRegister(uint8_t reg, uint8_t value);
    bool    applyConfig(const ADS1292R_Config& config);
    void    hardwareReset();
    void    enterRDATAC();
    void    exitRDATAC();
    void    csLow();
    void    csHigh();
    bool    waitForData(uint32_t timeoutMs);
    bool    readFrame(ADS1292R_Sample& sample);
    bool    measureInternal(ADS1292R_Channel ch, ADS1292R_Mux mux, ADS1292R_Gain gain, float& microvolts);
    float   referenceVolts() const;
    uint8_t chsetRegister(ADS1292R_Channel ch) const;

    static uint8_t maskRegister(uint8_t reg, uint8_t value);
    static uint8_t gainValue(ADS1292R_Gain gain);

    static Protocentral_ADS1292R* _irqInstances[ADS1292R_MAX_IRQ_INSTANCES];
    static void isr0();
    static void isr1();
    void handleDataReady();
};

#endif  // PROTOCENTRAL_ADS1292R_H
