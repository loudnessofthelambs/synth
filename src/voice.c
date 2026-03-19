#include "../headers/voice.h"
#include "../headers/utils.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>

static int voiceFinished(const Voice* voice) {
    if (voice->instr == NULL) {
        return 1;
    }
    if (voice->instr->numEnvs == 0) {
        return 0;
    }
    for (int i = 0; i < voice->instr->numEnvs; i++) {
        if (voice->state.envStates[i].stage != ADSR_OFF) {
            return 0;
        }
    }
    return 1;
}

static void clearVoiceState(Voice* voice) {
    for (int i = 0; i < MAX_STATE; i++) {
        voice->state.oscis[i] = (OscillatorState){0};
        voice->state.envStates[i] = (ADSREnvState){
            .stage = ADSR_ATTACK,
            .value = 0.0f,
            .used = 0,
            .releaseValue = 0.0f,
        };
        voice->state.filterStates[i] = (LPFState){0};
        voice->state.lfoState[i] = (OscillatorState){0};
    }
    for (int i = 0; i < MAX_OTHER; i++) {
        voice->state.delayStates[i].writeIndex = 0;
        voice->state.delayStates[i].used = 0;
        voice->state.chorusStates[i].writeIndex = 0;
        voice->state.chorusStates[i].phase = 0.0f;
        voice->state.chorusStates[i].used = 0;
    }
}

static void ensureDelayState(DelayState* state) {
    if (state->left == NULL) {
        state->left = calloc(MAX_DELAY_SAMPLES, sizeof(*state->left));
    }
    if (state->right == NULL) {
        state->right = calloc(MAX_DELAY_SAMPLES, sizeof(*state->right));
    }
    state->capacity = MAX_DELAY_SAMPLES;
}

static void ensureChorusState(ChorusState* state) {
    if (state->left == NULL) {
        state->left = calloc(MAX_CHORUS_SAMPLES, sizeof(*state->left));
    }
    if (state->right == NULL) {
        state->right = calloc(MAX_CHORUS_SAMPLES, sizeof(*state->right));
    }
    state->capacity = MAX_CHORUS_SAMPLES;
}

static OscillatorState* getFreeOscillatorState(Voice* voice) {
    for (int i = 0; i < MAX_STATE; i++) {
        if (!voice->state.oscis[i].used) {
            voice->state.oscis[i].used = 1;
            return &voice->state.oscis[i];
        }
    }
    return nullptr;
}

static ADSREnvState* getFreeEnvState(Voice* voice) {
    for (int i = 0; i < MAX_STATE; i++) {
        if (!voice->state.envStates[i].used) {
            voice->state.envStates[i].used = 1;
            return &voice->state.envStates[i];
        }
    }
    return nullptr;
}

static LPFState* getFreeFilterState(Voice* voice) {
    for (int i = 0; i < MAX_STATE; i++) {
        if (!voice->state.filterStates[i].used) {
            voice->state.filterStates[i].used = 1;
            return &voice->state.filterStates[i];
        }
    }
    return nullptr;
}

static OscillatorState* getFreeLFOState(Voice* voice) {
    for (int i = 0; i < MAX_STATE; i++) {
        if (!voice->state.lfoState[i].used) {
            voice->state.lfoState[i].used = 1;
            return &voice->state.lfoState[i];
        }
    }
    return nullptr;
}

static void copyBaseParams(Voice* voice) {
    for (int i = 0; i < voice->instr->numOscs; i++) {
        voice->params.oscis[i] = voice->instr->oscs[i].baseParams;
    }
    for (int i = 0; i < voice->instr->numFilters; i++) {
        voice->params.filterParams[i] = voice->instr->filters[i].baseParams;
    }
    for (int i = 0; i < voice->instr->numLFOS; i++) {
        voice->params.lfoParams[i] = voice->instr->lfos[i].baseParams;
    }
    for (int i = 0; i < voice->instr->numPanners; i++) {
        voice->params.pannerParams[i] = voice->instr->panners[i].baseParams;
    }
    for (int i = 0; i < voice->instr->numDistortions; i++) {
        voice->params.distortionParams[i] = voice->instr->distortions[i].baseParams;
    }
    for (int i = 0; i < voice->instr->numDelays; i++) {
        voice->params.delayParams[i] = voice->instr->delays[i].baseParams;
    }
    for (int i = 0; i < voice->instr->numChoruses; i++) {
        voice->params.chorusParams[i] = voice->instr->choruses[i].baseParams;
    }
}

