#pragma once
#include "all.h"

#define MAX_POLES 16

typedef struct LPF LPF;
typedef struct LPFState LPFState;
typedef union LPFParams LPFParams;

float LPFNext(LPF* lpf, LPFState* state, float input, float sampleRate);


union LPFParams {
    struct {
        float cutoff;
        float resonance;
    };
    float paramsArray[2];
};

struct LPF {
    
    int8_t poles;
    LPFParams baseParams;
};
struct LPFState {
    float values[MAX_POLES];
    LPFParams params;

};


