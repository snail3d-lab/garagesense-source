#pragma once
#include <Arduino.h>
#include "config.h"

// Garage-door detector — energy envelope, no ML model.
//
// The shape it listens for is a garage door, which is the opposite of a
// transient sound like a bark or a clap:
//   transient : 50-380 ms burst, sharp attack, fast decay, repeats in sequences
//   door      : 2-20 s SUSTAINED motor, slow ramp, plateau, abrupt stop, once
//
// So this detector looks for exactly that: energy above the noise floor that
// stays there, with low variance, then stops. One event per cycle.
//
//   1. one-pole high-pass (~120 Hz) removes HVAC rumble and traffic
//   2. per-frame RMS -> dB envelope (16 ms frames)
//   3. adaptive noise floor tracks the quiet ambient level
//   4. SUSTAINED check: above threshold for >= DOOR_MIN_MS
//   5. END check: falls below threshold and stays below DOOR_END_QUIET_MS
//   6. duration sanity: between DOOR_MIN_MS and DOOR_MAX_MS
//   7. optional LEARNED template: normalized correlation against a recorded
//      envelope, so this particular door is distinguishable from a lawnmower
//
// One detector class, swapped in by main.cpp.

class DoorDetector {
public:
    static const int HIST_LEN = 40;          // ~640 ms of envelope for the web meter
    static const int TEMPLATE_LEN = 512;     // ~8.2 s of envelope at 16 ms/frame

    void begin() { reset(); }

    void reset() {
        _prevX = 0;
        _prevY = 0;
        _noiseDb = NOISE_FLOOR_DB;
        _state = IDLE;
        _aboveMs = 0;
        _quietMs = 0;
        _eventLatched = false;
        for (int i = 0; i < HIST_LEN; i++) _hist[i] = -120.0f;
        _histIdx = 0;
    }

    void setMargin(float db) { _marginDb = db; }
    float marginDb() const { return _marginDb; }

    // Feed exactly FRAME_SAMPLES of 16-bit PCM.
    void processFrame(const int16_t *pcm, uint32_t nowMs) {
        float sumSq = 0;
        for (size_t i = 0; i < FRAME_SAMPLES; i++) {
            float x = (float)pcm[i];
            float y = x - _prevX + _hpA * _prevY;   // one-pole high-pass
            _prevX = x;
            _prevY = y;
            sumSq += y * y;
        }
        float rms = sqrtf(sumSq / (float)FRAME_SAMPLES);
        float db = 20.0f * log10f(rms / 32768.0f);
        if (db < -120.0f) db = -120.0f;
        _lastFrameDb = db;

        _hist[_histIdx] = db;
        _histIdx = (_histIdx + 1) % HIST_LEN;

        float threshold = _noiseDb + _marginDb;
        bool above = db > threshold;

        // Adaptive noise floor — only while quiet, so a running motor cannot
        // pull the floor up and mask its own tail.
        if (!above) {
            float coef = (db > _noiseDb) ? NOISE_RISE_COEF : NOISE_FALL_COEF;
            _noiseDb += (db - _noiseDb) * coef;
            if (_noiseDb < NOISE_FLOOR_DB) _noiseDb = NOISE_FLOOR_DB;
        }

        const uint32_t FRAME_MS = (uint32_t)(1000 * FRAME_SAMPLES / SAMPLE_RATE);

        // Recording a learn template: accumulate while energy is present.
        if (_recording) {
            if (_recLen < TEMPLATE_LEN) _recBuf[_recLen++] = db;
            if (!above) {
                _recQuietMs += FRAME_MS;
                if (_recQuietMs >= DOOR_END_QUIET_MS) { _recording = false; _recDone = true; }
            } else {
                _recQuietMs = 0;
            }
            return;   // learn mode does not raise events
        }

        if (_state == IDLE) {
            if (above) {
                _state = RUNNING;
                _aboveMs = FRAME_MS;
                _quietMs = 0;
                _peakDb = db;
                _sumDb = db; _nFrames = 1; _sumSqDb = db * db;
                _runBufLen = 0;
            }
        } else {   // RUNNING
            if (above) {
                _aboveMs += FRAME_MS;
                _quietMs = 0;
                if (db > _peakDb) _peakDb = db;
                _sumDb += db; _sumSqDb += db * db; _nFrames++;
                if (_runBufLen < TEMPLATE_LEN) _runBuf[_runBufLen++] = db;
            } else {
                _quietMs += FRAME_MS;
                if (_quietMs >= DOOR_END_QUIET_MS) {
                    finishRun(nowMs);
                    _state = IDLE;
                }
            }
        }
    }

    // True once per confirmed door cycle (auto-clears).
    bool eventDetected() {
        bool e = _eventLatched;
        _eventLatched = false;
        return e;
    }

