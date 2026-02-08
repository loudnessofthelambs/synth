#include "../headers/all.h"
#include "../headers/lpf.h"

float LPFNext(LPF* lpf, LPFState* state,float input, float sampleRate) {

    float alpha = 1.0f - expf(-2.0f * M_PI * state->cutoff / sampleRate);
    float feed = input - lpf->resonance * state->values[lpf->poles-1];
    for (int i = 0; i < lpf->poles; i++) {
        state->values[i] += alpha * (feed - state->values[i]);
        feed = state->values[i];
    }

    return state->values[lpf->poles-1];
}
