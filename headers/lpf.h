#pragma once
#include <stdint.h>

#define MAX_POLES 16

typedef struct LPF LPF;
typedef struct LPFState LPFState;
typedef union LPFParams LPFParams;
typedef enum FilterMode FilterMode;

float LPFNext(LPF* lpf, LPFState* state, LPFParams* params, float input, float sampleRate);

enum FilterMode {
    FILTER_LADDER,
    FILTER_BIQUAD_LP,
    FILTER_BIQUAD_HP,
    FILTER_BIQUAD_PEAK,
};

union LPFParams {
    struct {
        float cutoff;
        float resonance;
        float gainDB;
    };
    float paramsArray[3];
};

struct LPF {
    FilterMode mode;
    int8_t poles;
    LPFParams baseParams;
};
struct LPFState {
    float values[MAX_POLES];
    float x1;
    float x2;
    float y1;
    float y2;
    int8_t used;
    

};

