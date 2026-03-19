#include "../headers/utils.h"
#include "../headers/lpf.h"
#include <math.h>

static float clampCutoff(float cutoff, float sampleRate) {
    if (cutoff < 10.0f) {
        return 10.0f;
    }
    if (cutoff > sampleRate * 0.45f) {
        return sampleRate * 0.45f;
    }
    return cutoff;
}

static float biquadNext(LPF* lpf, LPFState* state, LPFParams* params, float input, float sampleRate) {
    const float cutoff = clampCutoff(params->cutoff, sampleRate);
    const float q = fmaxf(params->resonance, 0.1f);
    const float omega = 2.0f * M_PI * cutoff / sampleRate;
    const float sn = sinf(omega);
    const float cs = cosf(omega);
    const float alpha = sn / (2.0f * q);
    const float A = powf(10.0f, params->gainDB / 40.0f);
    float b0 = 0.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    float a0 = 1.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;

    switch (lpf->mode) {
        case FILTER_BIQUAD_LP:
            b0 = (1.0f - cs) * 0.5f;
            b1 = 1.0f - cs;
            b2 = (1.0f - cs) * 0.5f;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cs;
            a2 = 1.0f - alpha;
            break;
        case FILTER_BIQUAD_HP:
            b0 = (1.0f + cs) * 0.5f;
            b1 = -(1.0f + cs);
            b2 = (1.0f + cs) * 0.5f;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cs;
            a2 = 1.0f - alpha;
            break;
        case FILTER_BIQUAD_PEAK:
            b0 = 1.0f + alpha * A;
            b1 = -2.0f * cs;
            b2 = 1.0f - alpha * A;
            a0 = 1.0f + alpha / A;
            a1 = -2.0f * cs;
            a2 = 1.0f - alpha / A;
            break;
        case FILTER_LADDER:
            break;
    }

    const float out = (b0 / a0) * input +
        (b1 / a0) * state->x1 +
        (b2 / a0) * state->x2 -
        (a1 / a0) * state->y1 -
        (a2 / a0) * state->y2;

    state->x2 = state->x1;
    state->x1 = input;
    state->y2 = state->y1;
    state->y1 = out;
    return out;
}

float LPFNext(LPF* lpf, LPFState* state, LPFParams* params, float input, float sampleRate) {
    if (lpf->mode != FILTER_LADDER) {
        return biquadNext(lpf, state, params, input, sampleRate);
    }

    float alpha = 1.0f - expf(-2.0f * M_PI * clampCutoff(params->cutoff, sampleRate) / sampleRate);
    
    float feed = input - params->resonance * state->values[lpf->poles-1];
    for (int i = 0; i < lpf->poles; i++) {
        state->values[i] += alpha * (feed - state->values[i]);
        feed = state->values[i];
    }

    return state->values[lpf->poles-1];
}

