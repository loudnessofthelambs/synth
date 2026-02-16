#include "../headers/voice.h"
#include "../headers/all.h"


float voiceNext(Voice* voice) {

    float sample = 0;
    //void* x = &voice;
    //printf("vcreash\n");
    for (int i = 0; i < voice->instr->numOscs; i++) {
        sample += osciNext(&voice->instr->oscs[i], &voice->state.oscis[i], voice->sampleRate);
        //printf("%f\n", sample);
        
    }

    applyModMatrix(voice->instr->matrix, voice->instr->rowNum, voice->sampleRate);

    //printf("%f\n", sample);
    sample *= voice->instr->gain;
    sample *= voice->params.volume;
    //printf("%f\n", sample);
    return sample;
}



void voiceOn(Voice* voice, Instrument* instr, int midiNote, float velocity, float volume, float sampleRate) {
    voice->instr = instr;
    voice->sampleRate = sampleRate;
    voice->active = 1;
    voice->params.volume = volume;
    voice->note = midiNote;
    voice->velocity = velocity;

    for (int i = 0; i < instr->numOscs;i++) {
        voice->state.oscis[i].params = voice->instr->oscs[i].baseParams;
        voice->state.oscis[i].phase = fmod(instr->oscs[i].phaseOffset,1);
    }
    for (int i = 0; i < voice->instr->numEnvs; i++) {
        voice->state.envStates[i].releaseValue = 0.0;
        voice->state.envStates[i].value = 0.0;
        voice->state.envStates[i].stage = ADSR_ATTACK;
    }
    for (int i = 0; i < voice->instr->numFilters; i++) {
        voice->state.envStates[i].releaseValue = 0.0;
        voice->state.envStates[i].value = 0.0;
        voice->state.envStates[i].stage = ADSR_ATTACK;
    }



    for (int i = 0; i < voice->instr->rowNum; i++) {
        for (int j = 0; i < voice->instr->matrix[i].routesNum; j++) {
            switch (voice->matrix[i].routes[j].modifier) {
                case ENV:
                    voice->matrix[i].routes[j].next = (float (*)(void*, void*, float, float))adsrNext;
                    voice->matrix[i].routes[j].preset = &voice->instr->envs[voice->matrix[i].routes[j].index];
                    voice->matrix[i].routes[j].state = &voice->state.envStates[voice->matrix[i].routes[j].index];
                    break;
                case FILTER:
                    voice->matrix[i].routes[j].next = (float (*)(void*, void*, float, float))LPFNext;
                    voice->matrix[i].routes[j].preset = &voice->instr->filters[voice->matrix[i].routes[j].index];
                    voice->matrix[i].routes[j].state = &voice->state.filterStates[voice->matrix[i].routes[j].index];
                    break;
                case CONSTANT:
                    voice->matrix[i].routes[j].next = constOne;
                    break;

            }
            switch (voice->matrix[i].routes[j].destination.modifier) {
                case ENV:
                    break;
                case FILTER:
                    voice->matrix[i].routes[j].dest = &voice->state.filterStates[voice->matrix[i].routes[j].destination.index].
                                                        params.paramsArray[voice->matrix[i].routes[j].destination.field];
                    break;
                case CONSTANT:
                    voice->matrix[i].routes[j].dest = &voice->params.volume;
                    break;
                case OSCILLATOR:
                    voice->matrix[i].routes[j].dest = &voice->state.oscis[voice->matrix[i].routes[j].destination.index].
                                                        params.paramsArray[voice->matrix[i].routes[j].destination.field];
                    break;

            }
        }
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
    voice->active = 0;
    
}



void addModRouteToRow(MODROUTE_MODE mode, ModRow* row, Modifier modifier, int index, float amount) {
    row->routes[row->routesNum].amount = amount;
    row->routes[row->routesNum].mode = mode;
    row->routes[row->routesNum].index = index;
    row->routes[row->routesNum].modifier = modifier;
    row->routesNum += 1;
}

void addModRoute(Instrument* instrum, MODROUTE_MODE mode, Modifier modifier, int index, float amount, float* destination) {
    for (int i = 0; i < instrum->rowNum; i++) {
        if (instrum->matrix[i].destination == destination) {
            addModRouteToRow(mode, &instrum->matrix[i], modifier, index, amount);
            return;
        }
    }
    instrum->matrix[instrum->rowNum].destination = destination;
    instrum->matrix[instrum->rowNum].routesNum = 0;
    addModRouteToRow(mode, &instrum->matrix[instrum->rowNum], modifier, index, amount);
    instrum->rowNum += 1;
}