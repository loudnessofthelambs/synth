#pragma once

typedef struct Oscillator Oscillator;
typedef struct OscillatorState OscillatorState;


/*little bit of explanation: 
Every oscillator has some data that every oscillator has, like the current frequency and phase.
However, it also has some data that the individual oscillator needs to hold. For example, a pulse wave
needs to store the current pulse width, as a state variable, so that pulse width modulation is possible.
However, there is an issue, when initializing a voice, the oscillators in each instrument only have their
parameters set. There is no way with my system to initialize the state. So, the first paramLength entries
in OscillatorState.data will be propagated with Oscillator->params automatically. This also has the advantage
of only requiring the next function to take in an oscillator state variable, as it can read parameters from there.

*/
struct Oscillator{
    float (*get)(OscillatorState*, float);
    float detune;    
    float phaseOffset; 
    float gain;
    void* params;
    int paramLength;
};
struct OscillatorState{
    float phase;
    float freq;
    float last;
    void* data;
    int dataLength;
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