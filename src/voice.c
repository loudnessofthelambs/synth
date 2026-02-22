#include "../headers/voice.h"
#include "../headers/utils.h"
#include <stdio.h>
#include <math.h>

float constOne(void*, void*, void*, float, float) {
    return 1.0;
}

Signal voiceNext(Voice* voice) {

    //void* x = &voice;
    //printf("vcreash\n");
    applyModMatrix(voice->matrix, voice->instr->routeNum+voice->instr->numOscs+1, voice->sampleRate);
    for (int i = 0; i < voice->instr->nodeNum; i++) {
        processNode(&voice->signalChain[i],voice->sampleRate);
        
        
    }
    Signal ret = voice->signalChain[voice->instr->nodeNum-1].output;
    voice->params.sample = voice->signalChain[voice->instr->nodeNum-1].output.l;
    //printf("%f\n", voice->params.volume);
    //printf("Here%f\n", voice->state.filterStates[0].params.cutoff);
    ret.l *= voice->instr->gain;
    ret.l *= voice->params.volume;
    ret.r *= voice->instr->gain;
    ret.r *= voice->params.volume;
    //printf("%f\n", voice->params.volume);
    //printf("%f\n", voice->params.sample);
    voice->params.sample *= voice->instr->gain;
    voice->params.sample *= voice->params.volume;
    //printf("%f\n", sample);
    return ret;
}
static OscillatorState* getFreeOscillatorState(Voice* voice) {
    for (int i = 0; i < MAX_OTHER; i++) {
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
    voice->matrix[0].next = constOne;
    voice->matrix[0].next = constOne;
    voice->matrix[0].amount = volume;
    voice->matrix[0].mode = ASSIGN;
    voice->matrix[0].dest = &voice->params.volume;
    

    //setting up oscillators
    for (int i = 0; i < instr->numOscs;i++) {
        voice->params.oscis[i] = voice->instr->oscs[i].baseParams;
        voice->params.oscis[i].freqBase = midiToFreq(midiNote);
        voice->state.oscis[i].last = 0.0;
        voice->matrix[i+1].next = constOne;
        voice->matrix[i+1].mode = ASSIGN;
        voice->matrix[i+1].amount = midiToFreq(midiNote);
        voice->matrix[i+1].dest = &voice->params.oscis[i].freq;
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
        voice->matrix[i+instr->numOscs+1].amount = voice->instr->matrix[i].amount;
        voice->matrix[i+instr->numOscs+1].mode = voice->instr->matrix[i].mode;
        switch (voice->instr->matrix[i].modifier) {
            case MENV:
                voice->matrix[i+instr->numOscs+1].next = (float (*)(void*, void*, void* ,float, float))adsrNext;
                voice->matrix[i+instr->numOscs+1].preset = &voice->instr->envs[voice->instr->matrix[i].index];
                voice->matrix[i+instr->numOscs+1].params = NULL;
                voice->matrix[i+instr->numOscs+1].state = getFreeEnvState(voice);

                break;
            case MFILTER:
                voice->matrix[i+instr->numOscs+1].next = (float (*)(void*, void*, void* ,float, float))LPFNext;
                voice->matrix[i+instr->numOscs+1].preset = &voice->instr->filters[voice->instr->matrix[i].index];
                voice->matrix[i+instr->numOscs+1].params = &voice->params.filterParams[voice->instr->matrix[i].index];
                voice->matrix[i+instr->numOscs+1].state = getFreeFilterState(voice);
                break;
            case MCONSTANT:
                voice->matrix[i+instr->numOscs+1].next = constOne;
                break;
            case MLFO:
                voice->matrix[i+instr->numOscs+1].next = (float (*)(void*, void*, void* ,float, float))osciNext;
                voice->matrix[i+instr->numOscs+1].preset = &voice->instr->lfos[voice->instr->matrix[i].index];
                voice->matrix[i+instr->numOscs+1].params = &voice->params.lfoParams[voice->instr->matrix[i].index];
                voice->matrix[i+instr->numOscs+1].state = getFreeLFOState(voice);
                break;

        }
        switch (voice->instr->matrix[i].destination.modifier) {
            case DENV:
                break;
            case DFILTER:
                voice->matrix[i+instr->numOscs+1].dest = &voice->params.filterParams[voice->instr->matrix[i].destination.index].
                                                    paramsArray[voice->instr->matrix[i].destination.field];
                break;
            case DCONSTANT:
                voice->matrix[i+instr->numOscs+1].dest = &voice->params.sample;
                break;
            case DVOLUME:
                voice->matrix[i+instr->numOscs+1].dest = &voice->params.volume;
                break;
            case DOSCILLATOR:
                voice->matrix[i+instr->numOscs+1].dest = &voice->params.oscis[voice->instr->matrix[i].destination.index].
                                                    paramsArray[voice->instr->matrix[i].destination.field];
                break;
            case DROUTE:
                voice->matrix[i+instr->numOscs+1].dest = &voice->matrix[voice->instr->matrix[i].destination.index].amount;
                break;
            case DLFO:
                voice->matrix[i+instr->numOscs+1].dest = &voice->params.lfoParams[voice->instr->matrix[i].destination.index].
                                                    paramsArray[voice->instr->matrix[i].destination.field];
                break;

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
    instrum->routeNum += 1;
}

void setDefaultModRoutes(Instrument* instrum) {
    for (int i = 0; i < instrum->numOscs; i++) {
        addModRoute(instrum, ASSIGN, MCONSTANT, 0, instrum->oscs[i].baseParams.paramsArray[2], (ModDest){DOSCILLATOR, i, 2});
    }
    for (int i = 0; i < instrum->numLFOS; i++) {
        addModRoute(instrum, ASSIGN, MCONSTANT, 0, instrum->lfos[i].baseParams.paramsArray[2], (ModDest){DLFO, i, 2});
    }
    for (int i = 0; i < instrum->numFilters; i++) {
        addModRoute(instrum, ASSIGN, MCONSTANT, 0, instrum->filters[i].baseParams.paramsArray[0], (ModDest){DFILTER, i, 0});
        addModRoute(instrum, ASSIGN, MCONSTANT, 0, instrum->filters[i].baseParams.paramsArray[1], (ModDest){DFILTER, i, 1});
        printf("%f\n", instrum->filters[i].baseParams.paramsArray[0]);
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