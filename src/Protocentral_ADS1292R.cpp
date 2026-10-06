// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2017-2026 Ashwin Whitchurch, ProtoCentral Electronics
//
// Arduino driver for the TI ADS1292R two-channel 24-bit ECG + respiration AFE.
//
// https://github.com/Protocentral/protocentral-ads1292r-arduino

#include "Protocentral_ADS1292R.h"

#if defined(ARDUINO_ARCH_ESP32) || defined(ARDUINO_ARCH_ESP8266)
#define ADS1292R_ISR_ATTR IRAM_ATTR
#else
#define ADS1292R_ISR_ATTR
#endif

// Timing, from SBAS502 with fCLK = 512 kHz (tCLK = 1.95 us)
static const uint16_t TCLK4_US        = 8;    // 4 tCLK: command decode time and last SCLK -> CS high
static const uint16_t RESET_PULSE_US  = 50;   // PWDN/RESET low time: long enough to reset, too short to power down
static const uint16_t RESET_WAIT_US   = 100;  // 18 tCLK for the register file to reload, with margin
static const uint8_t  SETTLE_SAMPLES  = 4;    // settled data arrive on the 4th DRDY after a change
static const uint8_t  AVERAGE_SAMPLES = 8;    // samples averaged by the internal measurements

Protocentral_ADS1292R* Protocentral_ADS1292R::_irqInstances[ADS1292R_MAX_IRQ_INSTANCES] = {nullptr, nullptr};

Protocentral_ADS1292R::Protocentral_ADS1292R(uint8_t csPin, uint8_t drdyPin, int8_t startPin, int8_t resetPin,
                                             SPIClass& spi)
    : _csPin(csPin),
      _drdyPin(drdyPin),
      _startPin(startPin),
      _resetPin(resetPin),
      _spi(&spi),
      _spiSettings(ADS1292R_SPI_CLOCK, MSBFIRST, SPI_MODE1),
      _rdatac(false),
      _running(false),
      _offsetCal(false),
      _initialized(false),
      _lastError(ADS1292R_Status::NotInitialized),
      _drdyFlag(false),
      _missed(0),
      _irqSlot(-1) {
    memset(_regs, 0, sizeof(_regs));
}

// ============================================================================
// Initialisation
// ============================================================================

bool Protocentral_ADS1292R::begin(bool startSPI) {
    ADS1292R_Config config;
    return begin(config, startSPI);
}

bool Protocentral_ADS1292R::begin(const ADS1292R_Config& config, bool startSPI) {
    _initialized = false;

    pinMode(_csPin, OUTPUT);
    digitalWrite(_csPin, HIGH);
    pinMode(_drdyPin, INPUT);
    if (_startPin >= 0) {
        pinMode(_startPin, OUTPUT);
        digitalWrite(_startPin, LOW);  // hold conversions off while configuring
    }
    if (_resetPin >= 0) {
        pinMode(_resetPin, OUTPUT);
        digitalWrite(_resetPin, HIGH);
    }

    if (startSPI) {
        _spi->begin();
    }

    // The chip ignores SPI until its power-on reset completes
    uint32_t now = millis();
    if (now < ADS1292R_POWERUP_MS) {
        delay(ADS1292R_POWERUP_MS - now);
    }

    hardwareReset();
    exitRDATAC();
    _running = false;

    uint8_t id = rawReadRegister(ADS1292R_REG_ID);
    if (id == 0x00 || id == 0xFF) {
        _lastError = ADS1292R_Status::SpiError;
        return false;
    }
    if (id != ADS1292R_DEVICE_ID) {
        _lastError = ADS1292R_Status::WrongChipId;
        return false;
    }

    for (uint8_t reg = 0; reg < ADS1292R_NUM_REGS; reg++) {
        _regs[reg] = rawReadRegister(reg);
    }

    if (!applyConfig(config)) {
        return false;  // _lastError set by updateRegister()
    }

    _offsetCal   = false;
    _initialized = true;
    _lastError   = ADS1292R_Status::Ok;

    if (config.autoStart) {
        enterRDATAC();
        start();
    }
    return true;
}

#if defined(ARDUINO_ARCH_ESP32)
bool Protocentral_ADS1292R::begin(int8_t sck, int8_t miso, int8_t mosi, const ADS1292R_Config& config) {
    _spi->begin(sck, miso, mosi, -1);
    return begin(config, false);
}
#endif

