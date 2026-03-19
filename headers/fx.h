#pragma once

#include <stdint.h>

#define MAX_DELAY_SAMPLES 8192
#define MAX_CHORUS_SAMPLES 4096

typedef struct Distortion Distortion;
typedef union DistortionParams DistortionParams;
typedef struct Delay Delay;
typedef union DelayParams DelayParams;
typedef struct DelayState DelayState;
typedef struct Chorus Chorus;
typedef union ChorusParams ChorusParams;
typedef struct ChorusState ChorusState;

union DistortionParams {
    struct {
        float drive;
        float mix;
        float outputGain;
    };
    float paramsArray[3];
};

struct Distortion {
    DistortionParams baseParams;
};

union DelayParams {
    struct {
        float time;
        float feedback;
        float mix;
        float tone;
    };
    float paramsArray[4];
};

struct Delay {
    DelayParams baseParams;
};

struct DelayState {
    float* left;
    float* right;
    int writeIndex;
    int capacity;
    int used;
};

union ChorusParams {
    struct {
        float rate;
        float depth;
        float mix;
        float baseDelay;
    };
    float paramsArray[4];
};

struct Chorus {
    ChorusParams baseParams;
};

struct ChorusState {
    float* left;
    float* right;
    float phase;
    int writeIndex;
    int capacity;
    int used;
};

void distortionProcess(void* preset, DistortionParams* params, void* state, float inputL, float inputR, float* outL, float* outR);
void delayProcess(void* preset, DelayParams* params, DelayState* state, float sampleRate, float inputL, float inputR, float* outL, float* outR);
void chorusProcess(void* preset, ChorusParams* params, ChorusState* state, float sampleRate, float inputL, float inputR, float* outL, float* outR);
