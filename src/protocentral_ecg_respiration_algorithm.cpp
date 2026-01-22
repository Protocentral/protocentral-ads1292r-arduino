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

#include "protocentral_ecg_respiration_algorithm.h"

// 40Hz Low-pass FIR filter coefficients for ECG
const int16_t ECGRespirationAlgorithm::_ecgCoeff40HzLP[ECG_FILTER_ORDER] = {
    -72,    122,    -31,    -99,    117,      0,   -121,    105,     34,
   -137,     84,     70,   -146,     55,    104,   -147,     20,    135,
   -137,    -21,    160,   -117,    -64,    177,    -87,   -108,    185,
    -48,   -151,    181,      0,   -188,    164,     54,   -218,    134,
    112,   -238,     90,    171,   -244,     33,    229,   -235,    -36,
    280,   -208,   -115,    322,   -161,   -203,    350,    -92,   -296,
    361,      0,   -391,    348,    117,   -486,    305,    264,   -577,
    225,    445,   -660,     93,    676,   -733,   -119,    991,   -793,
   -480,   1486,   -837,  -1226,   2561,   -865,  -4018,   9438,  20972,
   9438,  -4018,   -865,   2561,  -1226,   -837,   1486,   -480,   -793,
    991,   -119,   -733,    676,     93,   -660,    445,    225,   -577,
    264,    305,   -486,    117,    348,   -391,      0,    361,   -296,
    -92,    350,   -203,   -161,    322,   -115,   -208,    280,    -36,
   -235,    229,     33,   -244,    171,     90,   -238,    112,    134,
   -218,     54,    164,   -188,      0,    181,   -151,    -48,    185,
   -108,    -87,    177,    -64,   -117,    160,    -21,   -137,    135,
     20,   -147,    104,     55,   -146,     70,     84,   -137,     34,
    105,   -121,      0,    117,    -99,    -31,    122,    -72
};

// 2Hz Low-pass FIR filter coefficients for Respiration
const int16_t ECGRespirationAlgorithm::_respCoeff2HzLP[ECG_FILTER_ORDER] = {
    120,    124,    126,    127,    127,    125,    122,    118,    113,
    106,     97,     88,     77,     65,     52,     38,     24,      8,
     -8,    -25,    -42,    -59,    -76,    -93,   -110,   -126,   -142,
   -156,   -170,   -183,   -194,   -203,   -211,   -217,   -221,   -223,
   -223,   -220,   -215,   -208,   -198,   -185,   -170,   -152,   -132,
   -108,    -83,    -55,    -24,      8,     43,     80,    119,    159,
    201,    244,    288,    333,    378,    424,    470,    516,    561,
    606,    650,    693,    734,    773,    811,    847,    880,    911,
    939,    964,    986,   1005,   1020,   1033,   1041,   1047,   1049,
   1047,   1041,   1033,   1020,   1005,    986,    964,    939,    911,
    880,    847,    811,    773,    734,    693,    650,    606,    561,
    516,    470,    424,    378,    333,    288,    244,    201,    159,
    119,     80,     43,      8,    -24,    -55,    -83,   -108,   -132,
   -152,   -170,   -185,   -198,   -208,   -215,   -220,   -223,   -223,
   -221,   -217,   -211,   -203,   -194,   -183,   -170,   -156,   -142,
   -126,   -110,    -93,    -76,    -59,    -42,    -25,     -8,      8,
     24,     38,     52,     65,     77,     88,     97,    106,    113,
    118,    122,    125,    127,    127,    126,    124,    120
};

ECGRespirationAlgorithm::ECGRespirationAlgorithm() {
    reset();
}

