#pragma once

#include <stdint.h>

typedef enum ADSRStage ADSRStage;
typedef struct ADSREnv ADSREnv;
typedef struct ADSREnvState ADSREnvState;


enum ADSRStage{
    ADSR_OFF,
    ADSR_ATTACK,
    ADSR_DECAY,
    ADSR_SUSTAIN,
    ADSR_RELEASE
};
struct ADSREnv {
    float attack;
    float decay;
    float sustain;
    float release;
    //this is a workaround to the fact that technically every instrument has to have an envelope, if this is 0 then
    //the envelope just returns 1
    int8_t active;
};

struct ADSREnvState{
    ADSRStage stage;
    float value;
    int8_t used;
    float releaseValue;

};

float adsrGet(ADSREnv* env, ADSREnvState* state, float sr);
void adsrAdvance(ADSREnv* env, ADSREnvState* state, float sr);
float adsrNext(ADSREnv* env, ADSREnvState* state, void*, float, float sr);
