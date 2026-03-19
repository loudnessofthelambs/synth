#include "../headers/presets.h"

#include "../headers/utils.h"

#include <string.h>

void presetInit(Instrument* instrument, float gain) {
    *instrument = (Instrument){0};
    instrument->gain = gain;
}

int presetAddOscillator(Instrument* instrument, float (*wave)(OscillatorState*, OscillatorParams*, float), float detune, float phaseOffset, float gain) {
    const int index = instrument->numOscs;
    if (index >= MAX_OSCI) {
        return -1;
    }
    instrument->oscs[index] = (Oscillator){wave, detune, phaseOffset, {{0.0f, gain}}};
    instrument->numOscs += 1;
    return index;
}

int presetAddEnvelope(Instrument* instrument, float attack, float decay, float sustain, float release, int active) {
    const int index = instrument->numEnvs;
    if (index >= MAX_OTHER) {
        return -1;
    }
    instrument->envs[index] = (ADSREnv){attack, decay, sustain, release, (int8_t)active};
    instrument->numEnvs += 1;
    return index;
}

int presetAddFilter(Instrument* instrument, FilterMode mode, int poles, float cutoff, float resonance, float gainDB) {
    const int index = instrument->numFilters;
    if (index >= MAX_OTHER) {
        return -1;
    }
    instrument->filters[index] = (LPF){mode, (int8_t)poles, {{cutoff, resonance, gainDB}}};
    instrument->numFilters += 1;
    return index;
}

int presetAddPanner(Instrument* instrument, float pan) {
    const int index = instrument->numPanners;
    if (index >= MAX_OTHER) {
        return -1;
    }
    instrument->panners[index] = (Panner){{{pan}}};
    instrument->numPanners += 1;
    return index;
}

int presetAddDistortion(Instrument* instrument, float drive, float mix, float outputGain) {
    const int index = instrument->numDistortions;
    if (index >= MAX_OTHER) {
        return -1;
    }
    instrument->distortions[index] = (Distortion){{{drive, mix, outputGain}}};
    instrument->numDistortions += 1;
    return index;
}

int presetAddDelay(Instrument* instrument, float time, float feedback, float mix, float tone) {
    const int index = instrument->numDelays;
    if (index >= MAX_OTHER) {
        return -1;
    }
    instrument->delays[index] = (Delay){{{time, feedback, mix, tone}}};
    instrument->numDelays += 1;
    return index;
}

int presetAddChorus(Instrument* instrument, float rate, float depth, float mix, float baseDelay) {
    const int index = instrument->numChoruses;
    if (index >= MAX_OTHER) {
        return -1;
    }
    instrument->choruses[index] = (Chorus){{{rate, depth, mix, baseDelay}}};
    instrument->numChoruses += 1;
    return index;
}

void presetAddOutputChain(Instrument* instrument, int mixerIndex, int filterIndex, int distortionIndex, int chorusIndex, int delayIndex, int pannerIndex) {
    const int mixerNode = instrument->nodeNum;
    addSignalNode(instrument, SMIXER, mixerIndex);
    for (int i = 0; i < instrument->numOscs; i++) {
        addSignalNodeInput(instrument, mixerNode, i);
    }

    int current = mixerNode;
    if (filterIndex >= 0) {
        addSignalNode(instrument, SFILTER, filterIndex);
        addSignalNodeInput(instrument, instrument->nodeNum - 1, current);
        current = instrument->nodeNum - 1;
    }
    if (distortionIndex >= 0) {
        addSignalNode(instrument, SDISTORTION, distortionIndex);
        addSignalNodeInput(instrument, instrument->nodeNum - 1, current);
        current = instrument->nodeNum - 1;
    }
    if (chorusIndex >= 0) {
        addSignalNode(instrument, SCHORUS, chorusIndex);
        addSignalNodeInput(instrument, instrument->nodeNum - 1, current);
        current = instrument->nodeNum - 1;
    }
    if (delayIndex >= 0) {
        addSignalNode(instrument, SDELAY, delayIndex);
        addSignalNodeInput(instrument, instrument->nodeNum - 1, current);
        current = instrument->nodeNum - 1;
    }
    if (pannerIndex >= 0) {
        addSignalNode(instrument, SPANNER, pannerIndex);
        addSignalNodeInput(instrument, instrument->nodeNum - 1, current);
    }
}