void Protocentral_ADS1292R::end() {
    disableDataReadyInterrupt();
    stop();
    exitRDATAC();
    sendCommand(ADS1292R_CMD_STANDBY);
    _initialized = false;
    _lastError   = ADS1292R_Status::NotInitialized;
}

bool Protocentral_ADS1292R::isConnected() {
    return readID() == ADS1292R_DEVICE_ID;
}

uint8_t Protocentral_ADS1292R::readID() {
    return readRegister(ADS1292R_REG_ID);
}

bool Protocentral_ADS1292R::applyConfig(const ADS1292R_Config& c) {
    uint8_t config1 = static_cast<uint8_t>(c.dataRate);

    uint8_t config2 = ADS1292R_CONFIG2_FIXED | ADS1292R_CONFIG2_PDB_REFBUF | static_cast<uint8_t>(c.testSignal);
    if (c.leadOff) config2 |= ADS1292R_CONFIG2_PDB_LOFF_COMP;
    if (c.reference == ADS1292R_Reference::V4_033) config2 |= ADS1292R_CONFIG2_VREF_4V;
    if (c.clockOutput) config2 |= ADS1292R_CONFIG2_CLK_EN;

    uint8_t loff = (static_cast<uint8_t>(c.leadOffThreshold) << 5) | ADS1292R_LOFF_FIXED |
                   (static_cast<uint8_t>(c.leadOffCurrent) << 2) | (c.leadOffAC ? ADS1292R_LOFF_FLEAD_OFF : 0);

    uint8_t ch1set = (static_cast<uint8_t>(c.ch1Gain) << 4) | static_cast<uint8_t>(c.ch1Mux);
    uint8_t ch2set = (static_cast<uint8_t>(c.ch2Gain) << 4) | static_cast<uint8_t>(c.ch2Mux);

    uint8_t rldSens = (c.rldSources & 0x0F);
    if (c.rld) rldSens |= ADS1292R_RLD_SENS_PDB_RLD;
    if (c.rldLeadOff) rldSens |= ADS1292R_RLD_SENS_LOFF_SENS;

    uint8_t loffSens = c.leadOff ? (c.leadOffSense & 0x0F) : 0;

    uint8_t maxPhase = (c.respFreq == ADS1292R_RespFreq::KHz_64) ? 7 : 15;
    if (c.respPhase > maxPhase) {
        _lastError = ADS1292R_Status::InvalidArgument;
        return false;
    }
    uint8_t resp1 = ADS1292R_RESP1_FIXED | (c.respExternalClock ? ADS1292R_RESP1_RESP_CTRL : 0);
    if (c.respiration) resp1 |= ADS1292R_RESP1_DEMOD_EN | ADS1292R_RESP1_MOD_EN | (c.respPhase << 2);

    uint8_t resp2 = ADS1292R_RESP2_FIXED | ADS1292R_RESP2_RLDREF_INT;
    if (c.respFreq == ADS1292R_RespFreq::KHz_64) resp2 |= ADS1292R_RESP2_RESP_FREQ;

    return updateRegister(ADS1292R_REG_CONFIG1, config1) &&
           updateRegister(ADS1292R_REG_CONFIG2, config2) &&
           updateRegister(ADS1292R_REG_LOFF, loff) &&
           updateRegister(ADS1292R_REG_CH1SET, ch1set) &&
           updateRegister(ADS1292R_REG_CH2SET, ch2set) &&
           updateRegister(ADS1292R_REG_RLD_SENS, rldSens) &&
           updateRegister(ADS1292R_REG_LOFF_SENS, loffSens) &&
           updateRegister(ADS1292R_REG_LOFF_STAT, 0x00) &&
           updateRegister(ADS1292R_REG_RESP1, resp1) &&
           updateRegister(ADS1292R_REG_RESP2, resp2) &&
           updateRegister(ADS1292R_REG_GPIO, 0x0C);  // both GPIOs as inputs (reset default)
}

// ============================================================================
// Conversion control
// ============================================================================

void Protocentral_ADS1292R::start() {
    if (_startPin >= 0) {
        // A low-high edge also re-arms single-shot mode
        digitalWrite(_startPin, LOW);
        delayMicroseconds(TCLK4_US);
        digitalWrite(_startPin, HIGH);
    } else {
        bool wasRdatac = _rdatac;
        exitRDATAC();
        sendCommand(ADS1292R_CMD_START);
        if (wasRdatac) enterRDATAC();
    }
    _running = true;
}