static void configureModSource(Voice* voice, int routeIndex) {
    ModRoute* route = &voice->matrix[routeIndex];
    const ModRoute* source = &voice->instr->matrix[routeIndex];

    route->amount = source->amount;
    route->mode = source->mode;
    route->modifier = source->modifier;
    route->index = source->index;
    route->destination = source->destination;
    route->priority = source->priority;
    route->preset = NULL;
    route->params = NULL;
    route->state = NULL;
    route->dest = NULL;

    switch (source->modifier) {
        case MENV:
            route->next = (float (*)(void*, void*, void*, float, float))adsrNext;
            route->preset = &voice->instr->envs[source->index];
            route->state = getFreeEnvState(voice);
            break;
        case MFILTER:
            route->next = (float (*)(void*, void*, void*, float, float))LPFNext;
            route->preset = &voice->instr->filters[source->index];
            route->params = &voice->params.filterParams[source->index];
            route->state = getFreeFilterState(voice);
            break;
        case MCONSTANT:
            route->next = constOne;
            break;
        case MLFO:
            route->next = (float (*)(void*, void*, void*, float, float))osciNext;
            route->preset = &voice->instr->lfos[source->index];
            route->params = &voice->params.lfoParams[source->index];
            route->state = getFreeLFOState(voice);
            break;
    }
}

static void configureModDestination(Voice* voice, int routeIndex) {
    ModRoute* route = &voice->matrix[routeIndex];
    const ModDest destination = voice->instr->matrix[routeIndex].destination;

    switch (destination.modifier) {
        case DENV:
            route->dest = NULL;
            break;
        case DFILTER:
            route->dest = &voice->params.filterParams[destination.index].paramsArray[destination.field];
            break;
        case DPANNER:
            route->dest = &voice->params.pannerParams[destination.index].paramsArray[destination.field];
            break;
        case DVOLUME:
            route->dest = &voice->params.volume;
            break;
        case DOSCILLATOR:
            route->dest = &voice->params.oscis[destination.index].paramsArray[destination.field];
            break;
        case DROUTE:
            route->dest = &voice->matrix[destination.index].amount;
            break;
        case DLFO:
            route->dest = &voice->params.lfoParams[destination.index].paramsArray[destination.field];
            break;
        case DPITCH:
            route->dest = &voice->params.freq;
            break;
    }
}

static void configureSignalNode(Voice* voice, int nodeIndex) {
    const NodePreset preset = voice->instr->signalChain[nodeIndex];
    Node* node = &voice->signalChain[nodeIndex];

    *node = (Node){0};
    node->numInputs = preset.numInputs;
    node->type = preset.type;

    switch (preset.type) {
        case SOSCILLATOR: {
            OscillatorState* state = getFreeOscillatorState(voice);
            if (state != NULL) {
                state->phase = fmodf(voice->instr->oscs[preset.index].phaseOffset, 1.0f);
            }
            node->preset = &voice->instr->oscs[preset.index];
            node->state[0] = state;
            node->params = &voice->params.oscis[preset.index];
            break;
        }
        case SMIXER:
            break;
        case SFILTER:
            node->preset = &voice->instr->filters[preset.index];
            node->state[0] = getFreeFilterState(voice);
            node->state[1] = getFreeFilterState(voice);
            node->params = &voice->params.filterParams[preset.index];
            break;
        case SPANNER:
            node->preset = &voice->instr->panners[preset.index];
            node->params = &voice->params.pannerParams[preset.index];
            break;
        case SDISTORTION:
            node->preset = &voice->instr->distortions[preset.index];
            node->params = &voice->params.distortionParams[preset.index];
            break;
        case SDELAY:
            node->preset = &voice->instr->delays[preset.index];
            node->params = &voice->params.delayParams[preset.index];
            node->state[0] = &voice->state.delayStates[preset.index];
            ensureDelayState((DelayState*)node->state[0]);
            break;
        case SCHORUS:
            node->preset = &voice->instr->choruses[preset.index];
            node->params = &voice->params.chorusParams[preset.index];
            node->state[0] = &voice->state.chorusStates[preset.index];
            ensureChorusState((ChorusState*)node->state[0]);
            break;
    }

    for (int inputIndex = 0; inputIndex < preset.numInputs; inputIndex++) {
        node->inputs[inputIndex] = &voice->signalChain[preset.inputIndexes[inputIndex]].output;
    }
}

static void configureVoice(Voice* voice) {
    clearVoiceState(voice);
    copyBaseParams(voice);

    for (int i = 0; i < voice->instr->routeNum; i++) {
        configureModSource(voice, i);
        configureModDestination(voice, i);
    }

    for (int i = 0; i < voice->instr->nodeNum; i++) {
        configureSignalNode(voice, i);
    }
}

static float voiceStealScore(const Voice* voice, uint64_t now) {
    float score = voice->lastLevel;
    if (voice->released) {
        score -= 10.0f;
    }
    if (now > voice->startedAtFrame) {
        score -= (float)(now - voice->startedAtFrame) * 1e-6f;
    }
    return score;
}

static void deactivateVoice(Voice* voice) {
    voice->active = 0;
    voice->released = 0;
    voice->lastLevel = 0.0f;
    voice->instr = NULL;
}

void synthInit(Synth* synth, float sampleRate) {
    synth->sampleRate = sampleRate;
    synth->frameIndex = 0;
    for (int i = 0; i < MAX_VOICES; i++) {
        synth->voices[i] = (Voice){0};
    }
}

