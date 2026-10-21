#include <waveform_analyzer.h>
#include "math.h"
#include <stdbool.h>

#define WAVE_PI 3.14159265f
#define WAVE_MIN_VPP_V 0.02f
#define WAVE_SAWTOOTH_H2 0.10f
#define WAVE_SINE_HARM_MAX 0.05f
static float s_Re[WAVE_MAX_LEN];
static float s_Im[WAVE_MAX_LEN];

static void wave_fill_windowed(uint32_t len, float mean)
{
    for (uint32_t i = 0; i < len; i++)
    {
        float w = 0.5f - 0.5f * cosf(2.0f * WAVE_PI * (float)i / (float)len);
        s_Re[i] = (s_Re[i] - mean) * w;
        s_Im[i] = 0.0f;
    }
}

static void wave_fft(uint32_t n)
{
    for (uint32_t i = 1, j = 0; i < n; i++)
    {
        uint32_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
        {
            j ^= bit;
        }
        j ^= bit;
        if (i < j)
        {
            float t = s_Re[i];
            s_Re[i] = s_Re[j];
            s_Re[j] = t;
            t = s_Im[i];
            s_Im[i] = s_Im[j];
            s_Im[j] = t;
        }
    }

    for (uint32_t len = 2; len <= n; len <<= 1)
    {
        uint32_t half = len >> 1;
        float ang = -2.0f * WAVE_PI / (float)len;
        float wr_step = cosf(ang);
        float wi_step = sinf(ang);

        for (uint32_t i = 0; i < n; i += len)
        {
            float wr = 1.0f;
            float wi = 0.0f;
            for (uint32_t k = 0; k < half; k++)
            {
                uint32_t a = i + k;
                uint32_t b = a + half;
                float xr = s_Re[b] * wr - s_Im[b] * wi;
                float xi = s_Re[b] * wi + s_Im[b] * wr;
                s_Re[b] = s_Re[a] - xr;
                s_Im[b] = s_Im[a] - xi;
                s_Re[a] += xr;
                s_Im[a] += xi;

                float next_wr = wr * wr_step - wi * wi_step;
                wi = wr * wi_step + wi * wr_step;
                wr = next_wr;
            }
        }
    }
}

static float wave_power(uint32_t k)
{
    return s_Re[k] * s_Re[k] + s_Im[k] * s_Im[k];
}

static float wave_amp_at(int bin, int half)
{
    if (bin < 1 || bin >= half)
    {
        return 0.0f;
    }
    return sqrtf(wave_power((uint32_t)bin));
}

static float wave_ratio(float part, float whole)
{
    return (whole > 1e-6f) ? (part / whole) : 0.0f;
}

static float wave_harmonic_ratio(uint32_t fund_bin, uint32_t order, float fund_amp, uint32_t half)
{
    return wave_ratio(wave_amp_at((int)(order * fund_bin), (int)half), fund_amp);
}

/* Period from mean-level rising crossings; unlike FFT bin interpolation it is not biased by the 2nd harmonic. */
static float wave_period_s(uint32_t len, float mean, float sample_interval_s)
{
    float first = 0.0f;
    float last = 0.0f;
    uint32_t count = 0;
    bool prev_above = s_Re[0] >= mean;

    for (uint32_t i = 1; i < len; i++)
    {
        bool above = s_Re[i] >= mean;
        if (!prev_above && above)
        {
            float pos = (float)(i - 1) + (mean - s_Re[i - 1]) / (s_Re[i] - s_Re[i - 1]);
            if (count == 0)
            {
                first = pos;
            }
            last = pos;
            count++;
        }
        prev_above = above;
    }

    if (count < 2)
    {
        return 0.0f;
    }
    return (last - first) / (float)(count - 1) * sample_interval_s;
}

waveformInfo wave_analyze(wave_sample_src_t src, void *ctx, uint32_t len, float sample_interval_s)
{
    waveformInfo info = {.waveform_type = ADC_WAVEFORM_UNKNOWN};

    for (uint32_t i = 0; i < len; i++)
    {
        s_Re[i] = src(ctx, i);
    }

    float vmin = s_Re[0];
    float vmax = s_Re[0];
    float sum = 0.0f;
    float sum_sq = 0.0f;
    for (uint32_t i = 0; i < len; i++)
    {
        float v = s_Re[i];
        if (v < vmin) vmin = v;
        if (v > vmax) vmax = v;
        sum += v;
        sum_sq += v * v;
    }
    info.vpp = vmax - vmin;
    info.mean = sum / (float)len;
    info.rms = sqrtf(sum_sq / (float)len);

    if (info.vpp < WAVE_MIN_VPP_V)
    {
        info.waveform_type = ADC_WAVEFORM_DC;
        return info;
    }

    float period_s = wave_period_s(len, info.mean, sample_interval_s);
    if (period_s <= 0.0f)
    {
        return info;
    }
    info.period = period_s * 1000.0f;
    info.frequency = 1.0f / period_s;

    wave_fill_windowed(len, info.mean);
    wave_fft(len);

    uint32_t half = len / 2U;
    uint32_t peak_idx = 1;
    float peak_pow = 0.0f;
    for (uint32_t k = 1; k < half; k++)
    {
        float p = wave_power(k);
        if (p > peak_pow)
        {
            peak_pow = p;
            peak_idx = k;
        }
    }

    float fund_amp = wave_amp_at((int)peak_idx, (int)half);
    info.h2_ratio = wave_harmonic_ratio(peak_idx, 2U, fund_amp, half);
    info.h3_ratio = wave_harmonic_ratio(peak_idx, 3U, fund_amp, half);
    info.h5_ratio = wave_harmonic_ratio(peak_idx, 5U, fund_amp, half);

    if (info.h2_ratio > WAVE_SAWTOOTH_H2)
    {
        info.waveform_type = ADC_WAVEFORM_SAWTOOTH;
    }
    else if (info.h3_ratio < WAVE_SINE_HARM_MAX && info.h5_ratio < WAVE_SINE_HARM_MAX)
    {
        info.waveform_type = ADC_WAVEFORM_SINE;
    }
    else if (info.h3_ratio > 1.0f / 5.0f)
    {
        info.waveform_type = ADC_WAVEFORM_SQUARE;
    }
    else if (info.h3_ratio > 1.0f / 15.0f)
    {
        info.waveform_type = ADC_WAVEFORM_TRIANGLE;
    }

    return info;
}
