#ifndef WAVEFORM_ANALYZER_H
#define WAVEFORM_ANALYZER_H

#include "stdint.h"

#define WAVE_MAX_LEN 1024U

typedef enum
{
    ADC_WAVEFORM_DC = 0,
    ADC_WAVEFORM_SINE = 1,
    ADC_WAVEFORM_SQUARE = 2,
    ADC_WAVEFORM_TRIANGLE = 3,
    ADC_WAVEFORM_SAWTOOTH = 4,
    ADC_WAVEFORM_UNKNOWN = 255
} ADC_WaveformType;

typedef struct
{
    ADC_WaveformType waveform_type;
    float frequency;   // Hz, 0 for DC
    float period;      // ms, 0 for DC
    float vpp;         // V
    float mean;        // V
    float rms;         // V
    float h2_ratio;    // 2nd harmonic / fundamental
    float h3_ratio;    // 3rd harmonic / fundamental
    float h5_ratio;    // 5th harmonic / fundamental
} waveformInfo;

/* Returns the sample at index, in volts. */
typedef float (*wave_sample_src_t)(void *ctx, uint32_t index);

/* len must be a power of two and no more than WAVE_MAX_LEN. */
waveformInfo wave_analyze(wave_sample_src_t src, void *ctx, uint32_t len, float sample_interval_s);

#endif
