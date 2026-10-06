// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2017-2026 Ashwin Whitchurch, ProtoCentral Electronics
// SPDX-FileCopyrightText: Filter and QRS approach derived from Texas Instruments ADS1x9x ECG reference code
//
// https://github.com/Protocentral/protocentral-ads1292r-arduino

#include "Protocentral_ADS1292R_Algorithms.h"

#if defined(__AVR__)
#include <avr/pgmspace.h>
#define ADS1292R_COEFF_ATTR PROGMEM
#define ADS1292R_COEFF(table, i) ((int16_t)pgm_read_word(&(table)[i]))
#else
#define ADS1292R_COEFF_ATTR
#define ADS1292R_COEFF(table, i) ((table)[i])
#endif

// QRS detector (TI reference values at 125 SPS)
static const uint16_t QRS_THRESHOLD_WINDOW = 2 * ADS1292R_ALGO_SAMPLE_RATE;  // initial threshold window
static const uint8_t  QRS_PEAKS_PER_RATE   = 5;    // peaks (4 R-R intervals) averaged per heart-rate update
// Envelope, maxima and refractory windows are shorter than TI's 32/25/30 so the
// detector follows rates up to ~200 bpm (the originals capped it near 136 bpm)
static const uint8_t  QRS_MAXIMA_WINDOW    = 15;   // samples searched for the peak after a threshold crossing
static const uint8_t  QRS_SKIP_WINDOW      = 20;   // refractory samples after a peak
static const uint16_t QRS_NO_PEAK_TIMEOUT  = 3 * ADS1292R_ALGO_SAMPLE_RATE;  // reset after 3 s without a beat
static const uint8_t  QRS_MAX_RATE         = 250;

// Respiration-rate detector
static const uint16_t RESP_WINDOW          = 4 * ADS1292R_ALGO_SAMPLE_RATE;  // amplitude re-evaluation period
static const int32_t  RESP_MIN_AMPLITUDE   = 400;   // peak-to-peak over the last two windows needed to lock
static const uint16_t RESP_MIN_PERIOD      = 40;    // 187 breaths/min
static const uint16_t RESP_MAX_PERIOD      = 1300;  // ~5.8 breaths/min
static const uint16_t RESP_COUNT_WRAP      = 1500;
static const uint8_t  RESP_EDGE_SKIP       = 4;     // samples ignored after an edge (hysteresis)

// 40 Hz low-pass, 161 taps, Q15, 125 SPS
static const int16_t ECG_COEFFS[ADS1292R_ALGO_FIR_TAPS] ADS1292R_COEFF_ATTR = {
       -72,    122,    -31,    -99,    117,      0,   -121,    105,     34,   -137,     84,     70,
      -146,     55,    104,   -147,     20,    135,   -137,    -21,    160,   -117,    -64,    177,
       -87,   -108,    185,    -48,   -151,    181,      0,   -188,    164,     54,   -218,    134,
       112,   -238,     90,    171,   -244,     33,    229,   -235,    -36,    280,   -208,   -115,
       322,   -161,   -203,    350,    -92,   -296,    361,      0,   -391,    348,    117,   -486,
       305,    264,   -577,    225,    445,   -660,     93,    676,   -733,   -119,    991,   -793,
      -480,   1486,   -837,  -1226,   2561,   -865,  -4018,   9438,  20972,   9438,  -4018,   -865,
      2561,  -1226,   -837,   1486,   -480,   -793,    991,   -119,   -733,    676,     93,   -660,
       445,    225,   -577,    264,    305,   -486,    117,    348,   -391,      0,    361,   -296,
       -92,    350,   -203,   -161,    322,   -115,   -208,    280,    -36,   -235,    229,     33,
      -244,    171,     90,   -238,    112,    134,   -218,     54,    164,   -188,      0,    181,
      -151,    -48,    185,   -108,    -87,    177,    -64,   -117,    160,    -21,   -137,    135,
        20,   -147,    104,     55,   -146,     70,     84,   -137,     34,    105,   -121,      0,
       117,    -99,    -31,    122,    -72
};

