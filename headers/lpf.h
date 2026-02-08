#pragma once
#include "all.h"

#define MAX_POLES 16

typedef struct LPF LPF;
typedef struct LPFState LPFState;

float LPFNext(LPF* lpf, LPFState* state, float input, float sampleRate);


struct LPF {
    float baseCutoff;
    float envRange;
    float resonance;
    int8_t poles;
};
struct LPFState {
    float values[MAX_POLES];
    float cutoff;
};