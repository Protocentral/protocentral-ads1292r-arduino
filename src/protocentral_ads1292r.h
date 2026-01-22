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

#ifndef PROTOCENTRAL_ADS1292R_H
#define PROTOCENTRAL_ADS1292R_H

#include "Arduino.h"
#include <SPI.h>

// SPI Commands
#define ADS1292R_SPI_CMD_WAKEUP    0x02
#define ADS1292R_SPI_CMD_STANDBY   0x04
#define ADS1292R_SPI_CMD_RESET     0x06
#define ADS1292R_SPI_CMD_START     0x08
#define ADS1292R_SPI_CMD_STOP      0x0A
#define ADS1292R_SPI_CMD_RDATAC    0x10
#define ADS1292R_SPI_CMD_SDATAC    0x11
#define ADS1292R_SPI_CMD_RDATA     0x12
#define ADS1292R_SPI_CMD_RREG      0x20
#define ADS1292R_SPI_CMD_WREG      0x40

// Register Addresses
#define ADS1292R_REG_ID           0x00
#define ADS1292R_REG_CONFIG1      0x01
#define ADS1292R_REG_CONFIG2      0x02
#define ADS1292R_REG_LOFF         0x03
#define ADS1292R_REG_CH1SET       0x04
#define ADS1292R_REG_CH2SET       0x05
#define ADS1292R_REG_RLD_SENS     0x06
#define ADS1292R_REG_LOFF_SENS    0x07
#define ADS1292R_REG_LOFF_STAT    0x08
#define ADS1292R_REG_RESP1        0x09
#define ADS1292R_REG_RESP2        0x0A
#define ADS1292R_REG_GPIO         0x0B

// OpenView Protocol Packet Format
#define CES_CMDIF_PKT_START_1     0x0A
#define CES_CMDIF_PKT_START_2     0xFA
#define CES_CMDIF_TYPE_DATA       0x02
#define CES_CMDIF_PKT_STOP_1      0x00
#define CES_CMDIF_PKT_STOP_2      0x0B

// Sampling Rate Configuration (CONFIG1 register bits 2:0)
enum class ADS1292R_SamplingRate : uint8_t {
    SPS_125  = 0x00,  // 125 samples per second (default)
    SPS_250  = 0x01,  // 250 samples per second
    SPS_500  = 0x02,  // 500 samples per second
    SPS_1000 = 0x03,  // 1000 samples per second
    SPS_2000 = 0x04,  // 2000 samples per second
    SPS_4000 = 0x05,  // 4000 samples per second
    SPS_8000 = 0x06   // 8000 samples per second
};

// Channel Gain Configuration (CHnSET register bits 6:4)
enum class ADS1292R_Gain : uint8_t {
    GAIN_6  = 0x00,   // Gain 6 (default)
    GAIN_1  = 0x10,   // Gain 1
    GAIN_2  = 0x20,   // Gain 2
    GAIN_3  = 0x30,   // Gain 3
    GAIN_4  = 0x40,   // Gain 4
    GAIN_8  = 0x50,   // Gain 8
    GAIN_12 = 0x60    // Gain 12
};

// Channel Input Selection (CHnSET register bits 3:0)
enum class ADS1292R_InputMux : uint8_t {
    NORMAL       = 0x00,  // Normal electrode input (default)
    INPUT_SHORT  = 0x01,  // Input shorted
    RLD_MEASURE  = 0x02,  // RLD measurement
    MVDD         = 0x03,  // MVDD for supply measurement
    TEMP         = 0x04,  // Temperature sensor
    TEST_SIGNAL  = 0x05,  // Test signal
    RLD_DRP      = 0x06,  // RLD_DRP
    RLD_DRN      = 0x07,  // RLD_DRN
    RLD_DRPM     = 0x08,  // RLD_DRPM
    RLD_DRNM     = 0x09   // RLD_DRNM
};

// Lead-off Detection Current (LOFF register bits 3:2)
enum class ADS1292R_LeadOffCurrent : uint8_t {
    CURRENT_6NA   = 0x00,  // 6 nA (default)
    CURRENT_22NA  = 0x04,  // 22 nA
    CURRENT_6UA   = 0x08,  // 6 uA
    CURRENT_22UA  = 0x0C   // 22 uA
};

