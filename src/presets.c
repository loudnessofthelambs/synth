#include "../headers/presets.h"

#include "../headers/utils.h"

#include <string.h>

void initWarmBass(Instrument* instrument) {
    *instrument = (Instrument){0};

    instrument->oscs[0] = (Oscillator){sawWave, 0.0f, 0.0f, {{0.0f, 0.50f}}};
    instrument->oscs[1] = (Oscillator){sawWave, -5.0f, 0.0f, {{0.0f, 0.50f}}};
    instrument->numOscs = 2;

    instrument->envs[0] = (ADSREnv){0.005f, 0.15f, 0.10f, 0.20f, 1};
    instrument->envs[1] = (ADSREnv){0.002f, 0.20f, 0.00f, 0.18f, 1};
    instrument->numEnvs = 2;

    instrument->filters[0] = (LPF){4, {{800.0f, 0.20f}}};
    instrument->numFilters = 1;

    instrument->panners[0] = (Panner){{{-0.25f}}};
    instrument->numPanners = 1;

    instrument->gain = 0.55f;

    addModRoute(instrument, ADD, MENV, 1, 2000.0f, (ModDest){DFILTER, 0, 0});
    addModRoute(instrument, MULT, MENV, 0, 1.0f, (ModDest){DVOLUME, 0, 0});

    addSignalNode(instrument, SOSCILLATOR, 0);
    addSignalNode(instrument, SOSCILLATOR, 1);
    addSignalNode(instrument, SMIXER, 0);
    addSignalNodeInput(instrument, 2, 0);
    addSignalNodeInput(instrument, 2, 1);
    addSignalNode(instrument, SFILTER, 0);
    addSignalNodeInput(instrument, 3, 2);
    addSignalNode(instrument, SPANNER, 0);
    addSignalNodeInput(instrument, 4, 3);
}

void initLead(Instrument* instrument) {
    *instrument = (Instrument){0};

    instrument->oscs[0] = (Oscillator){polyblepSaw, 0.0f, 0.10f, {{0.0f, 0.18f}}};
    instrument->oscs[1] = (Oscillator){polyblepSaw, -5.0f, 0.03f, {{0.0f, 0.18f}}};
    instrument->oscs[2] = (Oscillator){polyblepSaw, -2.0f, 0.35f, {{0.0f, 0.18f}}};
    instrument->oscs[3] = (Oscillator){polyblepSaw, 2.0f, 0.80f, {{0.0f, 0.18f}}};
    instrument->oscs[4] = (Oscillator){polyblepSaw, 5.0f, 0.62f, {{0.0f, 0.18f}}};
    instrument->oscs[5] = (Oscillator){noise, 12.0f, 0.62f, {{0.0f, 0.01f}}};
    instrument->numOscs = 6;

    instrument->envs[0] = (ADSREnv){0.01f, 0.05f, 0.70f, 0.20f, 1};
    instrument->envs[1] = (ADSREnv){0.005f, 0.20f, 0.20f, 0.18f, 1};
    instrument->numEnvs = 2;

    instrument->lfos[0] = (Oscillator){sineWave, 0.0f, 0.0f, {{5.0f, 0.10f}}};
    instrument->lfos[1] = (Oscillator){sineWave, 0.0f, 0.0f, {{0.18f, 1.0f}}};
    instrument->numLFOS = 2;

    instrument->filters[0] = (LPF){4, {{2500.0f, 0.20f}}};
    instrument->numFilters = 1;

    instrument->panners[0] = (Panner){{{0.15f}}};
    instrument->numPanners = 1;

    instrument->gain = 0.35f;

    addModRoute(instrument, ADD, MENV, 1, 3500.0f, (ModDest){DFILTER, 0, 0});
    addModRoute(instrument, MULT, MENV, 0, 1.0f, (ModDest){DVOLUME, 0, 0});
    addModRoutePrio(instrument, MULTADD, MLFO, 0, SEMITONE - 1.0f, (ModDest){DPITCH, 0, 0});
    addModRoute(instrument, ADD, MLFO, 1, 0.35f, (ModDest){DPANNER, 0, 0});

    for (int i = 0; i < instrument->numOscs; i++) {
        addSignalNode(instrument, SOSCILLATOR, i);
    }
    addSignalNode(instrument, SMIXER, 0);
    for (int i = 0; i < instrument->numOscs; i++) {
        addSignalNodeInput(instrument, instrument->numOscs, i);
    }
    addSignalNode(instrument, SFILTER, 0);
    addSignalNodeInput(instrument, instrument->numOscs + 1, instrument->numOscs);
    addSignalNode(instrument, SPANNER, 0);
    addSignalNodeInput(instrument, instrument->numOscs + 2, instrument->numOscs + 1);
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
    return 0;
}