void presetAddUnisonStack(Instrument* instrument, float (*wave)(OscillatorState*, OscillatorParams*, float), int voices, float detuneCents, float gainPerVoice) {
    if (voices <= 1) {
        presetAddOscillator(instrument, wave, 0.0f, 0.0f, gainPerVoice);
        return;
    }

    for (int i = 0; i < voices; i++) {
        const float t = voices == 1 ? 0.0f : ((float)i / (float)(voices - 1)) * 2.0f - 1.0f;
        presetAddOscillator(instrument, wave, t * detuneCents, (float)i * 0.13f, gainPerVoice);
    }
}

void initWarmBass(Instrument* instrument) {
    presetInit(instrument, 0.55f);
    presetAddOscillator(instrument, sawWave, 0.0f, 0.0f, 0.50f);
    presetAddOscillator(instrument, sawWave, -5.0f, 0.0f, 0.50f);
    presetAddEnvelope(instrument, 0.005f, 0.15f, 0.10f, 0.20f, 1);
    presetAddEnvelope(instrument, 0.002f, 0.20f, 0.00f, 0.18f, 1);
    presetAddFilter(instrument, FILTER_LADDER, 4, 800.0f, 0.20f, 0.0f);
    presetAddPanner(instrument, -0.25f);

    addModRoute(instrument, ADD, MENV, 1, 2000.0f, (ModDest){DFILTER, 0, 0});
    addModRoute(instrument, MULT, MENV, 0, 1.0f, (ModDest){DVOLUME, 0, 0});

    addSignalNode(instrument, SOSCILLATOR, 0);
    addSignalNode(instrument, SOSCILLATOR, 1);
    presetAddOutputChain(instrument, 0, 0, -1, -1, -1, 0);
}

void initLead(Instrument* instrument) {
    presetInit(instrument, 0.35f);
    presetAddUnisonStack(instrument, polyblepSaw, 5, 5.0f, 0.18f);
    presetAddOscillator(instrument, noise, 12.0f, 0.62f, 0.01f);
    presetAddEnvelope(instrument, 0.01f, 0.05f, 0.70f, 0.20f, 1);
    presetAddEnvelope(instrument, 0.005f, 0.20f, 0.20f, 0.18f, 1);
    instrument->lfos[0] = (Oscillator){sineWave, 0.0f, 0.0f, {{5.0f, 0.10f}}};
    instrument->lfos[1] = (Oscillator){sineWave, 0.0f, 0.0f, {{0.18f, 1.0f}}};
    instrument->numLFOS = 2;
    presetAddFilter(instrument, FILTER_LADDER, 4, 2500.0f, 0.20f, 0.0f);
    presetAddPanner(instrument, 0.15f);
    presetAddChorus(instrument, 0.35f, 0.008f, 0.18f, 0.014f);

    addModRoute(instrument, ADD, MENV, 1, 3500.0f, (ModDest){DFILTER, 0, 0});
    addModRoute(instrument, MULT, MENV, 0, 1.0f, (ModDest){DVOLUME, 0, 0});
    addModRoutePrio(instrument, MULTADD, MLFO, 0, SEMITONE - 1.0f, (ModDest){DPITCH, 0, 0});
    addModRoute(instrument, ADD, MLFO, 1, 0.35f, (ModDest){DPANNER, 0, 0});

    for (int i = 0; i < instrument->numOscs; i++) {
        addSignalNode(instrument, SOSCILLATOR, i);
    }
    presetAddOutputChain(instrument, 0, 0, -1, 0, -1, 0);
}

void initPluck(Instrument* instrument) {
    presetInit(instrument, 0.42f);
    presetAddUnisonStack(instrument, polyblepSquare, 3, 7.0f, 0.22f);
    presetAddEnvelope(instrument, 0.002f, 0.12f, 0.0f, 0.08f, 1);
    presetAddEnvelope(instrument, 0.001f, 0.08f, 0.0f, 0.06f, 1);
    presetAddFilter(instrument, FILTER_BIQUAD_LP, 2, 2200.0f, 0.9f, 0.0f);
    presetAddDistortion(instrument, 1.8f, 0.18f, 0.9f);
    presetAddDelay(instrument, 0.18f, 0.32f, 0.16f, 0.65f);
    presetAddPanner(instrument, -0.05f);

    addModRoute(instrument, ADD, MENV, 1, 2600.0f, (ModDest){DFILTER, 0, 0});
    addModRoute(instrument, MULT, MENV, 0, 1.0f, (ModDest){DVOLUME, 0, 0});

    for (int i = 0; i < instrument->numOscs; i++) {
        addSignalNode(instrument, SOSCILLATOR, i);
    }
    presetAddOutputChain(instrument, 0, 0, 0, -1, 0, 0);
}