void ECGRespirationAlgorithm::reset() {
    // Reset ECG filter state
    memset(_ecgWorkingBuff, 0, sizeof(_ecgWorkingBuff));
    _ecgBufStart = 0;
    _ecgBufCur = ECG_FILTER_ORDER - 1;
    _ecgPrevDCSample = 0;
    _ecgPrevSample = 0;
    _ecgFirstSample = true;

    // Reset respiration filter state
    memset(_respWorkingBuff, 0, sizeof(_respWorkingBuff));
    _respBufStart = 0;
    _respBufCur = ECG_FILTER_ORDER - 1;
    _respPrevDCSample = 0;
    _respPrevSample = 0;

    // Reset QRS detection state
    _qrsPrevPrevSample = 0;
    _qrsPrevSample = 0;
    _qrsCurrSample = 0;
    _qrsNextSample = 0;
    _qrsNextNextSample = 0;
    _qrsThresholdOld = 0;
    _qrsThresholdNew = 0;
    _qrsBufferPtr = 0;
    _qrsFirstPeakDetect = false;
    _heartRate = 0;

    // Reset peak detection state
    memset(_sampleIndex, 0, sizeof(_sampleIndex));
    _sampleCount = 0;
    _sArrayIndex = 0;
    _mArrayIndex = 0;
    _thresholdCrossed = false;
    _maximaSearch = 0;
    _peakDetected = false;
    _skipWindow = 0;
    _maximaSum = 0;
    _peak = 0;
    _sampleSum = 0;
    _noPeak = 0;
    _startSampleCountFlag = false;

    // Reset respiration detection state
    _respPrevPrevPrevSample = 0;
    _respPrevPrevSample2 = 0;
    _respPrevSample2 = 0;
    _respMinThreshold = 0x7FFF;
    _respMaxThreshold = -32768;
    _respMinThresholdNew = 0x7FFF;
    _respMaxThresholdNew = -32768;
    _respAvgThreshold = 0;
    _respSkipCount = 0;
    _respSampleCount = 0;
    _respSampleCountNtve = 0;
    _respTimeCnt = 0;
    _respStartCalc = false;
    _respPtiveEdgeDetected = false;
    _respNtiveEdgeDetected = false;
    _respPtiveCnt = 0;
    _respNtiveCnt = 0;
    _respPeakCount = 0;
    memset(_respPeakArray, 0, sizeof(_respPeakArray));
    _respirationRate = 0;
}

VitalSigns ECGRespirationAlgorithm::processSample(int16_t ecgSample, int16_t respirationSample) {
    VitalSigns vitals;

    // Filter and compute heart rate
    vitals.filteredECG = filterECG(ecgSample);
    vitals.heartRate = computeHeartRate(vitals.filteredECG);
    vitals.heartRateValid = (vitals.heartRate > 0 && vitals.heartRate < 250);

    // Filter and compute respiration rate
    vitals.filteredResp = filterRespiration(respirationSample);
    vitals.respirationRate = computeRespirationRate(vitals.filteredResp);
    vitals.respirationValid = (vitals.respirationRate > 0 && vitals.respirationRate < 60);

    return vitals;
}

void ECGRespirationAlgorithm::ecgFilterProcess(int16_t *workingBuff, const int16_t *coeffBuf, int16_t *filterOut) {
    int32_t acc = 0;

    for (int k = 0; k < ECG_FILTER_ORDER; k++) {
        acc += (int32_t)(coeffBuf[k]) * (int32_t)(workingBuff[-k]);
    }

    // Saturate
    if (acc > 0x3FFFFFFF) {
        acc = 0x3FFFFFFF;
    } else if (acc < -0x40000000) {
        acc = -0x40000000;
    }

    // Convert from Q30 to Q15
    *filterOut = (int16_t)(acc >> 15);
}

