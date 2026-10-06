// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2017-2026 Ashwin Whitchurch, ProtoCentral Electronics
// SPDX-FileCopyrightText: Filter and QRS approach derived from Texas Instruments ADS1x9x ECG reference code
//
// Optional heart-rate and respiration-rate processing for the ADS1292R.
// Fixed-point, ~1 KB RAM, designed for 125 SPS input.
//
// For research and education only - not a medical device.
//
// https://github.com/Protocentral/protocentral-ads1292r-arduino

#ifndef PROTOCENTRAL_ADS1292R_ALGORITHMS_H
#define PROTOCENTRAL_ADS1292R_ALGORITHMS_H

#include <Arduino.h>

#define ADS1292R_ALGO_SAMPLE_RATE 125  ///< The filter coefficients are designed for this rate only
#define ADS1292R_ALGO_FIR_TAPS    161

#ifndef ADS1292R_ALGO_ECG_SHIFT
#define ADS1292R_ALGO_ECG_SHIFT 4  ///< Right shift from 24-bit ECG counts to the 16-bit filter input
#endif

class ADS1292R_Algorithms {
public:
    ADS1292R_Algorithms();

    /** @brief Reset all state. Returns false unless @p sampleRate is 125 SPS. */
    bool begin(uint16_t sampleRate = ADS1292R_ALGO_SAMPLE_RATE);

    /** @brief Clear filters and detectors (e.g. after a lead-off event). */
    void reset();

    /** @brief Feed one sample of each channel, as returned by Protocentral_ADS1292R::read(). */
    void processSample(int32_t ecgCounts, int32_t respCounts);

    /** @brief Feed ECG only (heart rate). */
    void processEcg(int32_t ecgCounts);

    /** @brief Feed respiration only (respiration rate). */
    void processResp(int32_t respCounts);

    int16_t filteredEcg() const { return _ecgOut; }      ///< 0.16 Hz - 40 Hz band-passed ECG
    int16_t filteredResp() const { return _respOut; }    ///< DC-removed, 2 Hz low-passed respiration
    uint8_t heartRate() const { return _heartRate; }     ///< Beats per minute, 0 until locked
    uint8_t respirationRate() const { return _respRate; } ///< Breaths per minute, 0 until locked

private:
    // ---- ECG chain ----
    bool    _ecgPrimed;
    int32_t _ecgPrevIn;
    int32_t _ecgDcQ8;
    int16_t _ecgBuf[ADS1292R_ALGO_FIR_TAPS];
    uint8_t _ecgIdx;
    int16_t _ecgOut;

    // ---- QRS detector ----
    int16_t  _qrsHist[16];
    int32_t  _qrsSum;
    uint8_t  _qrsHistIdx;
    int32_t  _qrsWin[4];        // smoothed samples n .. n-3
    int32_t  _qrsMaxDeriv;
    uint16_t _qrsWindowCount;
    int32_t  _qrsThresholdOld;
    int32_t  _qrsThresholdNew;
    bool     _qrsArmed;         // first 2 s threshold window has completed
    bool     _qrsCrossed;
    bool     _qrsPeakDetected;
    bool     _qrsCounting;
    uint16_t _qrsMaximaSearch;
    uint16_t _qrsSkipWindow;
    uint16_t _qrsNoPeak;
    uint16_t _qrsSampleCount;
    uint16_t _qrsLastIndex;
    uint8_t  _qrsPeaks;
    uint16_t _qrsIntervalSum;
    int32_t  _qrsPeak;
    int32_t  _qrsMaximaSum;
    uint8_t  _heartRate;

    // ---- Respiration chain ----
    bool    _respPrimed;
    int32_t _respPrevIn;
    int32_t _respDcQ4;
    int16_t _respBuf[ADS1292R_ALGO_FIR_TAPS];
    uint8_t _respIdx;
    int16_t _respOut;

    // ---- Respiration-rate detector ----
    int16_t  _respHist[64];
    int32_t  _respSum;
    uint8_t  _respHistIdx;
    int32_t  _respPrev[2];      // detector input n-1, n-2
    uint16_t _respTimeCount;
    uint16_t _respPosCount;
    uint16_t _respNegCount;
    uint16_t _respPosPeriod;
    uint16_t _respNegPeriod;
    uint8_t  _respSkip;
    bool     _respLocked;
    bool     _respPosEdge;
    bool     _respNegEdge;
    int32_t  _respMin, _respMax;          // current 4 s window
    int32_t  _respPrevMin, _respPrevMax;  // previous 4 s window
    int32_t  _respMidline;
    uint16_t _respPeriods[8];
    uint8_t  _respPeriodCount;
    uint8_t  _respRate;

    static int16_t firFilter(const int16_t* coeffs, const int16_t* buf, uint8_t newest);
    static int16_t saturate16(int32_t v);
    void qrsDetect(int32_t smoothed);
    void qrsThresholdCheck(int32_t value);
    void qrsRestart();
    void respRateDetect(int32_t wave);
};

#endif  // PROTOCENTRAL_ADS1292R_ALGORITHMS_H
