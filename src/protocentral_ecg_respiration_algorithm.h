//////////////////////////////////////////////////////////////////////////////////////////
//
//   ECG and Respiration Signal Processing Algorithm
//   For use with ADS1292R Shield/Breakout
//
//   Copyright (c) 2017 ProtoCentral
//   Heartrate and respiration computation based on original code from Texas Instruments
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

#ifndef PROTOCENTRAL_ECG_RESPIRATION_ALGORITHM_H
#define PROTOCENTRAL_ECG_RESPIRATION_ALGORITHM_H

#include "Arduino.h"

// Algorithm Constants
#define ECG_FILTER_ORDER          161
#define ECG_NRCOEFF               0.992f
#define ECG_SAMPLING_RATE         125
#define ECG_TWO_SEC_SAMPLES       (2 * ECG_SAMPLING_RATE)

// QRS Detection Parameters
#define QRS_MAX_PEAK_TO_SEARCH    5
#define QRS_MAXIMA_SEARCH_WINDOW  25
#define QRS_MINIMUM_SKIP_WINDOW   30
#define QRS_THRESHOLD_FRACTION    0.4f

// Computed vital signs result structure
struct VitalSigns {
    uint8_t heartRate;        // Heart rate in BPM (beats per minute)
    uint8_t respirationRate;  // Respiration rate in breaths per minute
    int16_t filteredECG;      // Filtered ECG sample (for display)
    int16_t filteredResp;     // Filtered respiration sample (for display)
    bool heartRateValid;      // True if heart rate is valid
    bool respirationValid;    // True if respiration rate is valid
};

class ECGRespirationAlgorithm {
public:
    ECGRespirationAlgorithm();

    // Reset algorithm state
    void reset();

    // Process new samples and compute vital signs
    // Call this at 125 SPS with new ECG and respiration samples
    VitalSigns processSample(int16_t ecgSample, int16_t respirationSample);

    // Individual processing stages (for advanced use)
    int16_t filterECG(int16_t sample);
    int16_t filterRespiration(int16_t sample);
    uint8_t computeHeartRate(int16_t filteredECG);
    uint8_t computeRespirationRate(int16_t filteredResp);

private:
    // ECG filter state
    int16_t _ecgWorkingBuff[2 * ECG_FILTER_ORDER];
    uint16_t _ecgBufStart;
    uint16_t _ecgBufCur;
    int16_t _ecgPrevDCSample;
    int16_t _ecgPrevSample;
    bool _ecgFirstSample;

    // Respiration filter state
    int16_t _respWorkingBuff[2 * ECG_FILTER_ORDER];
    uint16_t _respBufStart;
    uint16_t _respBufCur;
    int16_t _respPrevDCSample;
    int16_t _respPrevSample;

    // QRS detection state
    int16_t _qrsPrevPrevSample;
    int16_t _qrsPrevSample;
    int16_t _qrsCurrSample;
    int16_t _qrsNextSample;
    int16_t _qrsNextNextSample;
    int16_t _qrsThresholdOld;
    int16_t _qrsThresholdNew;
    uint16_t _qrsBufferPtr;
    bool _qrsFirstPeakDetect;
    uint16_t _heartRate;

    // QRS peak detection state
    uint16_t _sampleIndex[QRS_MAX_PEAK_TO_SEARCH + 2];
    uint16_t _sampleCount;
    uint16_t _sArrayIndex;
    uint16_t _mArrayIndex;
    bool _thresholdCrossed;
    uint16_t _maximaSearch;
    bool _peakDetected;
    uint16_t _skipWindow;
    int32_t _maximaSum;
    uint16_t _peak;
    uint32_t _sampleSum;
    uint16_t _noPeak;
    bool _startSampleCountFlag;

    // Respiration detection state
    int16_t _respPrevPrevPrevSample;
    int16_t _respPrevPrevSample2;
    int16_t _respPrevSample2;
    int16_t _respMinThreshold;
    int16_t _respMaxThreshold;
    int16_t _respMinThresholdNew;
    int16_t _respMaxThresholdNew;
    int16_t _respAvgThreshold;
    uint16_t _respSkipCount;
    uint16_t _respSampleCount;
    uint16_t _respSampleCountNtve;
    uint16_t _respTimeCnt;
    bool _respStartCalc;
    bool _respPtiveEdgeDetected;
    bool _respNtiveEdgeDetected;
    uint16_t _respPtiveCnt;
    uint16_t _respNtiveCnt;
    uint8_t _respPeakCount;
    uint16_t _respPeakArray[8];
    uint8_t _respirationRate;

    // Filter coefficients (static)
    static const int16_t _ecgCoeff40HzLP[ECG_FILTER_ORDER];
    static const int16_t _respCoeff2HzLP[ECG_FILTER_ORDER];

    // Internal methods
    void ecgFilterProcess(int16_t *workingBuff, const int16_t *coeffBuf, int16_t *filterOut);
    void respFilterProcess(int16_t *workingBuff, const int16_t *coeffBuf, int16_t *filterOut);
    void qrsProcessBuffer();
    void qrsCheckThreshold(uint16_t scaledResult);
    void respirationRateDetection(int16_t respWave);
};

#endif // PROTOCENTRAL_ECG_RESPIRATION_ALGORITHM_H
