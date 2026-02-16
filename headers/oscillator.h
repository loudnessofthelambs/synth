#pragma once

typedef struct Oscillator Oscillator;
typedef struct OscillatorState OscillatorState;
typedef struct OscillatorParams OscillatorParams;

struct OscillatorParams {
    float freq;
    float freqBase;
    float gain;
};

struct Oscillator{
    float (*get)(OscillatorState*, float);
    float detune;    
    float phaseOffset; 
    
    OscillatorParams baseParams;

};
struct OscillatorState{
    float phase;
    float last;
    OscillatorParams params;
};



float osciGet(Oscillator* osci, OscillatorState* state, float sr);
void osciAdvance(Oscillator* osci, OscillatorState* state, float sr);
float osciNext(Oscillator* osci, OscillatorState* state, float sr);

float squareWave(OscillatorState* state, float sr);
float pulseWave(OscillatorState* state, float sr);
float triangleWave(OscillatorState* state, float sr);
float sineWave(OscillatorState* state, float sr);
float sawWave(OscillatorState* state, float sr);

float polyblepSaw(OscillatorState* state, float sr);

float polyblepSquare(OscillatorState* state, float sampleRate);
float polyblepPulse(OscillatorState* state, float sampleRate);
float polyblampTriangle(OscillatorState* state, float sampleRate);
float polyblepTriangle(OscillatorState* state, float sampleRate);