int16_t ECGRespirationAlgorithm::filterECG(int16_t sample) {
    if (_ecgFirstSample) {
        memset(_ecgWorkingBuff, 0, sizeof(_ecgWorkingBuff));
        _ecgPrevDCSample = 0;
        _ecgPrevSample = 0;
        _ecgFirstSample = false;
    }

    // DC removal (first order IIR high-pass)
    int16_t temp1 = (int16_t)(ECG_NRCOEFF * _ecgPrevDCSample);
    _ecgPrevDCSample = (sample - _ecgPrevSample) + temp1;
    _ecgPrevSample = sample;
    int16_t ecgData = _ecgPrevDCSample >> 2;

    // Store in circular buffer and apply FIR filter
    _ecgWorkingBuff[_ecgBufCur] = ecgData;
    int16_t filtOut = 0;
    ecgFilterProcess(&_ecgWorkingBuff[_ecgBufCur], _ecgCoeff40HzLP, &filtOut);
    _ecgWorkingBuff[_ecgBufStart] = ecgData;

    _ecgBufCur++;
    _ecgBufStart++;
    if (_ecgBufStart >= (ECG_FILTER_ORDER - 1)) {
        _ecgBufStart = 0;
        _ecgBufCur = ECG_FILTER_ORDER - 1;
    }

    return filtOut;
}

void ECGRespirationAlgorithm::respFilterProcess(int16_t *workingBuff, const int16_t *coeffBuf, int16_t *filterOut) {
    int32_t acc = 0;

    for (int k = 0; k < ECG_FILTER_ORDER; k++) {
        acc += (int32_t)(coeffBuf[k]) * (int32_t)(workingBuff[-k]);
    }

    if (acc > 0x3FFFFFFF) {
        acc = 0x3FFFFFFF;
    } else if (acc < -0x40000000) {
        acc = -0x40000000;
    }

    *filterOut = (int16_t)(acc >> 15);
}

int16_t ECGRespirationAlgorithm::filterRespiration(int16_t sample) {
    // DC removal
    int16_t temp1 = (int16_t)(ECG_NRCOEFF * _respPrevDCSample);
    _respPrevDCSample = (sample - _respPrevSample) + temp1;
    _respPrevSample = sample;

    // Store in circular buffer and apply FIR filter
    _respWorkingBuff[_respBufCur] = sample;  // Use raw sample for respiration
    int16_t filtOut = 0;
    respFilterProcess(&_respWorkingBuff[_respBufCur], _respCoeff2HzLP, &filtOut);
    _respWorkingBuff[_respBufStart] = sample;

    _respBufCur++;
    _respBufStart++;
    if (_respBufStart >= (ECG_FILTER_ORDER - 1)) {
        _respBufStart = 0;
        _respBufCur = ECG_FILTER_ORDER - 1;
    }

    return filtOut;
}

uint8_t ECGRespirationAlgorithm::computeHeartRate(int16_t filteredECG) {
    // Moving average (32 samples)
    static int16_t prevData[32] = {0};
    int32_t mac = 0;

    prevData[0] = filteredECG;
    for (int i = 31; i > 0; i--) {
        mac += prevData[i];
        prevData[i] = prevData[i - 1];
    }
    mac += filteredECG;
    mac = mac >> 2;
    int16_t currSample = (int16_t)mac;

    // Update sample history
    _qrsPrevPrevSample = _qrsPrevSample;
    _qrsPrevSample = _qrsCurrSample;
    _qrsCurrSample = _qrsNextSample;
    _qrsNextSample = _qrsNextNextSample;
    _qrsNextNextSample = currSample;

    // Process for QRS detection
    qrsProcessBuffer();

    return (uint8_t)_heartRate;
}

void ECGRespirationAlgorithm::qrsProcessBuffer() {
    static int16_t maxVal = 0;

    // Calculate first derivative
    int16_t firstDerivative = _qrsNextSample - _qrsPrevSample;
    if (firstDerivative < 0) {
        firstDerivative = -firstDerivative;
    }

    uint16_t scaledResult = (uint16_t)firstDerivative;

    if (scaledResult > maxVal) {
        maxVal = scaledResult;
    }

    _qrsBufferPtr++;

    // After 2 seconds, set initial threshold
    if (_qrsBufferPtr == ECG_TWO_SEC_SAMPLES) {
        _qrsThresholdOld = (maxVal * 7) / 10;
        _qrsThresholdNew = _qrsThresholdOld;
        _qrsFirstPeakDetect = true;
        maxVal = 0;
        _qrsBufferPtr = 0;
    }

    if (_qrsFirstPeakDetect) {
        qrsCheckThreshold(scaledResult);
    }
}