// 2 Hz low-pass, 161 taps, Q15, 125 SPS
static const int16_t RESP_COEFFS[ADS1292R_ALGO_FIR_TAPS] ADS1292R_COEFF_ATTR = {
       120,    124,    126,    127,    127,    125,    122,    118,    113,    106,     97,     88,
        77,     65,     52,     38,     24,      8,     -8,    -25,    -42,    -59,    -76,    -93,
      -110,   -126,   -142,   -156,   -170,   -183,   -194,   -203,   -211,   -217,   -221,   -223,
      -223,   -220,   -215,   -208,   -198,   -185,   -170,   -152,   -132,   -108,    -83,    -55,
       -24,      8,     43,     80,    119,    159,    201,    244,    288,    333,    378,    424,
       470,    516,    561,    606,    650,    693,    734,    773,    811,    847,    880,    911,
       939,    964,    986,   1005,   1020,   1033,   1041,   1047,   1049,   1047,   1041,   1033,
      1020,   1005,    986,    964,    939,    911,    880,    847,    811,    773,    734,    693,
       650,    606,    561,    516,    470,    424,    378,    333,    288,    244,    201,    159,
       119,     80,     43,      8,    -24,    -55,    -83,   -108,   -132,   -152,   -170,   -185,
      -198,   -208,   -215,   -220,   -223,   -223,   -221,   -217,   -211,   -203,   -194,   -183,
      -170,   -156,   -142,   -126,   -110,    -93,    -76,    -59,    -42,    -25,     -8,      8,
        24,     38,     52,     65,     77,     88,     97,    106,    113,    118,    122,    125,
       127,    127,    126,    124,    120
};

ADS1292R_Algorithms::ADS1292R_Algorithms() {
    reset();
}

bool ADS1292R_Algorithms::begin(uint16_t sampleRate) {
    reset();
    return sampleRate == ADS1292R_ALGO_SAMPLE_RATE;
}

void ADS1292R_Algorithms::reset() {
    _ecgPrimed = false;
    _ecgPrevIn = 0;
    _ecgDcQ8   = 0;
    memset(_ecgBuf, 0, sizeof(_ecgBuf));
    _ecgIdx = 0;
    _ecgOut = 0;

    memset(_qrsHist, 0, sizeof(_qrsHist));
    _qrsSum     = 0;
    _qrsHistIdx = 0;
    memset(_qrsWin, 0, sizeof(_qrsWin));
    _qrsMaxDeriv     = 0;
    _qrsWindowCount  = 0;
    _qrsThresholdOld = 0;
    _qrsThresholdNew = 0;
    _qrsCrossed      = false;
    _qrsMaximaSearch = 0;
    _qrsSkipWindow   = 0;
    _qrsPeak         = 0;
    qrsRestart();

    _respPrimed = false;
    _respPrevIn = 0;
    _respDcQ4   = 0;
    memset(_respBuf, 0, sizeof(_respBuf));
    _respIdx = 0;
    _respOut = 0;

    memset(_respHist, 0, sizeof(_respHist));
    _respSum     = 0;
    _respHistIdx = 0;
    memset(_respPrev, 0, sizeof(_respPrev));
    _respTimeCount   = 0;
    _respPosCount    = 0;
    _respNegCount    = 0;
    _respPosPeriod   = 0;
    _respNegPeriod   = 0;
    _respSkip        = 0;
    _respLocked      = false;
    _respPosEdge     = false;
    _respNegEdge     = false;
    _respMin = _respPrevMin = INT32_MAX;
    _respMax = _respPrevMax = INT32_MIN;
    _respMidline     = 0;
    memset(_respPeriods, 0, sizeof(_respPeriods));
    _respPeriodCount = 0;
    _respRate        = 0;
}

void ADS1292R_Algorithms::processSample(int32_t ecgCounts, int32_t respCounts) {
    processEcg(ecgCounts);
    processResp(respCounts);
}

// ============================================================================
// Shared helpers
// ============================================================================

int16_t ADS1292R_Algorithms::saturate16(int32_t v) {
    if (v > INT16_MAX) return INT16_MAX;
    if (v < INT16_MIN) return INT16_MIN;
    return (int16_t)v;
}

// Direct-form FIR over a circular buffer; buf[newest] is the latest sample
int16_t ADS1292R_Algorithms::firFilter(const int16_t* coeffs, const int16_t* buf, uint8_t newest) {
    int32_t acc = 0;
    uint8_t j   = newest;
    for (uint8_t k = 0; k < ADS1292R_ALGO_FIR_TAPS; k++) {
        acc += (int32_t)ADS1292R_COEFF(coeffs, k) * buf[j];
        j = (j == 0) ? ADS1292R_ALGO_FIR_TAPS - 1 : j - 1;
    }
    if (acc > 0x3FFFFFFFL) acc = 0x3FFFFFFFL;
    if (acc < -0x40000000L) acc = -0x40000000L;
    return (int16_t)(acc >> 15);  // Q30 -> Q15
}

// ============================================================================
// ECG: DC block -> 40 Hz FIR -> QRS detection
// ============================================================================