void Protocentral_ADS1292R::stop() {
    if (_startPin >= 0) {
        digitalWrite(_startPin, LOW);
    } else {
        bool wasRdatac = _rdatac;
        exitRDATAC();
        sendCommand(ADS1292R_CMD_STOP);
        if (wasRdatac) enterRDATAC();
    }
    _running = false;
}

void Protocentral_ADS1292R::standby() {
    bool wasRdatac = _rdatac;
    exitRDATAC();
    sendCommand(ADS1292R_CMD_STANDBY);
    if (wasRdatac) enterRDATAC();
}

void Protocentral_ADS1292R::wakeup() {
    bool wasRdatac = _rdatac;
    exitRDATAC();
    sendCommand(ADS1292R_CMD_WAKEUP);
    if (wasRdatac) enterRDATAC();
}

bool Protocentral_ADS1292R::setDataRate(ADS1292R_DataRate rate) {
    uint8_t v = (_regs[ADS1292R_REG_CONFIG1] & ~ADS1292R_CONFIG1_DR_MASK) | static_cast<uint8_t>(rate);
    return updateRegister(ADS1292R_REG_CONFIG1, v);
}

ADS1292R_DataRate Protocentral_ADS1292R::getDataRate() const {
    return static_cast<ADS1292R_DataRate>(_regs[ADS1292R_REG_CONFIG1] & ADS1292R_CONFIG1_DR_MASK);
}

uint16_t Protocentral_ADS1292R::getSampleRate() const {
    return 125U << (_regs[ADS1292R_REG_CONFIG1] & ADS1292R_CONFIG1_DR_MASK);
}

bool Protocentral_ADS1292R::setSingleShot(bool enable) {
    uint8_t v = _regs[ADS1292R_REG_CONFIG1];
    v = enable ? (v | ADS1292R_CONFIG1_SINGLE_SHOT) : (v & ~ADS1292R_CONFIG1_SINGLE_SHOT);
    return updateRegister(ADS1292R_REG_CONFIG1, v);
}

// ============================================================================
// Reading data
// ============================================================================

bool Protocentral_ADS1292R::available() {
    if (!_initialized) return false;
    if (_irqSlot >= 0) return _drdyFlag;
    return digitalRead(_drdyPin) == LOW;
}

bool Protocentral_ADS1292R::read(ADS1292R_Sample& sample) {
    if (!_initialized) {
        _lastError = ADS1292R_Status::NotInitialized;
        return false;
    }
    if (_irqSlot >= 0) {
        noInterrupts();
        _drdyFlag = false;
        interrupts();
    }
    return readFrame(sample);
}

static int32_t signExtend24(uint8_t b0, uint8_t b1, uint8_t b2) {
    uint32_t u = ((uint32_t)b0 << 16) | ((uint32_t)b1 << 8) | b2;
    if (u & 0x800000UL) u |= 0xFF000000UL;
    return (int32_t)u;
}

