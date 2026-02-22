#pragma once

#include "oscillator.h"
#include "lpf.h"
#include "adsr.h"
#include "modroutes.h"
#include "signal.h"

#define MAX_VOICES 16
#define MAX_OSCI 8
#define MAX_OTHER 16
#define MAX_STATE 16

typedef struct VoiceState VoiceState;
typedef struct Synth Synth;
typedef struct Instrument Instrument;
typedef struct VoiceParams VoiceParams;
typedef struct Voice Voice;



struct VoiceState{
    OscillatorState oscis[MAX_STATE];
    ADSREnvState envStates[MAX_STATE];
    LPFState filterStates[MAX_STATE];
    OscillatorState lfoState[MAX_STATE];
};

struct VoiceParams {
    float volume;
    float sample;
    OscillatorParams oscis[MAX_OSCI];
    LPFParams filterParams[MAX_OTHER];
    OscillatorParams lfoParams[MAX_OTHER];
};

struct Instrument{
    Oscillator oscs[MAX_OSCI];
    ADSREnv envs[MAX_OTHER]; 
    LPF filters[MAX_OTHER];
    Oscillator lfos[MAX_OTHER];
    int8_t numEnvs;
    int8_t numOscs;
    int8_t numFilters;
    int8_t numLFOS;
    ModMatrix matrix;
    int routeNum;
    NodePreset signalChain[MAX_NODES];
    int nodeNum;
    float gain;
};
struct Voice{
    Instrument* instr;
    VoiceParams params;
    VoiceState state;
    ModMatrix matrix;
    Node signalChain[MAX_NODES];
    float sampleRate;
    int note;
    float velocity;
    int active;
};

struct Synth{
    Voice voices[MAX_VOICES];
    float sampleRate;
};

Signal voiceNext(Voice* voice);

void voiceOn(Voice* voice, Instrument* instr, int midiNote, float velocity, float volume, float sampleRate);

void voiceOff(Voice* voice);

void addModRoute(Instrument* instrum, MODROUTE_MODE mode, Modifier modifier, int index, float amount, ModDest destination);

void setDefaultModRoutes(Instrument* instrum);

void addSignalNode(Instrument* instrum, NodeType type, int index);

void addSignalNodeInput(Instrument* instrum, int index, int inputIndex);

float constOne(void*, void*, void*, float, float);