void ADS1292R_Algorithms::processEcg(int32_t ecgCounts) {
    int32_t x = ecgCounts >> ADS1292R_ALGO_ECG_SHIFT;
    if (!_ecgPrimed) {
        _ecgPrevIn = x;  // avoid a huge start-up step from the electrode offset
        _ecgPrimed = true;
    }

    // First-order DC blocker, pole at 1 - 1/128 (0.16 Hz at 125 SPS), state kept in Q8
    _ecgDcQ8   = (x - _ecgPrevIn) * 256 + _ecgDcQ8 - (_ecgDcQ8 >> 7);
    _ecgPrevIn = x;

    _ecgBuf[_ecgIdx] = saturate16(_ecgDcQ8 >> 8);
    _ecgOut          = firFilter(ECG_COEFFS, _ecgBuf, _ecgIdx);
    _ecgIdx          = (_ecgIdx + 1 == ADS1292R_ALGO_FIR_TAPS) ? 0 : _ecgIdx + 1;

    // 16-sample (128 ms) moving sum as the QRS energy envelope
    _qrsSum += _ecgOut - _qrsHist[_qrsHistIdx];
    _qrsHist[_qrsHistIdx] = _ecgOut;
    _qrsHistIdx = (_qrsHistIdx + 1) & 15;

    qrsDetect(_qrsSum >> 2);
}

void ADS1292R_Algorithms::qrsDetect(int32_t smoothed) {
    _qrsWin[3] = _qrsWin[2];
    _qrsWin[2] = _qrsWin[1];
    _qrsWin[1] = _qrsWin[0];
    _qrsWin[0] = smoothed;

    int32_t deriv = _qrsWin[1] - _qrsWin[3];
    if (deriv < 0) deriv = -deriv;

    if (deriv > _qrsMaxDeriv) _qrsMaxDeriv = deriv;

    // Every 2 s, re-seed the threshold at 70% of the steepest slope seen
    if (++_qrsWindowCount >= QRS_THRESHOLD_WINDOW) {
        _qrsThresholdOld = (_qrsMaxDeriv * 7) / 10;
        _qrsThresholdNew = _qrsThresholdOld;
        _qrsArmed        = true;
        _qrsMaxDeriv     = 0;
        _qrsWindowCount  = 0;
    }

    if (_qrsArmed) qrsThresholdCheck(deriv);
}

void ADS1292R_Algorithms::qrsRestart() {
    _qrsSampleCount  = 0;
    _qrsPeaks        = 0;
    _qrsLastIndex    = 0;
    _qrsIntervalSum  = 0;
    _qrsMaximaSum    = 0;
    _qrsCounting     = false;
    _qrsPeakDetected = false;
    _qrsArmed        = false;
    _qrsNoPeak       = 0;
    _heartRate       = 0;
}

void ADS1292R_Algorithms::qrsThresholdCheck(int32_t value) {
    if (_qrsCrossed) {
        // Above threshold: track the maximum for QRS_MAXIMA_WINDOW samples
        _qrsSampleCount++;
        if (value > _qrsPeak) _qrsPeak = value;
        if (++_qrsMaximaSearch >= QRS_MAXIMA_WINDOW) {
            _qrsMaximaSum   += _qrsPeak;
            _qrsMaximaSearch = 0;
            _qrsCrossed      = false;
            _qrsPeakDetected = true;
        }
    } else if (_qrsPeakDetected) {
        // Refractory period after a peak
        _qrsSampleCount++;
        if (++_qrsSkipWindow >= QRS_SKIP_WINDOW) {
            _qrsSkipWindow   = 0;
            _qrsPeakDetected = false;
        }

        if (_qrsPeaks == QRS_PEAKS_PER_RATE) {
            uint16_t avgInterval = _qrsIntervalSum / (QRS_PEAKS_PER_RATE - 1);
            if (avgInterval > 0) {
                uint16_t hr = (60U * ADS1292R_ALGO_SAMPLE_RATE + avgInterval / 2) / avgInterval;
                _heartRate  = hr > QRS_MAX_RATE ? QRS_MAX_RATE : (uint8_t)hr;
            }

            // Adapt the threshold to 70% of the average peak, but never more than 4x the seed
            int32_t newThreshold = ((_qrsMaximaSum / QRS_PEAKS_PER_RATE) * 7) / 10;
            _qrsThresholdNew = (newThreshold > 4 * _qrsThresholdOld) ? _qrsThresholdOld : newThreshold;

            _qrsSampleCount = 0;
            _qrsPeaks       = 0;
            _qrsLastIndex   = 0;
            _qrsIntervalSum = 0;
            _qrsMaximaSum   = 0;
            _qrsCounting    = false;
        }
    } else if (value > _qrsThresholdNew) {
        // Threshold crossing: record the beat position
        _qrsCounting = true;
        _qrsSampleCount++;
        _qrsPeaks++;
        _qrsCrossed = true;
        _qrsPeak    = value;
        _qrsNoPeak  = 0;
        if (_qrsPeaks >= 2) _qrsIntervalSum += _qrsSampleCount - _qrsLastIndex;
        _qrsLastIndex = _qrsSampleCount;
    } else {
        if (_qrsCounting) _qrsSampleCount++;
        if (++_qrsNoPeak > QRS_NO_PEAK_TIMEOUT) qrsRestart();
    }
}

