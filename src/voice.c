#include "../headers/voice.h"
#include "../headers/utils.h"
#include <stdio.h>
#include <math.h>

float constOne(void*, void*, void*, float, float) {
    return 1.0;
}

Signal voiceNext(Voice* voice) {

    //void* x = &voice;
    
    initializeParams(voice);

    applyModMatrixVoice(voice);
    
    for (int i = 0; i < voice->instr->nodeNum; i++) {
        processNode(&voice->signalChain[i],voice->sampleRate);
        
        
    }
    Signal ret = voice->signalChain[voice->instr->nodeNum-1].output;
    //printf("%f\n", voice->params.volume);
    //printf("Here%f\n", voice->state.filterStates[0].params.cutoff);
    ret.l *= voice->instr->gain;
    ret.l *= voice->params.volume;
    ret.r *= voice->instr->gain;
    ret.r *= voice->params.volume;
    //printf("%f\n", voice->params.volume);
    //printf("%f\n", voice->params.sample);
    //printf("%f\n", sample);
    return ret;
}
static OscillatorState* getFreeOscillatorState(Voice* voice) {
    for (int i = 0; i < MAX_STATE; i++) {
        if (voice->state.oscis[i].used == 0) {
            voice->state.oscis[i].used = 1;
            return &voice->state.oscis[i];
        }

    }
    return nullptr;
}
static ADSREnvState* getFreeEnvState(Voice* voice) {
    for (int i = 0; i < MAX_STATE; i++) {
        if (voice->state.envStates[i].used == 0) {
            voice->state.envStates[i].used = 1;
            return &voice->state.envStates[i];
        }

    }
    return nullptr;
}
static LPFState* getFreeFilterState(Voice* voice) {
    for (int i = 0; i < MAX_STATE; i++) {
        if (voice->state.filterStates[i].used == 0) {
            voice->state.filterStates[i].used = 1;
            return &voice->state.filterStates[i];
        }

    }
    return nullptr;
}
static OscillatorState* getFreeLFOState(Voice* voice) {
    for (int i = 0; i < MAX_STATE; i++) {
        if (voice->state.lfoState[i].used == 0) {
            voice->state.lfoState[i].used = 1;
            return &voice->state.lfoState[i];
        }

    }
    return nullptr;
}