    // ---- learn mode ----------------------------------------------------
    // Start recording the next door cycle as this install's template.
    void startLearn() {
        _recording = true;
        _recDone = false;
        _recLen = 0;
        _recQuietMs = 0;
    }
    bool learning() const { return _recording; }
    bool learnDone() { bool d = _recDone; _recDone = false; return d; }
    int templateLen() const { return _templateLen; }
    // Copy the captured run into the template. Returns the sample count.
    int commitTemplate() {
        _templateLen = _recLen > TEMPLATE_LEN ? TEMPLATE_LEN : _recLen;
        for (int i = 0; i < _templateLen; i++) _template[i] = _recBuf[i];
        return _templateLen;
    }
    void clearTemplate() { _templateLen = 0; }
    // Restore a template from NVS at boot.
    void loadTemplate(const float *buf, int n) {
        _templateLen = (n > TEMPLATE_LEN) ? TEMPLATE_LEN : n;
        for (int i = 0; i < _templateLen; i++) _template[i] = buf[i];
    }
    const float *templateBuf() const { return _template; }

    float noiseDb() const { return _noiseDb; }
    float thresholdDb() const { return _noiseDb + _marginDb; }
    float lastFrameDb() const { return _lastFrameDb; }
    const float *hist() const { return _hist; }
    int histIdx() const { return _histIdx; }
    uint32_t lastDurationMs() const { return _lastDurationMs; }

private:
    void finishRun(uint32_t nowMs) {
        uint32_t dur = _aboveMs;
        _lastDurationMs = dur;
        if (dur < DOOR_MIN_MS || dur > DOOR_MAX_MS) return;   // too short / too long

        // Low variance: a motor is a steady hum. A voice, a slammed door or a
        // passing truck swings wildly frame to frame.
        if (_nFrames > 1) {
            float mean = _sumDb / _nFrames;
            float var = (_sumSqDb / _nFrames) - (mean * mean);
            if (var > DOOR_MAX_VARIANCE) return;
        }

        // If a template was learned, require it to match.
        if (_templateLen > 8) {
            float c = correlate(_runBuf, _runBufLen, _template, _templateLen);
            _lastScore = c;
            if (c < DOOR_MATCH_MIN) return;
        } else {
            _lastScore = -1.0f;
        }

        _eventLatched = true;
    }

    // Normalized cross-correlation of two envelopes, resampled to a common
    // length. 1.0 = identical shape, 0 = unrelated. Length-insensitive on
    // purpose: the same door sounds the same but may run a beat longer.
    static float correlate(const float *a, int na, const float *b, int nb) {
        if (na < 4 || nb < 4) return 0.0f;
        const int N = 64;                    // fixed comparison grid
        float sa = 0, sb = 0;
        float va[N], vb[N];
        for (int i = 0; i < N; i++) {
            va[i] = a[(int)((long)i * na / N)];
            vb[i] = b[(int)((long)i * nb / N)];
            sa += va[i]; sb += vb[i];
        }
        sa /= N; sb /= N;
        float num = 0, da = 0, db2 = 0;
        for (int i = 0; i < N; i++) {
            float x = va[i] - sa, y = vb[i] - sb;
            num += x * y; da += x * x; db2 += y * y;
        }
        if (da <= 0 || db2 <= 0) return 0.0f;
        return num / sqrtf(da * db2);
    }

    enum State { IDLE, RUNNING };

    float _hpA = 1.0f / (1.0f + 2.0f * 3.14159265f * DOOR_HP_CUTOFF_HZ / (float)SAMPLE_RATE);
    float _prevX = 0, _prevY = 0;
    float _noiseDb = NOISE_FLOOR_DB;
    float _marginDb = DOOR_THRESHOLD_MARGIN_DB;
    State _state = IDLE;

    uint32_t _aboveMs = 0, _quietMs = 0;
    float _peakDb = -120.0f;
    float _sumDb = 0, _sumSqDb = 0;
    int _nFrames = 0;

    uint32_t _lastDurationMs = 0;
    float _lastScore = -1.0f;

    bool _eventLatched = false;
    float _lastFrameDb = -120.0f;
    float _hist[HIST_LEN];
    int _histIdx = 0;

    // learn-mode capture
    bool _recording = false, _recDone = false;
    float _recBuf[TEMPLATE_LEN];
    int _recLen = 0;
    uint32_t _recQuietMs = 0;

    // this install's learned door envelope
    float _template[TEMPLATE_LEN];
    int _templateLen = 0;

    // envelope of the run being evaluated, for correlation
    float _runBuf[TEMPLATE_LEN];
    int _runBufLen = 0;
};