int synthHasActiveVoices(const Synth* synth) {
    for (int i = 0; i < MAX_VOICES; i++) {
        if (synth->voices[i].active) {
            return 1;
        }
    }
    return 0;
}

Voice* synthAcquireVoice(Synth* synth) {
    Voice* best = &synth->voices[0];
    float bestScore = INFINITY;

    for (int i = 0; i < MAX_VOICES; i++) {
        Voice* voice = &synth->voices[i];
        if (!voice->active) {
            voice->startedAtFrame = synth->frameIndex;
            voice->releasedAtFrame = 0;
            voice->released = 0;
            return voice;
        }

        float score = voiceStealScore(voice, synth->frameIndex);
        if (score < bestScore) {
            bestScore = score;
            best = voice;
        }
    }

    best->startedAtFrame = synth->frameIndex;
    best->releasedAtFrame = 0;
    best->released = 0;
    return best;
}

Signal synthNextFrame(Synth* synth) {
    Signal mixed = {0.0f, 0.0f};

    for (int i = 0; i < MAX_VOICES; i++) {
        if (!synth->voices[i].active) {
            continue;
        }
        mixed = addSignals(mixed, voiceNext(&synth->voices[i]));
    }

    synth->frameIndex += 1;
    return mixed;
}

float constOne(void*, void*, void*, float, float) {
    return 1.0f;
}

Signal voiceNext(Voice* voice) {
    if (!voice->active || voice->instr == NULL) {
        return (Signal){0.0f, 0.0f};
    }

    initializeParams(voice);
    applyModMatrixVoice(voice);

    for (int i = 0; i < voice->instr->nodeNum; i++) {
        processNode(&voice->signalChain[i], voice->sampleRate);
    }

    Signal output = voice->instr->nodeNum > 0
        ? voice->signalChain[voice->instr->nodeNum - 1].output
        : (Signal){0.0f, 0.0f};

    output.l *= voice->instr->gain * voice->params.volume;
    output.r *= voice->instr->gain * voice->params.volume;
    voice->lastLevel = fmaxf(fabsf(output.l), fabsf(output.r));

    if (voiceFinished(voice)) {
        deactivateVoice(voice);
    }

    return output;
}

void voiceOn(Voice* voice, Instrument* instr, int midiNote, float velocity, float volume, float sampleRate) {
    voice->generation += 1;
    voice->instr = instr;
    voice->sampleRate = sampleRate;
    voice->active = 1;
    voice->released = 0;
    voice->releasedAtFrame = 0;
    voice->params.volume = volume;
    voice->note = midiNote;
    voice->velocity = velocity;
    voice->baseVolume = volume;
    voice->baseFreq = midiToFreq((float)midiNote);
    voice->lastLevel = volume;
    configureVoice(voice);
}

void voiceOff(Voice* voice) {
    if (!voice->active || voice->instr == NULL) {
        return;
    }

    voice->released = 1;
    if (voice->releasedAtFrame == 0) {
        voice->releasedAtFrame = voice->startedAtFrame;
    }

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
        if (voice->matrix[i].priority == VOICE && voice->matrix[i].dest != NULL) {
            applyModRoute(&voice->matrix[i], voice->sampleRate);
        }
    }

    for (int i = 0; i < voice->instr->numOscs; i++) {
        voice->params.oscis[i].freq = voice->params.freq;
    }

    for (int i = 0; i < voice->instr->routeNum; i++) {
        if (voice->matrix[i].priority == OTHER && voice->matrix[i].dest != NULL) {
            applyModRoute(&voice->matrix[i], voice->sampleRate);
        }
    }
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
        voice->params.filterParams[i].gainDB = voice->instr->filters[i].baseParams.gainDB;
    }
    for (int i = 0; i < voice->instr->numPanners; i++) {
        voice->params.pannerParams[i].pan = voice->instr->panners[i].baseParams.pan;
    }
    for (int i = 0; i < voice->instr->numDistortions; i++) {
        voice->params.distortionParams[i] = voice->instr->distortions[i].baseParams;
    }
    for (int i = 0; i < voice->instr->numDelays; i++) {
        voice->params.delayParams[i] = voice->instr->delays[i].baseParams;
    }
    for (int i = 0; i < voice->instr->numChoruses; i++) {
        voice->params.chorusParams[i] = voice->instr->choruses[i].baseParams;
    }
}

void addSignalNode(Instrument* instrum, NodeType type, int index) {
    instrum->signalChain[instrum->nodeNum] = (NodePreset){type, index, 0, {0}};
    instrum->nodeNum += 1;
}

void addSignalNodeInput(Instrument* instrum, int index, int inputIndex) {
    instrum->signalChain[index].inputIndexes[instrum->signalChain[index].numInputs] = inputIndex;
    instrum->signalChain[index].numInputs += 1;
}
