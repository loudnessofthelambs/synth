#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../headers/adsr.h"
#include "../headers/fx.h"
#include "../headers/instrumentio.h"
#include "../headers/lpf.h"
#include "../headers/midi.h"
#include "../headers/modroutes.h"
#include "../headers/oscillator.h"
#include "../headers/panner.h"
#include "../headers/presets.h"
#include "../headers/signal.h"
#include "../headers/utils.h"
#include "../headers/voice.h"

typedef struct TestState TestState;

struct TestState {
    int passed;
    int failed;
};

#define EXPECT_TRUE(state, condition, label) \
    do { \
        if (condition) { \
            printf("  [PASS] %s\n", label); \
            (state)->passed += 1; \
        } else { \
            printf("  [FAIL] %s\n", label); \
            (state)->failed += 1; \
        } \
    } while (0)

#define EXPECT_NEAR(state, actual, expected, epsilon, label) \
    EXPECT_TRUE(state, fabs((double)((actual) - (expected))) <= (epsilon), label)

static void printComponent(const char* name) {
    printf("\n[%s]\n", name);
}

static Instrument makeSimpleInstrument(void) {
    Instrument instrument = {0};

    instrument.oscs[0] = (Oscillator){squareWave, 0.0f, 0.0f, {{0.0f, 0.2f}}};
    instrument.numOscs = 1;
    instrument.envs[0] = (ADSREnv){0.001f, 0.001f, 0.5f, 0.002f, 1};
    instrument.numEnvs = 1;
    instrument.panners[0] = (Panner){{{0.0f}}};
    instrument.numPanners = 1;
    instrument.gain = 1.0f;

    addModRoute(&instrument, MULT, MENV, 0, 1.0f, (ModDest){DVOLUME, 0, 0});
    addSignalNode(&instrument, SOSCILLATOR, 0);
    addSignalNode(&instrument, SPANNER, 0);
    addSignalNodeInput(&instrument, 1, 0);
    return instrument;
}

static int writeTestMidiFile(const char* path) {
    static const unsigned char midiBytes[] = {
        'M','T','h','d', 0x00,0x00,0x00,0x06, 0x00,0x00, 0x00,0x01, 0x01,0xE0,
        'M','T','r','k', 0x00,0x00,0x00,0x14,
        0x00, 0xFF,0x51,0x03, 0x07,0xA1,0x20,
        0x00, 0x90,0x3C,0x64,
        0x83,0x60, 0x80,0x3C,0x40,
        0x00, 0xFF,0x2F,0x00
    };
    FILE* file = fopen(path, "wb");
    if (file == NULL) {
        return 0;
    }
    fwrite(midiBytes, sizeof(midiBytes), 1, file);
    fclose(file);
    return 1;
}

static void testUtils(TestState* state) {
    printComponent("utils");
    EXPECT_NEAR(state, midiToFreq(69.0f), 440.0, 0.001, "midiToFreq maps A4 to 440 Hz");
    EXPECT_NEAR(state, midiToFreq(60.0f), 261.625565, 0.01, "midiToFreq maps middle C correctly");
}

static void testAdsr(TestState* state) {
    ADSREnv env = {0.01f, 0.01f, 0.5f, 0.01f, 1};
    ADSREnvState adsrState = {ADSR_ATTACK, 0.0f, 1, 0.0f};

    printComponent("adsr");
    adsrAdvance(&env, &adsrState, 1000.0f);
    EXPECT_TRUE(state, adsrState.value > 0.0f, "attack raises the envelope");

    for (int i = 0; i < 20; i++) {
        adsrAdvance(&env, &adsrState, 1000.0f);
    }
    EXPECT_TRUE(state, adsrState.stage == ADSR_SUSTAIN, "envelope reaches sustain");

    adsrState.stage = ADSR_RELEASE;
    adsrState.releaseValue = adsrState.value;
    for (int i = 0; i < 20; i++) {
        adsrAdvance(&env, &adsrState, 1000.0f);
    }
    EXPECT_TRUE(state, adsrState.stage == ADSR_OFF, "release reaches off state");
}

static void testOscillators(TestState* state) {
    OscillatorParams params = {.freq = 100.0f, .gain = 1.0f};
    OscillatorState oscState = {.phase = 0.0f, .last = 0.0f, .used = 1};
    Oscillator osc = {sineWave, 0.0f, 0.0f, {{100.0f, 1.0f}}};
    float randomSample = 0.0f;

    printComponent("oscillator");
    EXPECT_NEAR(state, sineWave(&oscState, &params, 44100.0f), 0.0, 0.0001, "sine wave starts at zero");
    EXPECT_NEAR(state, sawWave(&oscState, &params, 44100.0f), -1.0, 0.0001, "saw wave starts at -1");

    osciNext(&osc, &oscState, &params, 0.0f, 1000.0f);
    EXPECT_TRUE(state, oscState.phase > 0.0f, "osciNext advances phase");

    srand(67);
    randomSample = noise(&oscState, &params, 44100.0f);
    EXPECT_TRUE(state, randomSample >= -1.0f && randomSample <= 1.0f, "noise stays in [-1, 1]");
}