void initGlassKeys(Instrument* instrument) {
    presetInit(instrument, 0.30f);
    presetAddOscillator(instrument, sineWave, 0.0f, 0.0f, 0.65f);
    presetAddOscillator(instrument, triangleWave, 12.0f, 0.25f, 0.20f);
    presetAddEnvelope(instrument, 0.01f, 0.25f, 0.35f, 0.35f, 1);
    presetAddEnvelope(instrument, 0.002f, 0.12f, 0.0f, 0.25f, 1);
    presetAddFilter(instrument, FILTER_BIQUAD_PEAK, 2, 2300.0f, 0.8f, 4.5f);
    presetAddChorus(instrument, 0.22f, 0.007f, 0.22f, 0.012f);
    presetAddDelay(instrument, 0.26f, 0.38f, 0.14f, 0.75f);
    presetAddPanner(instrument, 0.10f);

    addModRoute(instrument, MULT, MENV, 0, 1.0f, (ModDest){DVOLUME, 0, 0});

    addSignalNode(instrument, SOSCILLATOR, 0);
    addSignalNode(instrument, SOSCILLATOR, 1);
    presetAddOutputChain(instrument, 0, 0, -1, 0, 0, 0);
}

void initAtmosPad(Instrument* instrument) {
    presetInit(instrument, 0.24f);
    presetAddUnisonStack(instrument, polyblepSaw, 4, 10.0f, 0.15f);
    presetAddOscillator(instrument, triangleWave, -12.0f, 0.33f, 0.18f);
    presetAddEnvelope(instrument, 0.45f, 0.80f, 0.70f, 1.20f, 1);
    presetAddEnvelope(instrument, 0.15f, 0.45f, 0.0f, 0.90f, 1);
    instrument->lfos[0] = (Oscillator){sineWave, 0.0f, 0.0f, {{0.18f, 1.0f}}};
    instrument->numLFOS = 1;
    presetAddFilter(instrument, FILTER_BIQUAD_LP, 2, 1800.0f, 0.7f, 0.0f);
    presetAddFilter(instrument, FILTER_BIQUAD_PEAK, 2, 900.0f, 0.9f, 3.0f);
    presetAddChorus(instrument, 0.14f, 0.012f, 0.35f, 0.018f);
    presetAddDelay(instrument, 0.38f, 0.42f, 0.20f, 0.82f);
    presetAddPanner(instrument, 0.0f);

    addModRoute(instrument, MULT, MENV, 0, 1.0f, (ModDest){DVOLUME, 0, 0});
    addModRoute(instrument, ADD, MENV, 1, 1800.0f, (ModDest){DFILTER, 0, 0});
    addModRoute(instrument, ADD, MLFO, 0, 0.2f, (ModDest){DPANNER, 0, 0});

    for (int i = 0; i < instrument->numOscs; i++) {
        addSignalNode(instrument, SOSCILLATOR, i);
    }
    presetAddOutputChain(instrument, 0, 0, -1, 0, 0, 0);
}

int loadBuiltInInstrument(const char* name, Instrument* instrument) {
    if (strcmp(name, "lead") == 0) {
        initLead(instrument);
        return 1;
    }
    if (strcmp(name, "warmbass") == 0 || strcmp(name, "bass") == 0) {
        initWarmBass(instrument);
        return 1;
    }
    if (strcmp(name, "pluck") == 0) {
        initPluck(instrument);
        return 1;
    }
    if (strcmp(name, "glasskeys") == 0 || strcmp(name, "keys") == 0) {
        initGlassKeys(instrument);
        return 1;
    }
    if (strcmp(name, "atmospad") == 0 || strcmp(name, "pad") == 0) {
        initAtmosPad(instrument);
        return 1;
    }
    return 0;
}