bool Protocentral_ADS1292R::readFrame(ADS1292R_Sample& sample) {
    uint8_t buf[9];

    _spi->beginTransaction(_spiSettings);
    csLow();
    if (!_rdatac) {
        _spi->transfer(ADS1292R_CMD_RDATA);
    }
    for (uint8_t i = 0; i < sizeof(buf); i++) {
        buf[i] = _spi->transfer(0x00);
    }
    delayMicroseconds(TCLK4_US);
    csHigh();
    _spi->endTransaction();

    // Status word: 1100 | LOFF_STAT[4:0] | GPIO[1:0] | 13 x 0
    uint32_t status = ((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | buf[2];
    if ((status & 0xF00000UL) != 0xC00000UL) {
        _lastError = ADS1292R_Status::SpiError;
        return false;
    }

    // LOFF_STAT bits are only meaningful for electrodes that are being sensed
    uint8_t loffMask = 0;
    if (_regs[ADS1292R_REG_CONFIG2] & ADS1292R_CONFIG2_PDB_LOFF_COMP) {
        loffMask = _regs[ADS1292R_REG_LOFF_SENS] & 0x0F;
        if (_regs[ADS1292R_REG_RLD_SENS] & ADS1292R_RLD_SENS_LOFF_SENS) loffMask |= ADS1292R_RLD;
    }

    sample.leadOff = (uint8_t)((status >> 15) & 0x1F) & loffMask;
    sample.gpio    = (uint8_t)((status >> 13) & 0x03);
    sample.resp    = signExtend24(buf[3], buf[4], buf[5]);
    sample.ecg     = signExtend24(buf[6], buf[7], buf[8]);
    return true;
}

bool Protocentral_ADS1292R::waitForData(uint32_t timeoutMs) {
    uint32_t t0 = millis();
    while (digitalRead(_drdyPin) != LOW) {
        if (millis() - t0 > timeoutMs) {
            _lastError = ADS1292R_Status::Timeout;
            return false;
        }
    }
    return true;
}

void ADS1292R_ISR_ATTR Protocentral_ADS1292R::isr0() {
    Protocentral_ADS1292R* self = _irqInstances[0];
    if (self) {
        if (self->_drdyFlag) self->_missed = self->_missed + 1;
        self->_drdyFlag = true;
    }
}

void ADS1292R_ISR_ATTR Protocentral_ADS1292R::isr1() {
    Protocentral_ADS1292R* self = _irqInstances[1];
    if (self) {
        if (self->_drdyFlag) self->_missed = self->_missed + 1;
        self->_drdyFlag = true;
    }
}

bool Protocentral_ADS1292R::enableDataReadyInterrupt() {
    if (_irqSlot >= 0) return true;

    int irq = digitalPinToInterrupt(_drdyPin);
#ifdef NOT_AN_INTERRUPT
    if (irq == NOT_AN_INTERRUPT) {
        _lastError = ADS1292R_Status::NotInterruptPin;
        return false;
    }
#endif
    if (irq < 0) {
        _lastError = ADS1292R_Status::NotInterruptPin;
        return false;
    }

    int8_t slot = -1;
    for (uint8_t i = 0; i < ADS1292R_MAX_IRQ_INSTANCES; i++) {
        if (_irqInstances[i] == nullptr) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        _lastError = ADS1292R_Status::InvalidArgument;
        return false;
    }

    _irqInstances[slot] = this;
    _irqSlot  = slot;
    _drdyFlag = (digitalRead(_drdyPin) == LOW);
    _missed   = 0;
    attachInterrupt(irq, slot == 0 ? isr0 : isr1, FALLING);
    return true;
}

void Protocentral_ADS1292R::disableDataReadyInterrupt() {
    if (_irqSlot < 0) return;
    detachInterrupt(digitalPinToInterrupt(_drdyPin));
    _irqInstances[_irqSlot] = nullptr;
    _irqSlot  = -1;
    _drdyFlag = false;
}

// ============================================================================
// Channels, reference and test signal
// ============================================================================

uint8_t Protocentral_ADS1292R::chsetRegister(ADS1292R_Channel ch) const {
    return ch == ADS1292R_Channel::CH1 ? ADS1292R_REG_CH1SET : ADS1292R_REG_CH2SET;
}

bool Protocentral_ADS1292R::setGain(ADS1292R_Channel ch, ADS1292R_Gain gain) {
    uint8_t reg = chsetRegister(ch);
    uint8_t v   = (_regs[reg] & ~ADS1292R_CHSET_GAIN_MASK) | (static_cast<uint8_t>(gain) << 4);
    if (!updateRegister(reg, v)) return false;
    // The datasheet requires OFFSETCAL after every gain change once calibration is in use
    return _offsetCal ? runOffsetCalibration() : true;
}

ADS1292R_Gain Protocentral_ADS1292R::getGain(ADS1292R_Channel ch) const {
    return static_cast<ADS1292R_Gain>((_regs[chsetRegister(ch)] & ADS1292R_CHSET_GAIN_MASK) >> 4);
}

bool Protocentral_ADS1292R::setInputMux(ADS1292R_Channel ch, ADS1292R_Mux mux) {
    uint8_t reg = chsetRegister(ch);
    uint8_t v   = (_regs[reg] & ~ADS1292R_CHSET_MUX_MASK) | static_cast<uint8_t>(mux);
    return updateRegister(reg, v);
}

bool Protocentral_ADS1292R::setChannelPowerDown(ADS1292R_Channel ch, bool powerDown) {
    uint8_t reg = chsetRegister(ch);
    uint8_t v   = _regs[reg] & ~(ADS1292R_CHSET_PD | ADS1292R_CHSET_MUX_MASK);
    // A powered-down channel must have its inputs shorted; powering up restores the electrode input
    v |= powerDown ? (ADS1292R_CHSET_PD | static_cast<uint8_t>(ADS1292R_Mux::Shorted))
                   : static_cast<uint8_t>(ADS1292R_Mux::Normal);
    return updateRegister(reg, v);
}

bool Protocentral_ADS1292R::setReference(ADS1292R_Reference ref) {
    uint8_t v = _regs[ADS1292R_REG_CONFIG2] & ~ADS1292R_CONFIG2_VREF_4V;
    if (ref == ADS1292R_Reference::V4_033) v |= ADS1292R_CONFIG2_VREF_4V;
    return updateRegister(ADS1292R_REG_CONFIG2, v);
}

bool Protocentral_ADS1292R::setTestSignal(ADS1292R_TestSignal signal) {
    uint8_t v = (_regs[ADS1292R_REG_CONFIG2] & ~(ADS1292R_CONFIG2_INT_TEST | ADS1292R_CONFIG2_TEST_FREQ)) |
                static_cast<uint8_t>(signal);
    return updateRegister(ADS1292R_REG_CONFIG2, v);
}

// ============================================================================
// Lead-off, respiration and RLD
// ============================================================================

bool Protocentral_ADS1292R::configureLeadOff(bool enable, uint8_t sense, ADS1292R_LeadOffThreshold threshold,
                                             ADS1292R_LeadOffCurrent current, bool ac) {
    uint8_t loff = (static_cast<uint8_t>(threshold) << 5) | ADS1292R_LOFF_FIXED |
                   (static_cast<uint8_t>(current) << 2) | (ac ? ADS1292R_LOFF_FLEAD_OFF : 0);

    uint8_t config2 = _regs[ADS1292R_REG_CONFIG2] & ~ADS1292R_CONFIG2_PDB_LOFF_COMP;
    if (enable) config2 |= ADS1292R_CONFIG2_PDB_LOFF_COMP;

    return updateRegister(ADS1292R_REG_LOFF, loff) &&
           updateRegister(ADS1292R_REG_LOFF_SENS, enable ? (sense & 0x0F) : 0) &&
           updateRegister(ADS1292R_REG_CONFIG2, config2);
}

uint8_t Protocentral_ADS1292R::readLeadOffStatus() {
    if (!(_regs[ADS1292R_REG_CONFIG2] & ADS1292R_CONFIG2_PDB_LOFF_COMP)) return 0;
    uint8_t mask = _regs[ADS1292R_REG_LOFF_SENS] & 0x0F;
    if (_regs[ADS1292R_REG_RLD_SENS] & ADS1292R_RLD_SENS_LOFF_SENS) mask |= ADS1292R_RLD;
    return readRegister(ADS1292R_REG_LOFF_STAT) & mask;
}

bool Protocentral_ADS1292R::configureRespiration(bool enable, ADS1292R_RespFreq freq, uint8_t phase) {
    uint8_t maxPhase = (freq == ADS1292R_RespFreq::KHz_64) ? 7 : 15;
    if (phase > maxPhase) {
        _lastError = ADS1292R_Status::InvalidArgument;
        return false;
    }

    uint8_t resp1 = ADS1292R_RESP1_FIXED | (_regs[ADS1292R_REG_RESP1] & ADS1292R_RESP1_RESP_CTRL);
    if (enable) resp1 |= ADS1292R_RESP1_DEMOD_EN | ADS1292R_RESP1_MOD_EN | (phase << 2);

    uint8_t resp2 = _regs[ADS1292R_REG_RESP2] & ~ADS1292R_RESP2_RESP_FREQ;
    if (freq == ADS1292R_RespFreq::KHz_64) resp2 |= ADS1292R_RESP2_RESP_FREQ;

    return updateRegister(ADS1292R_REG_RESP1, resp1) && updateRegister(ADS1292R_REG_RESP2, resp2);
}

bool Protocentral_ADS1292R::runOffsetCalibration() {
    if (!updateRegister(ADS1292R_REG_RESP2, _regs[ADS1292R_REG_RESP2] | ADS1292R_RESP2_CALIB_ON)) {
        return false;
    }
    bool wasRdatac = _rdatac;
    exitRDATAC();
    sendCommand(ADS1292R_CMD_OFFSETCAL);
    // Allow a few conversion periods for the calibration to complete
    delay(4000U / getSampleRate() + 5);
    if (wasRdatac) enterRDATAC();
    _offsetCal = true;
    return true;
}

bool Protocentral_ADS1292R::configureRLD(bool enable, uint8_t sources, bool senseLeadOff) {
    uint8_t v = (_regs[ADS1292R_REG_RLD_SENS] & 0xC0) | (sources & 0x0F);  // keep CHOP bits
    if (enable) v |= ADS1292R_RLD_SENS_PDB_RLD;
    if (senseLeadOff) v |= ADS1292R_RLD_SENS_LOFF_SENS;
    return updateRegister(ADS1292R_REG_RLD_SENS, v);
}

// ============================================================================
// Clock and GPIO
// ============================================================================

bool Protocentral_ADS1292R::setClockOutput(bool enable) {
    uint8_t v = _regs[ADS1292R_REG_CONFIG2] & ~ADS1292R_CONFIG2_CLK_EN;
    if (enable) v |= ADS1292R_CONFIG2_CLK_EN;
    return updateRegister(ADS1292R_REG_CONFIG2, v);
}

bool Protocentral_ADS1292R::setGPIOMode(uint8_t gpio, bool output) {
    if (gpio < 1 || gpio > 2) {
        _lastError = ADS1292R_Status::InvalidArgument;
        return false;
    }
    uint8_t ctrlBit = 0x04 << (gpio - 1);  // GPIOC: 1 = input, 0 = output
    uint8_t v = _regs[ADS1292R_REG_GPIO];
    v = output ? (v & ~ctrlBit) : (v | ctrlBit);
    return updateRegister(ADS1292R_REG_GPIO, v);
}

bool Protocentral_ADS1292R::writeGPIO(uint8_t gpio, bool level) {
    if (gpio < 1 || gpio > 2) {
        _lastError = ADS1292R_Status::InvalidArgument;
        return false;
    }
    uint8_t dataBit = 0x01 << (gpio - 1);
    uint8_t v = _regs[ADS1292R_REG_GPIO];
    v = level ? (v | dataBit) : (v & ~dataBit);
    return updateRegister(ADS1292R_REG_GPIO, v);
}

int Protocentral_ADS1292R::readGPIO(uint8_t gpio) {
    if (gpio < 1 || gpio > 2) {
        _lastError = ADS1292R_Status::InvalidArgument;
        return -1;
    }
    return (readRegister(ADS1292R_REG_GPIO) >> (gpio - 1)) & 0x01;
}

// ============================================================================
// Units and on-chip measurements
// ============================================================================

uint8_t Protocentral_ADS1292R::gainValue(ADS1292R_Gain gain) {
    static const uint8_t table[] = {6, 1, 2, 3, 4, 8, 12};
    uint8_t i = static_cast<uint8_t>(gain);
    return i < sizeof(table) ? table[i] : 6;
}

float Protocentral_ADS1292R::referenceVolts() const {
    return (_regs[ADS1292R_REG_CONFIG2] & ADS1292R_CONFIG2_VREF_4V) ? 4.033f : 2.42f;
}

float Protocentral_ADS1292R::countsToMicrovolts(int32_t counts, ADS1292R_Channel ch) const {
    // 1 LSB = (2 x VREF / gain) / 2^24
    float lsbUv = (2.0f * referenceVolts() * 1.0e6f) / (gainValue(getGain(ch)) * 16777216.0f);
    return counts * lsbUv;
}

bool Protocentral_ADS1292R::measureInternal(ADS1292R_Channel ch, ADS1292R_Mux mux, ADS1292R_Gain gain,
                                            float& microvolts) {
    if (!_initialized) {
        _lastError = ADS1292R_Status::NotInitialized;
        return false;
    }

    uint8_t reg       = chsetRegister(ch);
    uint8_t savedCh   = _regs[reg];
    uint8_t savedResp = _regs[ADS1292R_REG_RESP1];
    bool    wasRunning = _running;

    // The respiration demodulator sits on CH1 and would corrupt a DC measurement
    bool borrowResp = (ch == ADS1292R_Channel::CH1) &&
                      (savedResp & (ADS1292R_RESP1_DEMOD_EN | ADS1292R_RESP1_MOD_EN));

    bool ok = updateRegister(reg, (static_cast<uint8_t>(gain) << 4) | static_cast<uint8_t>(mux));
    if (ok && borrowResp) {
        ok = updateRegister(ADS1292R_REG_RESP1, savedResp & ~(ADS1292R_RESP1_DEMOD_EN | ADS1292R_RESP1_MOD_EN));
    }
    if (ok && !wasRunning) start();

    int32_t  sum       = 0;
    uint32_t timeoutMs = 3000U / getSampleRate() + 20;
    for (uint8_t i = 0; ok && i < SETTLE_SAMPLES + AVERAGE_SAMPLES; i++) {
        ADS1292R_Sample s;
        ok = waitForData(timeoutMs) && readFrame(s);
        if (ok && i >= SETTLE_SAMPLES) {
            sum += (ch == ADS1292R_Channel::CH1) ? s.resp : s.ecg;
        }
    }

    // Always restore the channel, even after a failure
    if (!wasRunning) stop();
    bool restored = updateRegister(reg, savedCh);
    if (borrowResp) restored = updateRegister(ADS1292R_REG_RESP1, savedResp) && restored;
    if (_irqSlot >= 0) _drdyFlag = false;  // samples taken here are not the caller's

    if (!ok || !restored) return false;

    float lsbUv = (2.0f * referenceVolts() * 1.0e6f) / (gainValue(gain) * 16777216.0f);
    microvolts  = ((float)sum / AVERAGE_SAMPLES) * lsbUv;
    return true;
}

float Protocentral_ADS1292R::readTemperature() {
    float uv;
    if (!measureInternal(ADS1292R_Channel::CH1, ADS1292R_Mux::Temperature, ADS1292R_Gain::X1, uv)) return NAN;
    // SBAS502 equation 4
    return (uv - 145300.0f) / 490.0f + 25.0f;
}

float Protocentral_ADS1292R::readAnalogSupply() {
    float uv;
    if (!measureInternal(ADS1292R_Channel::CH1, ADS1292R_Mux::Supply, ADS1292R_Gain::X1, uv)) return NAN;
    return 2.0f * uv / 1.0e6f;  // CH1 measures (AVDD + AVSS) / 2, AVSS = 0
}

float Protocentral_ADS1292R::readDigitalSupply() {
    float uv;
    if (!measureInternal(ADS1292R_Channel::CH2, ADS1292R_Mux::Supply, ADS1292R_Gain::X1, uv)) return NAN;
    return 4.0f * uv / 1.0e6f;  // CH2 measures DVDD / 4
}

// ============================================================================
// Register access
// ============================================================================

uint8_t Protocentral_ADS1292R::readRegister(uint8_t reg) {
    if (reg >= ADS1292R_NUM_REGS) {
        _lastError = ADS1292R_Status::InvalidArgument;
        return 0;
    }
    bool wasRdatac = _rdatac;
    exitRDATAC();
    uint8_t v = rawReadRegister(reg);
    if (wasRdatac) enterRDATAC();
    _regs[reg] = v;
    return v;
}

bool Protocentral_ADS1292R::writeRegister(uint8_t reg, uint8_t value) {
    if (reg == ADS1292R_REG_ID || reg >= ADS1292R_NUM_REGS) {
        _lastError = ADS1292R_Status::InvalidArgument;
        return false;
    }
    return updateRegister(reg, value);
}

// Force the bits the datasheet says "must be 0/1", so callers cannot misconfigure them
uint8_t Protocentral_ADS1292R::maskRegister(uint8_t reg, uint8_t v) {
    switch (reg) {
        case ADS1292R_REG_CONFIG1:   return v & 0x87;
        case ADS1292R_REG_CONFIG2:   return (v & 0xFB) | ADS1292R_CONFIG2_FIXED;
        case ADS1292R_REG_LOFF:      return (v & 0xFD) | ADS1292R_LOFF_FIXED;
        case ADS1292R_REG_LOFF_SENS: return v & 0x3F;
        case ADS1292R_REG_LOFF_STAT: return v & ADS1292R_LOFF_STAT_CLK_DIV;
        case ADS1292R_REG_RESP1:     return v | ADS1292R_RESP1_FIXED;
        case ADS1292R_REG_RESP2:     return (v & 0x87) | ADS1292R_RESP2_FIXED;
        case ADS1292R_REG_GPIO:      return v & 0x0F;
        default:                     return v;
    }
}

bool Protocentral_ADS1292R::updateRegister(uint8_t reg, uint8_t value) {
    value = maskRegister(reg, value);

    // Registers cannot be accessed in RDATAC mode: drop out, write, verify, resume
    bool wasRdatac = _rdatac;
    exitRDATAC();
    rawWriteRegister(reg, value);
    uint8_t readBack = rawReadRegister(reg);
    if (wasRdatac) enterRDATAC();

    // Status bits and GPIO input levels read back as live pin state, so only compare writable control bits
    uint8_t verifyMask = 0xFF;
    if (reg == ADS1292R_REG_LOFF_STAT) verifyMask = ADS1292R_LOFF_STAT_CLK_DIV;
    if (reg == ADS1292R_REG_GPIO) verifyMask = 0x0C;

    _regs[reg] = value;
    if ((readBack & verifyMask) != (value & verifyMask)) {
        _lastError = ADS1292R_Status::VerifyFailed;
        return false;
    }
    return true;
}

static const __FlashStringHelper* registerName(uint8_t reg) {
    switch (reg) {
        case ADS1292R_REG_ID:        return F("ID");
        case ADS1292R_REG_CONFIG1:   return F("CONFIG1");
        case ADS1292R_REG_CONFIG2:   return F("CONFIG2");
        case ADS1292R_REG_LOFF:      return F("LOFF");
        case ADS1292R_REG_CH1SET:    return F("CH1SET");
        case ADS1292R_REG_CH2SET:    return F("CH2SET");
        case ADS1292R_REG_RLD_SENS:  return F("RLD_SENS");
        case ADS1292R_REG_LOFF_SENS: return F("LOFF_SENS");
        case ADS1292R_REG_LOFF_STAT: return F("LOFF_STAT");
        case ADS1292R_REG_RESP1:     return F("RESP1");
        case ADS1292R_REG_RESP2:     return F("RESP2");
        default:                     return F("GPIO");
    }
}

void Protocentral_ADS1292R::printRegisters(Stream& out) {
    for (uint8_t reg = 0; reg < ADS1292R_NUM_REGS; reg++) {
        uint8_t v = readRegister(reg);
        out.print(F("0x0"));
        out.print(reg, HEX);
        out.print(F("  "));
        out.print(registerName(reg));
        out.print(F("\t0x"));
        if (v < 0x10) out.print('0');
        out.println(v, HEX);
    }
}

// ============================================================================
// Low-level SPI
// ============================================================================

void Protocentral_ADS1292R::csLow() {
    digitalWrite(_csPin, LOW);
}

void Protocentral_ADS1292R::csHigh() {
    digitalWrite(_csPin, HIGH);
}

void Protocentral_ADS1292R::sendCommand(uint8_t cmd) {
    _spi->beginTransaction(_spiSettings);
    csLow();
    _spi->transfer(cmd);
    delayMicroseconds(TCLK4_US);
    csHigh();
    _spi->endTransaction();
    delayMicroseconds(TCLK4_US);  // the next command must wait for this one to decode
}

uint8_t Protocentral_ADS1292R::rawReadRegister(uint8_t reg) {
    _spi->beginTransaction(_spiSettings);
    csLow();
    _spi->transfer(ADS1292R_CMD_RREG | reg);
    delayMicroseconds(TCLK4_US);
    _spi->transfer(0x00);  // number of registers - 1
    delayMicroseconds(TCLK4_US);
    uint8_t v = _spi->transfer(0x00);
    delayMicroseconds(TCLK4_US);
    csHigh();
    _spi->endTransaction();
    return v;
}

void Protocentral_ADS1292R::rawWriteRegister(uint8_t reg, uint8_t value) {
    _spi->beginTransaction(_spiSettings);
    csLow();
    _spi->transfer(ADS1292R_CMD_WREG | reg);
    delayMicroseconds(TCLK4_US);
    _spi->transfer(0x00);  // number of registers - 1
    delayMicroseconds(TCLK4_US);
    _spi->transfer(value);
    delayMicroseconds(TCLK4_US);
    csHigh();
    _spi->endTransaction();
}

void Protocentral_ADS1292R::hardwareReset() {
    if (_resetPin >= 0) {
        digitalWrite(_resetPin, LOW);
        delayMicroseconds(RESET_PULSE_US);
        digitalWrite(_resetPin, HIGH);
    } else {
        sendCommand(ADS1292R_CMD_SDATAC);  // RESET is ignored while in RDATAC (the power-on mode)
        sendCommand(ADS1292R_CMD_RESET);
    }
    delayMicroseconds(RESET_WAIT_US);
    _rdatac = true;  // the chip always comes out of reset in RDATAC
}

void Protocentral_ADS1292R::enterRDATAC() {
    if (_rdatac) return;
    sendCommand(ADS1292R_CMD_RDATAC);
    _rdatac = true;
}

void Protocentral_ADS1292R::exitRDATAC() {
    if (!_rdatac) return;
    sendCommand(ADS1292R_CMD_SDATAC);
    _rdatac = false;
}