void voiceOn(Voice* voice, Instrument* instr, int midiNote, float velocity, float volume, float sampleRate) {
    voice->instr = instr;
    voice->sampleRate = sampleRate;
    voice->active = 1;
    voice->params.volume = volume;
    voice->note = midiNote;
    voice->velocity = velocity;
    voice->baseVolume = volume;
    voice->baseFreq = midiToFreq(midiNote);
    

    //setting up oscillators
    for (int i = 0; i < instr->numOscs;i++) {
        voice->params.oscis[i] = voice->instr->oscs[i].baseParams;
        voice->state.oscis[i].last = 0.0;

        //printf("%d\n", i);
    }


    //setting all state variables to default
    for (int i = 0; i < MAX_STATE; i++) {
        voice->state.oscis[i].last = 0.0;
        voice->state.envStates[i].releaseValue = 0.0;
        voice->state.envStates[i].value = 0.0;
        voice->state.envStates[i].stage = ADSR_ATTACK;
        voice->state.envStates[i].used = 0;
        
        for (int j = 0; j < MAX_POLES; j++) {
            voice->state.filterStates[i].values[j]=0.0; 
        }
        voice->state.filterStates[i].used=0;
        voice->state.lfoState[i].phase = 0.0;
        voice->state.lfoState[i].last = 0.0;
        voice->state.lfoState[i].used = 0;
        voice->state.oscis[i].phase = 0.0;
        voice->state.oscis[i].used = 0;
    }
    
    //setting baseparams
    for (int i = 0; i < voice->instr->numFilters; i++) {
        voice->params.filterParams[i] = voice->instr->filters[i].baseParams;

    }
    for (int i = 0; i < voice->instr->numLFOS; i++) {
        voice->params.lfoParams[i] = voice->instr->lfos[i].baseParams;
    }

    //setting up mod matrix
    for (int i = 0; i < voice->instr->routeNum; i++) {
        voice->matrix[i].amount = voice->instr->matrix[i].amount;
        voice->matrix[i].mode = voice->instr->matrix[i].mode;
        switch (voice->instr->matrix[i].modifier) {
            case MENV:
                voice->matrix[i].next = (float (*)(void*, void*, void* ,float, float))adsrNext;
                voice->matrix[i].preset = &voice->instr->envs[voice->instr->matrix[i].index];
                voice->matrix[i].params = NULL;
                voice->matrix[i].state = getFreeEnvState(voice);

                break;
            case MFILTER:
                voice->matrix[i].next = (float (*)(void*, void*, void* ,float, float))LPFNext;
                voice->matrix[i].preset = &voice->instr->filters[voice->instr->matrix[i].index];
                voice->matrix[i].params = &voice->params.filterParams[voice->instr->matrix[i].index];
                voice->matrix[i].state = getFreeFilterState(voice);
                break;
            case MCONSTANT:
                voice->matrix[i].next = constOne;
                break;
            case MLFO:
                voice->matrix[i].next = (float (*)(void*, void*, void* ,float, float))osciNext;
                voice->matrix[i].preset = &voice->instr->lfos[voice->instr->matrix[i].index];
                voice->matrix[i].params = &voice->params.lfoParams[voice->instr->matrix[i].index];
                voice->matrix[i].state = getFreeLFOState(voice);
                break;

        }
        switch (voice->instr->matrix[i].destination.modifier) {
            case DENV:
                break;
            case DFILTER:
                voice->matrix[i].dest = &voice->params.filterParams[voice->instr->matrix[i].destination.index].
                                                    paramsArray[voice->instr->matrix[i].destination.field];
                break;
            case DPANNER:
                //voice->matrix[i].dest = &voice->params.sample;
                break;
            case DVOLUME:
                voice->matrix[i].dest = &voice->params.volume;
                break;
            case DOSCILLATOR:
                voice->matrix[i].dest = &voice->params.oscis[voice->instr->matrix[i].destination.index].
                                                    paramsArray[voice->instr->matrix[i].destination.field];
                break;
            case DROUTE:
                voice->matrix[i].dest = &voice->matrix[voice->instr->matrix[i].destination.index].amount;
                break;
            case DLFO:
                voice->matrix[i].dest = &voice->params.lfoParams[voice->instr->matrix[i].destination.index].
                                                    paramsArray[voice->instr->matrix[i].destination.field];
                break;
            case DPITCH:
                voice->matrix[i].dest = &voice->params.freq;

        }
        //printf("%p\n", voice->matrix[i].dest);
    }
    

    //setting up signal chain
    for (int i = 0; i < voice->instr->nodeNum; i++) {
        NodePreset nodePreset = instr->signalChain[i];
        voice->signalChain[i].numInputs = nodePreset.numInputs;
        voice->signalChain[i].type = nodePreset.type;
        switch (nodePreset.type) {
            case SOSCILLATOR:
                voice->signalChain[i].preset = &voice->instr->oscs[nodePreset.index];
                OscillatorState* state = getFreeOscillatorState(voice);
                state->phase = fmod(voice->instr->oscs[nodePreset.index].phaseOffset,1);
                voice->signalChain[i].state[0] = state;
                voice->signalChain[i].params = &voice->params.oscis[nodePreset.index];
                
                break;
            case SMIXER:
                break; //mixer needs no special calibration
            case SFILTER:
                voice->signalChain[i].preset = &voice->instr->filters[nodePreset.index];
                voice->signalChain[i].state[0] = getFreeFilterState(voice);
                voice->signalChain[i].state[1] = getFreeFilterState(voice);
                voice->signalChain[i].params = &voice->params.filterParams[nodePreset.index];
                break;
        } 
        
        for (int j = 0; j < nodePreset.numInputs; j++) {

            voice->signalChain[i].inputs[j] = &voice->signalChain[nodePreset.inputIndexes[j]].output;
        }
        voice->signalChain[i].output = (Signal){0,0};
    } 
    
    

}
void voiceOff(Voice* voice) {
    if (!voice->active) return;
    for (int i = 0; i < voice->instr->numEnvs; i++) {
        if (voice->state.envStates[i].stage != ADSR_OFF &&
            voice->state.envStates[i].stage != ADSR_RELEASE) {

            voice->state.envStates[i].stage = ADSR_RELEASE;
            voice->state.envStates[i].releaseValue = voice->state.envStates[i].value;
            
        }
    }
    
}





