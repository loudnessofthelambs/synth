#pragma once

#include "voice.h"

void presetInit(Instrument* instrument, float gain);
int presetAddOscillator(Instrument* instrument, float (*wave)(OscillatorState*, OscillatorParams*, float), float detune, float phaseOffset, float gain);
int presetAddEnvelope(Instrument* instrument, float attack, float decay, float sustain, float release, int active);
int presetAddFilter(Instrument* instrument, FilterMode mode, int poles, float cutoff, float resonance, float gainDB);
int presetAddPanner(Instrument* instrument, float pan);
int presetAddDistortion(Instrument* instrument, float drive, float mix, float outputGain);
int presetAddDelay(Instrument* instrument, float time, float feedback, float mix, float tone);
int presetAddChorus(Instrument* instrument, float rate, float depth, float mix, float baseDelay);
void presetAddOutputChain(Instrument* instrument, int mixerIndex, int filterIndex, int distortionIndex, int chorusIndex, int delayIndex, int pannerIndex);
void presetAddUnisonStack(Instrument* instrument, float (*wave)(OscillatorState*, OscillatorParams*, float), int voices, float detuneCents, float gainPerVoice);

void initWarmBass(Instrument* instrument);
void initLead(Instrument* instrument);
void initPluck(Instrument* instrument);
void initGlassKeys(Instrument* instrument);
void initAtmosPad(Instrument* instrument);
int loadBuiltInInstrument(const char* name, Instrument* instrument);
