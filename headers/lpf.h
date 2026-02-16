#pragma once
#include "all.h"

#define MAX_POLES 16

typedef struct LPF LPF;
typedef struct LPFState LPFState;
typedef struct LPFParams LPFParams;

float LPFNext(LPF* lpf, LPFState* state, float input, float sampleRate);


struct LPFParams {
    float cutoff;
    float resonance;
};

struct LPF {
    
    int8_t poles;
    LPFParams baseParams;
};
struct LPFState {
    float values[MAX_POLES];
    LPFParams params;

};


