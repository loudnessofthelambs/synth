#include "../headers/fx.h"
#include "../headers/utils.h"

#include <math.h>
#include <stddef.h>

static float clampf(float value, float minValue, float maxValue) {
    if (value < minValue) return minValue;
    if (value > maxValue) return maxValue;
    return value;
}

void distortionProcess(void* preset, DistortionParams* params, void* state, float inputL, float inputR, float* outL, float* outR) {
    (void)preset;
    (void)state;
    const float dryMix = 1.0f - clampf(params->mix, 0.0f, 1.0f);
    const float wetMix = clampf(params->mix, 0.0f, 1.0f);
    const float drive = fmaxf(params->drive, 0.01f);
    const float makeup = params->outputGain;
    const float wetL = tanhf(inputL * drive);
    const float wetR = tanhf(inputR * drive);

    *outL = (inputL * dryMix + wetL * wetMix) * makeup;
    *outR = (inputR * dryMix + wetR * wetMix) * makeup;
}

void delayProcess(void* preset, DelayParams* params, DelayState* state, float sampleRate, float inputL, float inputR, float* outL, float* outR) {
    (void)preset;
    if (state->left == NULL || state->right == NULL || state->capacity <= 1) {
        *outL = inputL;
        *outR = inputR;
        return;
    }
    const int delaySamples = (int)clampf(params->time * sampleRate, 1.0f, (float)(state->capacity - 1));
    const int readIndex = (state->writeIndex - delaySamples + state->capacity) % state->capacity;
    const float delayedL = state->left[readIndex];
    const float delayedR = state->right[readIndex];
    const float feedback = clampf(params->feedback, 0.0f, 0.98f);
    const float mix = clampf(params->mix, 0.0f, 1.0f);
    const float tone = clampf(params->tone, 0.0f, 1.0f);
    const float filteredL = delayedL * tone + inputL * (1.0f - tone);
    const float filteredR = delayedR * tone + inputR * (1.0f - tone);

    state->left[state->writeIndex] = inputL + filteredL * feedback;
    state->right[state->writeIndex] = inputR + filteredR * feedback;
    state->writeIndex = (state->writeIndex + 1) % state->capacity;

    *outL = inputL * (1.0f - mix) + delayedL * mix;
    *outR = inputR * (1.0f - mix) + delayedR * mix;
}

void chorusProcess(void* preset, ChorusParams* params, ChorusState* state, float sampleRate, float inputL, float inputR, float* outL, float* outR) {
    (void)preset;
    if (state->left == NULL || state->right == NULL || state->capacity <= 2) {
        *outL = inputL;
        *outR = inputR;
        return;
    }
    const float rate = fmaxf(params->rate, 0.01f);
    const float depthSamples = clampf(params->depth * sampleRate, 1.0f, (float)(state->capacity / 2));
    const float baseDelaySamples = clampf(params->baseDelay * sampleRate, 1.0f, (float)(state->capacity / 2));
    const float mod = 0.5f + 0.5f * sinf(2.0f * M_PI * state->phase);
    const int delaySamplesL = (int)clampf(baseDelaySamples + mod * depthSamples, 1.0f, (float)(state->capacity - 2));
    const int delaySamplesR = (int)clampf(baseDelaySamples + (1.0f - mod) * depthSamples, 1.0f, (float)(state->capacity - 2));
    const int readIndexL = (state->writeIndex - delaySamplesL + state->capacity) % state->capacity;
    const int readIndexR = (state->writeIndex - delaySamplesR + state->capacity) % state->capacity;
    const float wetL = state->left[readIndexL];
    const float wetR = state->right[readIndexR];
    const float mix = clampf(params->mix, 0.0f, 1.0f);

    state->left[state->writeIndex] = inputL;
    state->right[state->writeIndex] = inputR;
    state->writeIndex = (state->writeIndex + 1) % state->capacity;
    state->phase += rate / sampleRate;
    if (state->phase >= 1.0f) {
        state->phase -= 1.0f;
    }

    *outL = inputL * (1.0f - mix) + wetL * mix;
    *outR = inputR * (1.0f - mix) + wetR * mix;
}