void ECGRespirationAlgorithm::qrsCheckThreshold(uint16_t scaledResult) {
    uint16_t maxVal = 0;
    uint16_t hrAvg;

    if (_thresholdCrossed) {
        _sampleCount++;
        _maximaSearch++;

        if (scaledResult > _peak) {
            _peak = scaledResult;
        }

        if (_maximaSearch >= QRS_MAXIMA_SEARCH_WINDOW) {
            _maximaSum += _peak;
            _maximaSearch = 0;
            _thresholdCrossed = false;
            _peakDetected = true;
        }
    } else if (_peakDetected) {
        _sampleCount++;
        _skipWindow++;

        if (_skipWindow >= QRS_MINIMUM_SKIP_WINDOW) {
            _skipWindow = 0;
            _peakDetected = false;
        }

        if (_mArrayIndex == QRS_MAX_PEAK_TO_SEARCH) {
            _sampleSum = _sampleSum / (QRS_MAX_PEAK_TO_SEARCH - 1);
            hrAvg = (uint16_t)_sampleSum;

            _heartRate = (uint16_t)(60 * ECG_SAMPLING_RATE) / hrAvg;
            if (_heartRate > 250) {
                _heartRate = 250;
            }

            _maximaSum = _maximaSum / QRS_MAX_PEAK_TO_SEARCH;
            maxVal = (int16_t)_maximaSum;
            _maximaSum = (maxVal * 7) / 10;
            _qrsThresholdNew = (int16_t)_maximaSum;

            if (_qrsThresholdNew > (4 * _qrsThresholdOld)) {
                _qrsThresholdNew = _qrsThresholdOld;
            }

            // Reset state
            _sampleCount = 0;
            _sArrayIndex = 0;
            _mArrayIndex = 0;
            _maximaSum = 0;
            memset(_sampleIndex, 0, sizeof(_sampleIndex));
            _startSampleCountFlag = false;
            _sampleSum = 0;
        }
    } else if (scaledResult > _qrsThresholdNew) {
        _startSampleCountFlag = true;
        _sampleCount++;
        _mArrayIndex++;
        _thresholdCrossed = true;
        _peak = scaledResult;
        _noPeak = 0;

        _sampleIndex[_sArrayIndex] = _sampleCount;
        if (_sArrayIndex >= 1) {
            _sampleSum += _sampleIndex[_sArrayIndex] - _sampleIndex[_sArrayIndex - 1];
        }
        _sArrayIndex++;
    } else if ((scaledResult < _qrsThresholdNew) && _startSampleCountFlag) {
        _sampleCount++;
        _noPeak++;

        if (_noPeak > (3 * ECG_SAMPLING_RATE)) {
            // Reset state - no peak found in 3 seconds
            _sampleCount = 0;
            _sArrayIndex = 0;
            _mArrayIndex = 0;
            _maximaSum = 0;
            memset(_sampleIndex, 0, sizeof(_sampleIndex));
            _startSampleCountFlag = false;
            _peakDetected = false;
            _sampleSum = 0;
            _qrsFirstPeakDetect = false;
            _noPeak = 0;
            _heartRate = 0;
        }
    } else {
        _noPeak++;
        if (_noPeak > (3 * ECG_SAMPLING_RATE)) {
            // Reset state
            _sampleCount = 0;
            _sArrayIndex = 0;
            _mArrayIndex = 0;
            _maximaSum = 0;
            memset(_sampleIndex, 0, sizeof(_sampleIndex));
            _startSampleCountFlag = false;
            _peakDetected = false;
            _sampleSum = 0;
            _qrsFirstPeakDetect = false;
            _noPeak = 0;
            _heartRate = 0;
        }
    }
}

