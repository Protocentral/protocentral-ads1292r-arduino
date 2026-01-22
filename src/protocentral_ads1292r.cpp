//////////////////////////////////////////////////////////////////////////////////////////
//
//   Arduino Library for ADS1292R Shield/Breakout
//
//   Copyright (c) 2017 ProtoCentral
//
//   This software is licensed under the MIT License(http://opensource.org/licenses/MIT).
//
//   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
//   INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
//   PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
//   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
//   OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
//   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
/////////////////////////////////////////////////////////////////////////////////////////

#include "protocentral_ads1292r.h"

ADS1292R::ADS1292R(uint8_t drdyPin, uint8_t csPin, uint8_t startPin, uint8_t pwdnPin)
    : _drdyPin(drdyPin), _csPin(csPin), _startPin(startPin), _pwdnPin(pwdnPin),
      _spi(&SPI), _useCustomSPI(false),
      _config1(0x00), _config2(0xA0), _loff(0x10),
      _ch1set(0x40), _ch2set(0x60), _rldSens(0x2C),
      _loffSens(0x00), _resp1(0xF2), _resp2(0x03) {
}

bool ADS1292R::begin() {
    pinMode(_drdyPin, INPUT);
    pinMode(_csPin, OUTPUT);
    pinMode(_startPin, OUTPUT);
    pinMode(_pwdnPin, OUTPUT);

    digitalWrite(_csPin, HIGH);
    digitalWrite(_startPin, LOW);
    digitalWrite(_pwdnPin, HIGH);

    _spi->begin();
    // Use 500kHz for better compatibility (ADS1292R supports up to 4MHz but timing can be tricky)
    _spiSettings = SPISettings(500000, MSBFIRST, SPI_MODE1);

    reset();
    delay(100);

    // Stop continuous data mode
    stopConversion();
    stopReadDataContinuous();
    delay(50);

    // Apply default configuration
    applyConfiguration();

    // Start continuous read
    startReadDataContinuous();
    delay(10);
    startConversion();

    return true;
}

bool ADS1292R::begin(uint8_t sckPin, uint8_t misoPin, uint8_t mosiPin) {
    _useCustomSPI = true;
    _sckPin = sckPin;
    _misoPin = misoPin;
    _mosiPin = mosiPin;

    pinMode(_drdyPin, INPUT);
    pinMode(_csPin, OUTPUT);
    pinMode(_startPin, OUTPUT);
    pinMode(_pwdnPin, OUTPUT);

    digitalWrite(_csPin, HIGH);
    digitalWrite(_startPin, LOW);
    digitalWrite(_pwdnPin, HIGH);

#if defined(ARDUINO_ARCH_ESP32)
    _spi->begin(_sckPin, _misoPin, _mosiPin, _csPin);
#else
    _spi->begin();
#endif
    // Use 500kHz for better compatibility
    _spiSettings = SPISettings(500000, MSBFIRST, SPI_MODE1);

    reset();
    delay(100);

    stopConversion();
    stopReadDataContinuous();
    delay(50);

    applyConfiguration();

    startReadDataContinuous();
    delay(10);
    startConversion();

    return true;
}

void ADS1292R::reset() {
    digitalWrite(_pwdnPin, HIGH);
    delay(100);
    digitalWrite(_pwdnPin, LOW);
    delay(100);
    digitalWrite(_pwdnPin, HIGH);
    delay(100);
}

void ADS1292R::powerDown() {
    digitalWrite(_pwdnPin, LOW);
}

void ADS1292R::wakeUp() {
    digitalWrite(_pwdnPin, HIGH);
    delay(100);
}