// ============================================================================
// Respiration: DC block -> 2 Hz FIR -> zero-crossing rate detection
// ============================================================================

void ADS1292R_Algorithms::processResp(int32_t respCounts) {
    if (!_respPrimed) {
        _respPrevIn = respCounts;
        _respPrimed = true;
    }

    // DC blocker, pole at 1 - 1/512 (0.04 Hz at 125 SPS) so slow breathing passes; state in Q4
    _respDcQ4   = (respCounts - _respPrevIn) * 16 + _respDcQ4 - (_respDcQ4 >> 9);
    _respPrevIn = respCounts;

    _respBuf[_respIdx] = saturate16(_respDcQ4 >> 4);
    _respOut           = firFilter(RESP_COEFFS, _respBuf, _respIdx);
    _respIdx           = (_respIdx + 1 == ADS1292R_ALGO_FIR_TAPS) ? 0 : _respIdx + 1;

    // 64-sample moving sum smooths the waveform before edge detection
    _respSum += _respOut - _respHist[_respHistIdx];
    _respHist[_respHistIdx] = _respOut;
    _respHistIdx = (_respHistIdx + 1) & 63;

    respRateDetect(_respSum >> 1);
}

void ADS1292R_Algorithms::respRateDetect(int32_t wave) {
    if (++_respPosCount > RESP_COUNT_WRAP) _respPosCount = 0;
    if (++_respNegCount > RESP_COUNT_WRAP) _respNegCount = 0;

    if (wave < _respMin) _respMin = wave;
    if (wave > _respMax) _respMax = wave;

    // Re-evaluate the breathing amplitude over the last two windows, so a single
    // artefact ages out instead of pinning the thresholds forever
    if (++_respTimeCount >= RESP_WINDOW) {
        _respTimeCount = 0;
        int32_t lo = _respMin < _respPrevMin ? _respMin : _respPrevMin;
        int32_t hi = _respMax > _respPrevMax ? _respMax : _respPrevMax;
        _respPrevMin = _respMin;
        _respPrevMax = _respMax;
        _respMin = _respMax = wave;

        if (hi - lo > RESP_MIN_AMPLITUDE) {
            _respMidline = lo + (hi - lo) / 2;
            if (!_respLocked) {
                _respLocked  = true;
                _respPrev[0] = _respPrev[1] = wave;
            }
        } else {
            _respLocked      = false;
            _respRate        = 0;
            _respPeriodCount = 0;
        }
    }

    if (!_respLocked) return;

    int32_t twoBack = _respPrev[1];
    _respPrev[1] = _respPrev[0];
    _respPrev[0] = wave;

    if (_respSkip > 0) {
        _respSkip--;
        return;
    }

    // Each edge direction measures one full breath period
    if (twoBack < _respMidline && wave > _respMidline) {
        if (_respPosCount > RESP_MIN_PERIOD && _respPosCount < RESP_MAX_PERIOD) {
            _respPosEdge   = true;
            _respPosPeriod = _respPosCount;
            _respSkip      = RESP_EDGE_SKIP;
        }
        _respPosCount = 0;
    } else if (twoBack > _respMidline && wave < _respMidline) {
        if (_respNegCount > RESP_MIN_PERIOD && _respNegCount < RESP_MAX_PERIOD) {
            _respNegEdge   = true;
            _respNegPeriod = _respNegCount;
            _respSkip      = RESP_EDGE_SKIP;
        }
        _respNegCount = 0;
    }

    if (_respPosEdge && _respNegEdge) {
        _respPosEdge = false;
        _respNegEdge = false;

        // Accept the pair only if both edges agree on the period (within 25%)
        int16_t diff = (int16_t)_respPosPeriod - (int16_t)_respNegPeriod;
        if (diff < 0) diff = -diff;
        if ((uint16_t)diff * 4 <= _respPosPeriod) {
            _respPeriods[_respPeriodCount++] = _respPosPeriod;
            _respPeriods[_respPeriodCount++] = _respNegPeriod;

            if (_respPeriodCount >= 8) {
                _respPeriodCount = 0;
                uint32_t sum = 0;
                for (uint8_t i = 0; i < 8; i++) sum += _respPeriods[i];
                uint16_t avgPeriod = sum >> 3;
                _respRate = (uint8_t)((60UL * ADS1292R_ALGO_SAMPLE_RATE + avgPeriod / 2) / avgPeriod);
            }
        }
    }
}
