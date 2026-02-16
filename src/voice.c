#include "../headers/voice.h"
#include "../headers/all.h"
#include <stdio.h>

float voiceNext(Voice* voice) {

    voice->params.sample = 0;
    //void* x = &voice;
    //printf("vcreash\n");
    for (int i = 0; i < voice->instr->numOscs; i++) {
        voice->params.sample += osciNext(&voice->instr->oscs[i], &voice->state.oscis[i], voice->params.sample, voice->sampleRate);
        //printf("%f\n", sample);
        
    }
    //printf("%f\n", voice->params.volume);
    //printf("Here%f\n", voice->state.filterStates[0].params.cutoff);

    applyModMatrix(voice->matrix, voice->instr->routeNum+voice->instr->numOscs+1, voice->sampleRate);
    //printf("%f\n", voice->params.volume);
    //printf("%f\n", voice->params.sample);
    voice->params.sample *= voice->instr->gain;
    voice->params.sample *= voice->params.volume;
    //printf("%f\n", sample);
    return voice->params.sample;
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
    
    for (int i = 0; i < instr->numOscs;i++) {
        voice->state.oscis[i].params = voice->instr->oscs[i].baseParams;
        voice->state.oscis[i].params.freqBase = midiToFreq(midiNote);
        voice->state.oscis[i].last = 0.0;
        voice->matrix[i+1].next = constOne;
        voice->matrix[i+1].mode = ASSIGN;
        voice->matrix[i+1].amount = midiToFreq(midiNote);
        voice->matrix[i+1].dest = &voice->state.oscis[i].params.freq;
        //printf("%d\n", i);
        voice->state.oscis[i].phase = fmod(instr->oscs[i].phaseOffset,1);
    }
    for (int i = 0; i < voice->instr->numEnvs; i++) {
        voice->state.envStates[i].releaseValue = 0.0;
        voice->state.envStates[i].value = 0.0;
        voice->state.envStates[i].stage = ADSR_ATTACK;
    }
    for (int i = 0; i < voice->instr->numFilters; i++) {
        voice->state.filterStates[i].params = voice->instr->filters[i].baseParams;
        for (int j = 0; j < voice->instr->filters[i].poles; i++) {
            voice->state.filterStates[i].values[i]=0.0;
        }
    }
    for (int i = 0; i < voice->instr->numLFOS; i++) {
        voice->state.lfoState[i].params = voice->instr->lfos[i].baseParams;
        voice->state.lfoState[i].phase = 0.0;
        voice->state.lfoState[i].last = 0.0;
    }

    for (int i = 0; i < voice->instr->routeNum; i++) {
        voice->matrix[i+instr->numOscs+1].amount = voice->instr->matrix[i].amount;
        voice->matrix[i+instr->numOscs+1].mode = voice->instr->matrix[i].mode;
        switch (voice->instr->matrix[i].modifier) {
            case ENV:
                voice->matrix[i+instr->numOscs+1].next = (float (*)(void*, void*, float, float))adsrNext;
                voice->matrix[i+instr->numOscs+1].preset = &voice->instr->envs[voice->instr->matrix[i].index];
                voice->matrix[i+instr->numOscs+1].state = &voice->state.envStates[voice->instr->matrix[i].index];
                break;
            case FILTER:
                voice->matrix[i+instr->numOscs+1].next = (float (*)(void*, void*, float, float))LPFNext;
                voice->matrix[i+instr->numOscs+1].preset = &voice->instr->filters[voice->instr->matrix[i].index];
                voice->matrix[i+instr->numOscs+1].state = &voice->state.filterStates[voice->instr->matrix[i].index];
                break;
            case CONSTANT:
                voice->matrix[i+instr->numOscs+1].next = constOne;
                break;
            case LFO:
                voice->matrix[i+instr->numOscs+1].next = (float (*)(void*, void*, float, float))osciNext;
                voice->matrix[i+instr->numOscs+1].preset = &voice->instr->lfos[voice->instr->matrix[i].index];
                voice->matrix[i+instr->numOscs+1].state = &voice->state.lfoState[voice->instr->matrix[i].index];
                break;

        }
        switch (voice->instr->matrix[i].destination.modifier) {
            case ENV:
                break;
            case FILTER:
                voice->matrix[i+instr->numOscs+1].dest = &voice->state.filterStates[voice->instr->matrix[i].destination.index].
                                                    params.paramsArray[voice->instr->matrix[i].destination.field];
                break;
            case CONSTANT:
                voice->matrix[i+instr->numOscs+1].dest = &voice->params.sample;
                break;
            case VOLUME:
                voice->matrix[i+instr->numOscs+1].dest = &voice->params.volume;
                break;
            case OSCILLATOR:
                voice->matrix[i+instr->numOscs+1].dest = &voice->state.oscis[voice->instr->matrix[i].destination.index].
                                                    params.paramsArray[voice->instr->matrix[i].destination.field];
                break;
            case ROUTE:
                voice->matrix[i+instr->numOscs+1].dest = &voice->matrix[voice->instr->matrix[i].destination.index].amount;
                break;
            case LFO:
                voice->matrix[i+instr->numOscs+1].dest = &voice->state.lfoState[voice->instr->matrix[i].destination.index].
                                                    params.paramsArray[voice->instr->matrix[i].destination.field];
                break;

        }
        //printf("%p\n", voice->matrix[i].dest);
    }
    //applyModMatrix(voice->matrix, voice->instr->)
    

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
        addModRoute(instrum, ASSIGN, CONSTANT, 0, instrum->oscs[i].baseParams.paramsArray[2], (ModDest){OSCILLATOR, i, 2});
    }
    for (int i = 0; i < instrum->numLFOS; i++) {
        addModRoute(instrum, ASSIGN, CONSTANT, 0, instrum->lfos[i].baseParams.paramsArray[2], (ModDest){LFO, i, 2});
    }
    for (int i = 0; i < instrum->numFilters; i++) {
        addModRoute(instrum, ASSIGN, CONSTANT, 0, instrum->filters[i].baseParams.paramsArray[0], (ModDest){FILTER, i, 0});
        addModRoute(instrum, ASSIGN, CONSTANT, 0, instrum->filters[i].baseParams.paramsArray[1], (ModDest){FILTER, i, 1});
        printf("%f\n", instrum->filters[i].baseParams.paramsArray[0]);
    }
}