// Lead-off Detection Frequency (LOFF register bits 1:0)
enum class ADS1292R_LeadOffFreq : uint8_t {
    DC       = 0x00,       // DC lead-off detection (default)
    AC_7_8HZ = 0x01,       // AC lead-off detection at fDR/4
    AC_31HZ  = 0x02,       // AC lead-off detection at fDR
    AC_DR_4  = 0x03        // AC lead-off detection at fDR/4
};

// Respiration Modulation Frequency (RESP1 register bits 4:2)
enum class ADS1292R_RespModFreq : uint8_t {
    FREQ_64KHZ  = 0x00,   // 64 kHz modulation clock
    FREQ_32KHZ  = 0x04    // 32 kHz modulation clock (default)
};

// Respiration Phase (RESP1 register bits 7:6)
enum class ADS1292R_RespPhase : uint8_t {
    PHASE_0     = 0x00,   // 0 degrees
    PHASE_22_5  = 0x40,   // 22.5 degrees
    PHASE_45    = 0x80,   // 45 degrees
    PHASE_67_5  = 0xC0,   // 67.5 degrees
    PHASE_90    = 0x10,   // 90 degrees
    PHASE_112_5 = 0x50,   // 112.5 degrees
    PHASE_135   = 0x90,   // 135 degrees
    PHASE_157_5 = 0xD0    // 157.5 degrees
};

// Data structure for ECG and Respiration samples
struct ADS1292R_Data {
    int32_t ecg;              // ECG channel data (24-bit signed)
    int32_t respiration;      // Respiration channel data (24-bit signed)
    uint32_t status;          // Status byte
    bool leadOff;             // Lead-off detection flag
    bool ok;                  // Data validity flag
};

class ADS1292R {
public:
    // Constructor with pin configuration
    ADS1292R(uint8_t drdyPin, uint8_t csPin, uint8_t startPin, uint8_t pwdnPin);

    // Initialization
    bool begin();
    bool begin(uint8_t sckPin, uint8_t misoPin, uint8_t mosiPin);  // For ESP32 custom SPI pins

    // Reset and power control
    void reset();
    void powerDown();
    void wakeUp();

    // Configuration methods
    void setSamplingRate(ADS1292R_SamplingRate rate);
    void setChannel1Gain(ADS1292R_Gain gain);
    void setChannel2Gain(ADS1292R_Gain gain);
    void setChannel1Input(ADS1292R_InputMux mux);
    void setChannel2Input(ADS1292R_InputMux mux);

    // Lead-off detection configuration
    void enableLeadOffDetection(bool enable);
    void setLeadOffCurrent(ADS1292R_LeadOffCurrent current);
    void setLeadOffFrequency(ADS1292R_LeadOffFreq freq);

    // Respiration configuration
    void enableRespiration(bool enable);
    void setRespirationModFreq(ADS1292R_RespModFreq freq);
    void setRespirationPhase(ADS1292R_RespPhase phase);

    // RLD (Right Leg Drive) configuration
    void enableRLD(bool enable);
    void setRLDChannel(uint8_t channel);  // 1 or 2

    // Convenience setup methods
    void beginECG();                      // Standard ECG setup (125 SPS, Gain 6)
    void beginECGWithRespiration();       // ECG + Respiration measurement

    // Data acquisition
    bool isDataReady();
    ADS1292R_Data getData();

    // Low-level register access
    uint8_t readRegister(uint8_t reg);
    void writeRegister(uint8_t reg, uint8_t value);
    uint8_t readDeviceID();

    // Start/stop continuous data conversion
    void startConversion();
    void stopConversion();
    void startReadDataContinuous();
    void stopReadDataContinuous();

private:
    uint8_t _drdyPin;
    uint8_t _csPin;
    uint8_t _startPin;
    uint8_t _pwdnPin;

    SPIClass *_spi;
    SPISettings _spiSettings;
    bool _useCustomSPI;
    uint8_t _sckPin;
    uint8_t _misoPin;
    uint8_t _mosiPin;

    // Configuration state
    uint8_t _config1;
    uint8_t _config2;
    uint8_t _loff;
    uint8_t _ch1set;
    uint8_t _ch2set;
    uint8_t _rldSens;
    uint8_t _loffSens;
    uint8_t _resp1;
    uint8_t _resp2;

    void sendCommand(uint8_t cmd);
    void readDataRaw(uint8_t *buffer);
    void applyConfiguration();
};

#endif // PROTOCENTRAL_ADS1292R_H
