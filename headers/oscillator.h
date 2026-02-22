#pragma once
#include <stdint.h>
typedef struct Oscillator Oscillator;
typedef struct OscillatorState OscillatorState;
typedef union OscillatorParams OscillatorParams;

union OscillatorParams {
    struct {
        float freq;
        float freqBase;
        float gain;
    };
    float paramsArray[4];
};

struct Oscillator{
    float (*get)(OscillatorState*, OscillatorParams*, float);
    float detune;    
    float phaseOffset; 
    
    OscillatorParams baseParams;

};
struct OscillatorState{
    float phase;
    float last;
    int8_t used;
};



float osciGet(Oscillator* osci, OscillatorState* state, OscillatorParams*, float sr);
void osciAdvance(Oscillator* osci, OscillatorState* state, OscillatorParams* params, float sr);
float osciNext(Oscillator* osci, OscillatorState* state, OscillatorParams* params, float input, float sr);

float squareWave(OscillatorState* state, OscillatorParams*, float sr);
float pulseWave(OscillatorState* state,OscillatorParams*, float sr);
float triangleWave(OscillatorState* state,OscillatorParams*, float sr);
float sineWave(OscillatorState* state, OscillatorParams*,float sr);
float sawWave(OscillatorState* state, OscillatorParams*,float sr);

float polyblepSaw(OscillatorState* state, OscillatorParams*,float sr);

float polyblepSquare(OscillatorState* state, OscillatorParams*,float sampleRate);
float polyblepPulse(OscillatorState* state, OscillatorParams*,float sampleRate);
float polyblampTriangle(OscillatorState* state, OscillatorParams*,float sampleRate);
float polyblepTriangle(OscillatorState* state, OscillatorParams*,float sampleRate);
float noise(OscillatorState* state, OscillatorParams*,float sampleRate);