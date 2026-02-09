#include "../headers/all.h"
#include "../headers/adsr.h"


float adsrGet(ADSREnv* env, ADSREnvState* state, float sr) {
    return state->value;
}
void adsrAdvance(ADSREnv* env, ADSREnvState* state, float sr) {
    
    switch (state->stage) {
    case ADSR_ATTACK:
        state->value += 1.0f / (env->attack * sr);
        if (state->value >= 1.0f) {
            state->value = 1.0f;
            state->stage = ADSR_DECAY;
        }
        break;

    case ADSR_DECAY:
        state->value -= (1.0f - env->sustain) / (env->decay * sr);
        if (state->value <= env->sustain) {
            state->value = env->sustain;
            state->stage = ADSR_SUSTAIN;
        }
        break;

    case ADSR_SUSTAIN:
        break;

    case ADSR_RELEASE:
        if (env->active == 0) {
                state->value = 0.0f;
                state->stage = ADSR_OFF;
                return;
            }
        state->value -= state->releaseValue / (env->release * sr);
        if (state->value <= 0.0f) {
            state->value = 0.0f;
            state->stage = ADSR_OFF;
        }
        
        break;

    case ADSR_OFF:
        state->value = 0.0f;
        break;
    }
    if (env->active == 0) {
        state->value = 1.0f;
        return;
    }
}

float adsrNext(ADSREnv* env, ADSREnvState* state, float sr) {
    float ret = state->value;
    adsrAdvance(env,state, sr);
    return ret;
}

float applyEnvLinear(ADSREnv* env, ADSREnvState* state, float val, float base, float sr) {
    float envVal = adsrNext(env, state, sr);
    return base + (envVal * val * env->modulAmount);
     //(1-fabsf(voice->instr->adsr.modulAmount))*sample
}