#include "../headers/utils.h"
#include "../headers/lpf.h"
#include <math.h>

float LPFNext(LPF* lpf, LPFState* state, LPFParams* params, float input, float sampleRate) {

    float alpha = 1.0f - expf(-2.0f * M_PI * params->cutoff / sampleRate);
    
    float feed = input - params->resonance * state->values[lpf->poles-1];
    for (int i = 0; i < lpf->poles; i++) {
        state->values[i] += alpha * (feed - state->values[i]);
        feed = state->values[i];
    }

    return state->values[lpf->poles-1];
}


