#include "../headers/oscillator.h"
#include "../headers/utils.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>


float osciGet(Oscillator* osci, OscillatorState* state, OscillatorParams* params,float sr) {
    float ret = osci->get(state, params, sr);

    return ret;
}
void osciAdvance(Oscillator* osci, OscillatorState* state, OscillatorParams* params, float sr) {

    state->phase += params->freq*powf(2.0f, osci->detune / 1200.0f)/sr;
    if (state->phase >= 1.0f) state->phase -= 1.0f;

    
}
float osciNext(Oscillator* osci, OscillatorState* state, OscillatorParams* params, float input, float sr) {
    (void)input;
    float ret = osciGet(osci, state, params, sr);
    state->last = ret;

    osciAdvance(osci, state, params, sr); 
 
    return ret*params->gain;
}


float squareWave(OscillatorState* state, OscillatorParams* params, float sr) {
    (void)sr;
    (void)params;
    return state->phase < 0.5f ? 1.0f : -1.0f;
}

//float pulseWave(OscillatorState* state, float sr) {
  //  return state->phase < ((float*)state->data)[0] ? 1.0f : -1.0f;
//}
float triangleWave(OscillatorState* state, OscillatorParams* params, float sr) {
    (void)sr;
    (void)params;
    return 2.0f * fabsf(2.0f * state->phase - 1.0f) - 1.0f;
}
float sineWave(OscillatorState* state, OscillatorParams* params, float sr) {
    (void)sr;
    (void)params;
    return sinf(2.0f * M_PI * state->phase);
}
float sawWave(OscillatorState* state, OscillatorParams* params, float sr) {
    (void)sr;
    (void)params;
    return 2.0 * state->phase - 1.0;
}

static float polyBLEP(float t, float dt) {
    if (t < dt) {
        t = t / dt;
        return t + t - t*t - 1.0f;  // t*(2 - t) -1
    } else if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return t*t + t + t + 1.0f;  // t*t + 2*t +1
    } else {
        return 0.0f;
    }
}

float polyblepSaw(OscillatorState* state,  OscillatorParams* params, float sr) {
    float dt = params->freq / sr;
    float t = state->phase;
    float y = 2.0f * t - 1.0f;            
    y -= polyBLEP(t, dt);                 
    return y;
}

float polyblepSquare(OscillatorState* state,  OscillatorParams* params,float sampleRate) {
    float t  = state->phase;
    float dt = params->freq / sampleRate;

    float y = (t < 0.5f) ? 1.0f : -1.0f;

    // BLEP at rising edge (phase = 0)
    y += polyBLEP(t, dt);

    // BLEP at falling edge (phase = 0.5)
    float t2 = t + 0.5f;
    if (t2 >= 1.0f) t2 -= 1.0f;
    y -= polyBLEP(t2, dt);
    y *= 1.0f / (1.0f - dt);
    return y;
}

/*
float polyblepPulse(OscillatorState* state, float sampleRate) {
    float t  = state->phase;
    float dt = state->freq / sampleRate;

    //float y = (t < ((float*)state->data)[0]) ? 1.0f : -1.0f;

    // BLEP at rising edge (phase = 0)
    y += polyBLEP(t, dt);

    // BLEP at falling edge (phase = 0.5)
    float t2 = t + 0.5f;
    if (t2 >= 1.0f) t2 -= 1.0f;
    y -= polyBLEP(t2, dt);
    y *= 1.0f / (1.0f - dt);
    return y;
}
*/

static float polyBLAMP(float t, float dt)
{
    if (t < dt) {
        t /= dt;
        // cubic polynomial
        return (t*t*t)/6.0f - (t*t)/2.0f + t/2.0f - 1.0f/6.0f;
    }
    else if (t > 1.0f - dt) {
        t = (t - 1.0f) / dt;
        return (t*t*t)/6.0f + (t*t)/2.0f + t/2.0f + 1.0f/6.0f;
    }
    else {
        return 0.0f;
    }
}

//broken but its fine i can fix it later
float polyblampTriangle(OscillatorState* state,  OscillatorParams* params, float sampleRate)
{
    float t  = state->phase;
    float dt = params->freq / sampleRate;

    // safety clamp
    if (dt > 0.5f)
        dt = 0.5f;

    // base triangle
    float y = 4.0f * fabsf(t - 0.5f) - 1.0f;

    y -= polyBLAMP(t, dt);
    y += polyBLAMP(fmodf(t + 0.5f, 1.0f), dt);
    printf("%f\n",y);
    return y/2;
}

float polyblepTriangle(OscillatorState* state,  OscillatorParams* params,float sampleRate) {
    float dt = params->freq / sampleRate;

    // 1. Get band-limited square
    float sq = polyblepSquare(state, params, sampleRate);

    // 2. Leaky integrator
    float out = dt * sq + (1.0f - dt) * state->last;
    out *= 1;
    return out;
}
float noise(OscillatorState* state, OscillatorParams*, float sampleRate) {
    (void)state;
    (void)sampleRate;
    return (float)rand() / (float)RAND_MAX;
}