void addModRoute(Instrument* instrum, MODROUTE_MODE mode, Modifier modifier, int index, float amount, ModDest destination) {

    instrum->matrix[instrum->routeNum].destination = destination;
    instrum->matrix[instrum->routeNum].amount = amount;
    instrum->matrix[instrum->routeNum].mode = mode;
    instrum->matrix[instrum->routeNum].index = index;
    instrum->matrix[instrum->routeNum].modifier = modifier;
    instrum->matrix[instrum->routeNum].priority = OTHER;
    instrum->routeNum += 1;
}
void addModRoutePrio(Instrument* instrum, MODROUTE_MODE mode, Modifier modifier, int index, float amount, ModDest destination) {

    instrum->matrix[instrum->routeNum].destination = destination;
    instrum->matrix[instrum->routeNum].amount = amount;
    instrum->matrix[instrum->routeNum].mode = mode;
    instrum->matrix[instrum->routeNum].index = index;
    instrum->matrix[instrum->routeNum].modifier = modifier;
    instrum->matrix[instrum->routeNum].priority = VOICE;
    instrum->routeNum += 1;
}

void applyModMatrixVoice(Voice* voice) {
    for (int i = 0; i < voice->instr->routeNum; i++) {
        if (voice->matrix[i].priority == VOICE) {
            applyModRoute(&voice->matrix[i], voice->sampleRate);
        }
    }
    for (int i = 0; i < voice->instr->numOscs; i++) {
        voice->params.oscis[i].freq = voice->params.freq;
    }
    for (int i = 0; i < voice->instr->routeNum; i++) {
        if (voice->matrix[i].priority == OTHER) {
            applyModRoute(&voice->matrix[i], voice->sampleRate);
        }
    }
    return;
}

void initializeParams(Voice* voice) {
    voice->params.freq = voice->baseFreq;
    voice->params.volume = voice->baseVolume;
    for (int i = 0; i < voice->instr->numOscs; i++) {
        voice->params.oscis[i].freq = voice->params.freq;
        voice->params.oscis[i].gain = voice->instr->oscs[i].baseParams.gain;
    }
    for (int i = 0; i < voice->instr->numLFOS; i++) {
        voice->params.lfoParams[i].freq = voice->instr->lfos[i].baseParams.freq;
        voice->params.lfoParams[i].gain = voice->instr->lfos[i].baseParams.gain;
    }
    for (int i = 0; i < voice->instr->numFilters; i++) {
        voice->params.filterParams[i].cutoff = voice->instr->filters[i].baseParams.cutoff;
        voice->params.filterParams[i].resonance = voice->instr->filters[i].baseParams.resonance;
    }
}



void addSignalNode(Instrument *instrum, NodeType type, int index) {
    instrum->signalChain[instrum->nodeNum] = (NodePreset){type, index, 0, {0}};
    instrum->nodeNum++;
}

void addSignalNodeInput(Instrument *instrum, int index, int inputIndex) {
    instrum->signalChain[index].inputIndexes[instrum->signalChain[index].numInputs] = inputIndex;
    instrum->signalChain[index].numInputs++;
}