void ADS1292R::sendCommand(uint8_t cmd) {
    _spi->beginTransaction(_spiSettings);
    digitalWrite(_csPin, LOW);
    delayMicroseconds(10);
    digitalWrite(_csPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(_csPin, LOW);
    delayMicroseconds(10);
    _spi->transfer(cmd);
    delayMicroseconds(10);
    digitalWrite(_csPin, HIGH);
    _spi->endTransaction();
}

void ADS1292R::startConversion() {
    digitalWrite(_startPin, HIGH);
    delay(20);
}

void ADS1292R::stopConversion() {
    digitalWrite(_startPin, LOW);
    delay(100);
}

void ADS1292R::startReadDataContinuous() {
    sendCommand(ADS1292R_SPI_CMD_RDATAC);
}

void ADS1292R::stopReadDataContinuous() {
    sendCommand(ADS1292R_SPI_CMD_SDATAC);
}

uint8_t ADS1292R::readRegister(uint8_t reg) {
    uint8_t data;

    stopReadDataContinuous();
    delay(10);

    _spi->beginTransaction(_spiSettings);
    digitalWrite(_csPin, LOW);
    delayMicroseconds(10);
    _spi->transfer(ADS1292R_SPI_CMD_RREG | reg);
    _spi->transfer(0x00);  // Read 1 register
    data = _spi->transfer(0x00);
    delayMicroseconds(10);
    digitalWrite(_csPin, HIGH);
    _spi->endTransaction();

    startReadDataContinuous();

    return data;
}

void ADS1292R::writeRegister(uint8_t reg, uint8_t value) {
    // Apply register-specific masks based on ADS1292R datasheet
    switch (reg) {
        case ADS1292R_REG_CONFIG1:
            value &= 0x87;
            break;
        case ADS1292R_REG_CONFIG2:
            value &= 0xFB;
            value |= 0x80;
            break;
        case ADS1292R_REG_LOFF:
            value &= 0xFD;
            value |= 0x10;
            break;
        case ADS1292R_REG_LOFF_SENS:
            value &= 0x3F;
            break;
        case ADS1292R_REG_LOFF_STAT:
            value &= 0x5F;
            break;
        case ADS1292R_REG_RESP1:
            value |= 0x02;
            break;
        case ADS1292R_REG_RESP2:
            value &= 0x87;
            value |= 0x01;
            break;
        case ADS1292R_REG_GPIO:
            value &= 0x0F;
            break;
    }

    _spi->beginTransaction(_spiSettings);
    digitalWrite(_csPin, LOW);
    delayMicroseconds(10);
    digitalWrite(_csPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(_csPin, LOW);
    delayMicroseconds(10);
    _spi->transfer(ADS1292R_SPI_CMD_WREG | reg);
    _spi->transfer(0x00);  // Write 1 register
    _spi->transfer(value);
    delayMicroseconds(10);
    digitalWrite(_csPin, HIGH);
    _spi->endTransaction();
}

uint8_t ADS1292R::readDeviceID() {
    return readRegister(ADS1292R_REG_ID);
}

void ADS1292R::applyConfiguration() {
    writeRegister(ADS1292R_REG_CONFIG1, _config1);
    delay(10);
    writeRegister(ADS1292R_REG_CONFIG2, _config2);
    delay(10);
    writeRegister(ADS1292R_REG_LOFF, _loff);
    delay(10);
    writeRegister(ADS1292R_REG_CH1SET, _ch1set);
    delay(10);
    writeRegister(ADS1292R_REG_CH2SET, _ch2set);
    delay(10);
    writeRegister(ADS1292R_REG_RLD_SENS, _rldSens);
    delay(10);
    writeRegister(ADS1292R_REG_LOFF_SENS, _loffSens);
    delay(10);
    writeRegister(ADS1292R_REG_RESP1, _resp1);
    delay(10);
    writeRegister(ADS1292R_REG_RESP2, _resp2);
    delay(10);
}

void ADS1292R::setSamplingRate(ADS1292R_SamplingRate rate) {
    _config1 = (_config1 & 0xF8) | static_cast<uint8_t>(rate);
}

void ADS1292R::setChannel1Gain(ADS1292R_Gain gain) {
    _ch1set = (_ch1set & 0x8F) | static_cast<uint8_t>(gain);
}

void ADS1292R::setChannel2Gain(ADS1292R_Gain gain) {
    _ch2set = (_ch2set & 0x8F) | static_cast<uint8_t>(gain);
}

void ADS1292R::setChannel1Input(ADS1292R_InputMux mux) {
    _ch1set = (_ch1set & 0xF0) | static_cast<uint8_t>(mux);
}

void ADS1292R::setChannel2Input(ADS1292R_InputMux mux) {
    _ch2set = (_ch2set & 0xF0) | static_cast<uint8_t>(mux);
}

void ADS1292R::enableLeadOffDetection(bool enable) {
    if (enable) {
        _loffSens |= 0x0F;
    } else {
        _loffSens &= 0xF0;
    }
}

void ADS1292R::setLeadOffCurrent(ADS1292R_LeadOffCurrent current) {
    _loff = (_loff & 0xF3) | static_cast<uint8_t>(current);
}

void ADS1292R::setLeadOffFrequency(ADS1292R_LeadOffFreq freq) {
    _loff = (_loff & 0xFC) | static_cast<uint8_t>(freq);
}

void ADS1292R::enableRespiration(bool enable) {
    if (enable) {
        _resp1 |= 0x80;   // Enable respiration modulation
        _resp1 |= 0x40;   // Enable respiration demodulation
    } else {
        _resp1 &= 0x3F;   // Disable both
    }
}

void ADS1292R::setRespirationModFreq(ADS1292R_RespModFreq freq) {
    _resp1 = (_resp1 & 0xE3) | static_cast<uint8_t>(freq);
}

void ADS1292R::setRespirationPhase(ADS1292R_RespPhase phase) {
    _resp1 = (_resp1 & 0x0F) | static_cast<uint8_t>(phase);
}

void ADS1292R::enableRLD(bool enable) {
    if (enable) {
        _config2 |= 0x20;  // Enable RLD buffer
    } else {
        _config2 &= 0xDF;
    }
}

void ADS1292R::setRLDChannel(uint8_t channel) {
    if (channel == 1) {
        _rldSens = 0x23;  // RLD from channel 1
    } else {
        _rldSens = 0x2C;  // RLD from channel 2 (default)
    }
}

void ADS1292R::beginECG() {
    // Standard ECG configuration: 125 SPS, Gain 6, RLD enabled
    _config1 = 0x00;   // 125 SPS
    _config2 = 0xA0;   // Lead-off comp off, test signal disabled
    _loff = 0x10;      // Lead-off defaults
    _ch1set = 0x40;    // Ch1 enabled, gain 6, normal electrode input
    _ch2set = 0x60;    // Ch2 enabled, gain 6, normal electrode input
    _rldSens = 0x2C;   // RLD from Ch2
    _loffSens = 0x00;  // Lead-off detection disabled
    _resp1 = 0x02;     // Respiration disabled
    _resp2 = 0x01;     // Respiration defaults

    stopConversion();
    stopReadDataContinuous();
    delay(50);
    applyConfiguration();
    startReadDataContinuous();
    delay(10);
    startConversion();
}

void ADS1292R::beginECGWithRespiration() {
    // ECG + Respiration configuration
    _config1 = 0x00;   // 125 SPS
    _config2 = 0xA0;   // Lead-off comp off, test signal disabled
    _loff = 0x10;      // Lead-off defaults
    _ch1set = 0x40;    // Ch1 enabled, gain 6 (respiration)
    _ch2set = 0x60;    // Ch2 enabled, gain 6 (ECG)
    _rldSens = 0x2C;   // RLD from Ch2
    _loffSens = 0x00;  // Lead-off detection disabled
    _resp1 = 0xF2;     // Respiration MOD/DEMOD enabled, phase 0
    _resp2 = 0x03;     // Respiration freq defaults

    stopConversion();
    stopReadDataContinuous();
    delay(50);
    applyConfiguration();
    startReadDataContinuous();
    delay(10);
    startConversion();
}

bool ADS1292R::isDataReady() {
    return (digitalRead(_drdyPin) == LOW);
}

void ADS1292R::readDataRaw(uint8_t *buffer) {
    _spi->beginTransaction(_spiSettings);
    digitalWrite(_csPin, LOW);
    delayMicroseconds(2);  // Small setup time

    for (int i = 0; i < 9; i++) {
        buffer[i] = _spi->transfer(0x00);
    }

    digitalWrite(_csPin, HIGH);
    _spi->endTransaction();
}

ADS1292R_Data ADS1292R::getData() {
    ADS1292R_Data data;
    data.ok = false;
    data.leadOff = true;
    data.ecg = 0;
    data.respiration = 0;
    data.status = 0;

    // Note: Caller should check isDataReady() before calling this function.
    // We read immediately to avoid race conditions with DRDY timing.
    uint8_t buffer[9];
    readDataRaw(buffer);

    // Parse status byte (first 3 bytes)
    data.status = ((uint32_t)buffer[0] << 16) | ((uint32_t)buffer[1] << 8) | buffer[2];

    // Check lead-off status (bits 15-19)
    uint8_t leadStatus = (data.status >> 15) & 0x1F;
    data.leadOff = (leadStatus != 0);

    // Parse respiration data (bytes 3-5) - Channel 1
    uint32_t respRaw = ((uint32_t)buffer[3] << 16) | ((uint32_t)buffer[4] << 8) | buffer[5];
    // Sign extend 24-bit to 32-bit
    if (respRaw & 0x800000) {
        data.respiration = (int32_t)(respRaw | 0xFF000000);
    } else {
        data.respiration = (int32_t)respRaw;
    }

    // Parse ECG data (bytes 6-8) - Channel 2
    uint32_t ecgRaw = ((uint32_t)buffer[6] << 16) | ((uint32_t)buffer[7] << 8) | buffer[8];
    // Sign extend 24-bit to 32-bit
    if (ecgRaw & 0x800000) {
        data.ecg = (int32_t)(ecgRaw | 0xFF000000);
    } else {
        data.ecg = (int32_t)ecgRaw;
    }

    data.ok = true;
    return data;
}