static void testFilter(TestState* state) {
    LPF filter = {FILTER_LADDER, 2, {{500.0f, 0.1f, 0.0f}}};
    LPFParams params = {.cutoff = 500.0f, .resonance = 0.1f, .gainDB = 0.0f};
    LPFState filterState = {0};
    float output = 0.0f;

    printComponent("lpf");
    for (int i = 0; i < 16; i++) {
        output = LPFNext(&filter, &filterState, &params, 1.0f, 44100.0f);
    }
    EXPECT_TRUE(state, output > 0.0f && output < 1.0f, "LPF smooths a step input");

    filter.mode = FILTER_BIQUAD_HP;
    params.cutoff = 200.0f;
    filterState = (LPFState){0};
    for (int i = 0; i < 64; i++) {
        output = LPFNext(&filter, &filterState, &params, 1.0f, 44100.0f);
    }
    EXPECT_TRUE(state, fabsf(output) < 0.5f, "biquad high-pass rejects DC");
}

static void testModRoutes(TestState* state) {
    float destination = 2.0f;
    ModRoute route = {
        .next = constOne,
        .amount = 0.5f,
        .dest = &destination,
        .mode = MULTADD,
    };

    printComponent("modroutes");
    applyModRoute(&route, 44100.0f);
    EXPECT_NEAR(state, destination, 3.0, 0.0001, "MULTADD scales destination correctly");
}

static void testPanner(TestState* state) {
    Signal input = {1.0f, 1.0f};
    PannerParams params = {.pan = 1.0f};
    Node node = {
        .type = SPANNER,
        .inputs = {&input},
        .numInputs = 1,
        .params = &params,
    };

    printComponent("panner");
    pannerProcess(&node, 44100.0f);
    EXPECT_NEAR(state, node.output.l, 0.0, 0.0001, "full-right pan mutes left");
    EXPECT_TRUE(state, node.output.r > 0.9f, "full-right pan keeps right loud");
}

static void testEffects(TestState* state) {
    DistortionParams dist = {.drive = 3.0f, .mix = 1.0f, .outputGain = 1.0f};
    DelayParams delay = {.time = 0.01f, .feedback = 0.4f, .mix = 0.5f, .tone = 0.8f};
    ChorusParams chorus = {.rate = 0.5f, .depth = 0.003f, .mix = 0.5f, .baseDelay = 0.006f};
    DelayState delayState = {0};
    ChorusState chorusState = {0};
    float outL = 0.0f;
    float outR = 0.0f;
    float maxDelayEcho = 0.0f;

    printComponent("effects");
    delayState.left = calloc(MAX_DELAY_SAMPLES, sizeof(*delayState.left));
    delayState.right = calloc(MAX_DELAY_SAMPLES, sizeof(*delayState.right));
    delayState.capacity = MAX_DELAY_SAMPLES;
    chorusState.left = calloc(MAX_CHORUS_SAMPLES, sizeof(*chorusState.left));
    chorusState.right = calloc(MAX_CHORUS_SAMPLES, sizeof(*chorusState.right));
    chorusState.capacity = MAX_CHORUS_SAMPLES;

    distortionProcess(NULL, &dist, NULL, 0.8f, -0.8f, &outL, &outR);
    EXPECT_TRUE(state, fabsf(outL) <= 1.0f && fabsf(outL) > 0.8f, "distortion saturates into a bounded output");

    for (int i = 0; i < 128; i++) {
        delayProcess(NULL, &delay, &delayState, 1000.0f, i == 0 ? 1.0f : 0.0f, 0.0f, &outL, &outR);
        if (fabsf(outL) > maxDelayEcho) {
            maxDelayEcho = fabsf(outL);
        }
    }
    EXPECT_TRUE(state, maxDelayEcho > 0.1f, "delay produces audible echo");

    chorusProcess(NULL, &chorus, &chorusState, 1000.0f, 1.0f, 1.0f, &outL, &outR);
    EXPECT_TRUE(state, outL > 0.0f && outR > 0.0f, "chorus produces stereo output");

    free(delayState.left);
    free(delayState.right);
    free(chorusState.left);
    free(chorusState.right);
}

static void testSignal(TestState* state) {
    Signal left = {0.25f, 0.50f};
    Signal right = {0.75f, 0.10f};
    Node mixer = {
        .type = SMIXER,
        .inputs = {&left, &right},
        .numInputs = 2,
    };

    printComponent("signal");
    processNode(&mixer, 44100.0f);
    EXPECT_NEAR(state, mixer.output.l, 1.0, 0.0001, "mixer sums left channel");
    EXPECT_NEAR(state, mixer.output.r, 0.60, 0.0001, "mixer sums right channel");
}