uint8_t ECGRespirationAlgorithm::computeRespirationRate(int16_t filteredResp) {
    // Moving average (64 samples)
    static int16_t prevData[64] = {0};
    int32_t mac = 0;

    prevData[0] = filteredResp;
    for (int i = 63; i > 0; i--) {
        mac += prevData[i];
        prevData[i] = prevData[i - 1];
    }
    mac += filteredResp;
    int16_t currSample = (int16_t)(mac >> 1);

    respirationRateDetection(currSample);

    return _respirationRate;
}

void ECGRespirationAlgorithm::respirationRateDetection(int16_t respWave) {
    _respSampleCount++;
    _respSampleCountNtve++;
    _respTimeCnt++;

    if (respWave < _respMinThresholdNew) {
        _respMinThresholdNew = respWave;
    }
    if (respWave > _respMaxThresholdNew) {
        _respMaxThresholdNew = respWave;
    }

    if (_respSampleCount > 1000) _respSampleCount = 0;
    if (_respSampleCountNtve > 1000) _respSampleCountNtve = 0;

    if (_respStartCalc) {
        if (_respTimeCnt >= 500) {
            _respTimeCnt = 0;
            if ((_respMaxThresholdNew - _respMinThresholdNew) > 400) {
                _respMaxThreshold = _respMaxThresholdNew;
                _respMinThreshold = _respMinThresholdNew;
                _respAvgThreshold = (_respMaxThreshold + _respMinThreshold) >> 1;
            } else {
                _respStartCalc = false;
                _respirationRate = 0;
            }
        }

        _respPrevPrevPrevSample = _respPrevPrevSample2;
        _respPrevPrevSample2 = _respPrevSample2;
        _respPrevSample2 = respWave;

        if (_respSkipCount == 0) {
            if (_respPrevPrevPrevSample < _respAvgThreshold && respWave > _respAvgThreshold) {
                if (_respSampleCount > 40 && _respSampleCount < 700) {
                    _respPtiveEdgeDetected = true;
                    _respPtiveCnt = _respSampleCount;
                    _respSkipCount = 4;
                }
                _respSampleCount = 0;
            }

            if (_respPrevPrevPrevSample < _respAvgThreshold && respWave > _respAvgThreshold) {
                if (_respSampleCountNtve > 40 && _respSampleCountNtve < 700) {
                    _respNtiveEdgeDetected = true;
                    _respNtiveCnt = _respSampleCountNtve;
                    _respSkipCount = 4;
                }
                _respSampleCountNtve = 0;
            }

            if (_respPtiveEdgeDetected && _respNtiveEdgeDetected) {
                _respPtiveEdgeDetected = false;
                _respNtiveEdgeDetected = false;

                if (abs(_respPtiveCnt - _respNtiveCnt) < 5) {
                    _respPeakArray[_respPeakCount++] = _respPtiveCnt;
                    _respPeakArray[_respPeakCount++] = _respNtiveCnt;

                    if (_respPeakCount == 8) {
                        _respPeakCount = 0;
                        uint16_t sum = 0;
                        for (int i = 0; i < 8; i++) {
                            sum += _respPeakArray[i];
                        }
                        sum = sum >> 3;
                        _respirationRate = 6000 / sum;  // 60 * 125 / sampleCount
                    }
                }
            }
        } else {
            _respSkipCount--;
        }
    } else {
        _respTimeCnt++;
        if (_respTimeCnt >= 500) {
            _respTimeCnt = 0;
            if ((_respMaxThresholdNew - _respMinThresholdNew) > 400) {
                _respStartCalc = true;
                _respMaxThreshold = _respMaxThresholdNew;
                _respMinThreshold = _respMinThresholdNew;
                _respAvgThreshold = (_respMaxThreshold + _respMinThreshold) >> 1;
                _respPrevPrevPrevSample = respWave;
                _respPrevPrevSample2 = respWave;
                _respPrevSample2 = respWave;
            }
        }
    }
}