static void testPresets(TestState* state) {
    Instrument lead = {0};
    Instrument pad = {0};

    printComponent("presets");
    initLead(&lead);
    EXPECT_TRUE(state, lead.numOscs == 6, "lead preset creates six oscillators");
    EXPECT_TRUE(state, lead.numPanners == 1, "lead preset includes a panner");
    EXPECT_TRUE(state, loadBuiltInInstrument("warmbass", &lead), "builtin preset lookup works");
    initAtmosPad(&pad);
    EXPECT_TRUE(state, pad.numChoruses == 1, "pad preset includes chorus");
    EXPECT_TRUE(state, pad.numDelays == 1, "pad preset includes delay");
}

static void testInstrumentIo(TestState* state) {
    const char* path = "/tmp/audio-utils-test.preset";
    Instrument lead = {0};
    Instrument loaded = {0};
    char name[64] = {0};

    printComponent("instrumentio");
    initLead(&lead);
    EXPECT_TRUE(state, applyInstrumentSetting(&lead, "osc0.gain=0.42"), "applyInstrumentSetting accepts oscillator assignments");
    EXPECT_TRUE(state, saveInstrumentFile(path, &lead, "test-lead"), "preset save succeeds");
    EXPECT_TRUE(state, loadInstrumentFile(path, &loaded, name, sizeof(name)), "preset load succeeds");
    EXPECT_TRUE(state, strcmp(name, "test-lead") == 0, "preset name round-trips");
    EXPECT_NEAR(state, loaded.oscs[0].baseParams.gain, 0.42, 0.0001, "oscillator gain round-trips");
    EXPECT_TRUE(state, loaded.nodeNum == lead.nodeNum, "signal graph round-trips");
    EXPECT_TRUE(state, applyInstrumentSetting(&lead, "filter0.mode=FILTER_BIQUAD_PEAK"), "filter mode accepts symbolic values");
    EXPECT_TRUE(state, applyInstrumentSetting(&lead, "delay0.time=0.25"), "delay settings are editable");
}

static void testMidi(TestState* state) {
    const char* path = "/tmp/audio-utils-test.mid";
    MidiSong song = {0};

    printComponent("midi");
    EXPECT_TRUE(state, writeTestMidiFile(path), "test MIDI fixture is written");
    EXPECT_TRUE(state, loadMidiFile(path, &song), "MIDI file loads");
    EXPECT_TRUE(state, song.count == 1, "MIDI parser extracts one note");
    if (song.count == 1) {
        EXPECT_TRUE(state, song.events[0].midiNote == 60, "MIDI parser keeps note number");
        EXPECT_NEAR(state, song.events[0].durationSeconds, 0.5, 0.01, "MIDI parser computes duration");
    }
    freeMidiSong(&song);
}

static void testVoice(TestState* state) {
    Instrument instrument = makeSimpleInstrument();
    Synth synth;
    Voice* chosen = NULL;

    printComponent("voice");
    synthInit(&synth, 1000.0f);
    chosen = synthAcquireVoice(&synth);
    voiceOn(chosen, &instrument, 69, 1.0f, 1.0f, synth.sampleRate);
    EXPECT_TRUE(state, chosen->active, "voiceOn activates a voice");

    for (int i = 0; i < 8; i++) {
        (void)voiceNext(chosen);
    }
    voiceOff(chosen);
    for (int i = 0; i < 32; i++) {
        (void)voiceNext(chosen);
    }
    EXPECT_TRUE(state, !chosen->active, "voice release eventually deactivates the voice");

    for (int i = 0; i < MAX_VOICES; i++) {
        synth.voices[i].active = 1;
        synth.voices[i].lastLevel = 0.8f + (float)i;
        synth.voices[i].released = 0;
    }
    synth.voices[3].released = 1;
    synth.voices[3].lastLevel = 0.05f;
    synth.voices[7].released = 1;
    synth.voices[7].lastLevel = 0.10f;
    EXPECT_TRUE(state, synthAcquireVoice(&synth) == &synth.voices[3], "voice stealing prefers the quietest released voice");
}

int main(void) {
    TestState state = {0};

    testUtils(&state);
    testAdsr(&state);
    testOscillators(&state);
    testFilter(&state);
    testModRoutes(&state);
    testPanner(&state);
    testEffects(&state);
    testSignal(&state);
    testPresets(&state);
    testInstrumentIo(&state);
    testMidi(&state);
    testVoice(&state);

    printf("\nSummary: %d passed, %d failed\n", state.passed, state.failed);
    return state.failed == 0 ? 0 : 1;
}
