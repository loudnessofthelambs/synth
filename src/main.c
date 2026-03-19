#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../headers/instrumentio.h"
#include "../headers/midi.h"
#include "../headers/presets.h"
#include "../headers/signal.h"
#include "../headers/voice.h"

#define BLOCK_SIZE 512

typedef struct WavHeader WavHeader;
typedef struct WavFile WavFile;
typedef struct RenderEvent RenderEvent;

struct WavHeader {
    char riff[4];
    uint32_t file_size;
    char wave[4];
    char fmt[4];
    uint32_t fmt_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data[4];
    uint32_t data_size;
};

struct WavFile {
    WavHeader header;
    FILE* fp;
    uint32_t bytesWritten;
};

struct RenderEvent {
    Instrument* instrument;
    double startSeconds;
    double durationSeconds;
    int midiNote;
    float velocity;
    float volume;
    int startSample;
    int endSample;
    Voice* voice;
    uint64_t voiceGeneration;
};

static float bpm = 125.0f;
static int numChannels = 2;

static void printUsage(void) {
    printf("audio-utils\n");
    printf("Usage:\n");
    printf("  ./audio [--instrument lead|warmbass|pluck|glasskeys|atmospad] [--load-preset file]\n");
    printf("          [--set key=value] [--save-preset file] [--midi file.mid]\n");
    printf("          [--output file.wav] [--bpm value] [--list-instruments]\n");
    printf("\n");
    printf("Examples:\n");
    printf("  ./audio --instrument lead --set osc0.gain=0.25 --save-preset bright.preset\n");
    printf("  ./audio --load-preset bright.preset --midi song.mid --output song.wav\n");
    printf("  ./audio --instrument atmospad --set chorus0.mix=0.45 --save-preset custom-pad.preset\n");
}

static void printInstrumentList(void) {
    printf("Built-in instruments:\n");
    printf("  warmbass\n");
    printf("  lead\n");
    printf("  pluck\n");
    printf("  glasskeys\n");
    printf("  atmospad\n");
}

static int timeToSamples(double seconds, float sampleRate) {
    return (int)llround(seconds * sampleRate);
}

static double beatsToSeconds(double beats) {
    return beats * 60.0 / bpm;
}

static float clampSample(float value) {
    if (value > 1.0f) return 1.0f;
    if (value < -1.0f) return -1.0f;
    return value;
}

static int openWavFile(WavFile* file, const char* path, float sampleRate) {
    memset(file, 0, sizeof(*file));
    file->fp = fopen(path, "wb+");
    if (file->fp == NULL) {
        fprintf(stderr, "Error opening %s for writing.\n", path);
        return 0;
    }

    memcpy(file->header.riff, "RIFF", 4);
    memcpy(file->header.wave, "WAVE", 4);
    memcpy(file->header.fmt, "fmt ", 4);
    memcpy(file->header.data, "data", 4);
    file->header.fmt_size = 16;
    file->header.audio_format = 1;
    file->header.num_channels = (uint16_t)numChannels;
    file->header.sample_rate = (uint32_t)sampleRate;
    file->header.bits_per_sample = 16;
    file->header.block_align = file->header.num_channels * (file->header.bits_per_sample / 8);
    file->header.byte_rate = file->header.sample_rate * file->header.block_align;

    fwrite(&file->header, sizeof(file->header), 1, file->fp);
    return 1;
}

static void writeBlock(WavFile* file, const Signal* block, int frames) {
    int16_t pcm[BLOCK_SIZE * 2];

    for (int i = 0; i < frames; i++) {
        Signal sample = block[i];
        sample.l = clampSample(sample.l);
        sample.r = clampSample(sample.r);

        if (numChannels == 1) {
            pcm[i] = (int16_t)(((sample.l + sample.r) * 0.5f) * 32767.0f);
        } else {
            pcm[i * 2] = (int16_t)(sample.l * 32767.0f);
            pcm[i * 2 + 1] = (int16_t)(sample.r * 32767.0f);
        }
    }

    size_t samplesToWrite = (size_t)frames * (size_t)numChannels;
    fwrite(pcm, sizeof(int16_t), samplesToWrite, file->fp);
    file->bytesWritten += (uint32_t)(samplesToWrite * sizeof(int16_t));
}

static void finalizeWavFile(WavFile* file) {
    file->header.data_size = file->bytesWritten;
    file->header.file_size = file->bytesWritten + 36U;
    fseek(file->fp, 0, SEEK_SET);
    fwrite(&file->header, sizeof(file->header), 1, file->fp);
    fclose(file->fp);
}

static void prepareEvents(RenderEvent* events, int eventCount, float sampleRate, int* songEndSample) {
    int endSample = 0;

    for (int i = 0; i < eventCount; i++) {
        events[i].startSample = timeToSamples(events[i].startSeconds, sampleRate);
        events[i].endSample = timeToSamples(events[i].startSeconds + events[i].durationSeconds, sampleRate);
        events[i].voice = NULL;
        events[i].voiceGeneration = 0;
        if (events[i].endSample > endSample) {
            endSample = events[i].endSample;
        }
    }
    *songEndSample = endSample;
}

static void renderFrames(WavFile* file, Synth* synth, int frames) {
    Signal block[BLOCK_SIZE] = {0};
    for (int i = 0; i < frames; i++) {
        block[i] = synthNextFrame(synth);
    }
    writeBlock(file, block, frames);
}

static void renderArrangement(WavFile* file, Synth* synth, RenderEvent* events, int eventCount) {
    int songEndSample = 0;
    int currentSample = 0;

    prepareEvents(events, eventCount, synth->sampleRate, &songEndSample);

    while (currentSample < songEndSample || synthHasActiveVoices(synth)) {
        for (int i = 0; i < eventCount; i++) {
            if (events[i].startSample == currentSample) {
                Voice* voice = synthAcquireVoice(synth);
                voiceOn(
                    voice,
                    events[i].instrument,
                    events[i].midiNote,
                    events[i].velocity,
                    events[i].volume,
                    synth->sampleRate
                );
                events[i].voice = voice;
                events[i].voiceGeneration = voice->generation;
            }
        }

        for (int i = 0; i < eventCount; i++) {
            if (events[i].endSample == currentSample &&
                events[i].voice != NULL &&
                events[i].voice->generation == events[i].voiceGeneration) {
                events[i].voice->releasedAtFrame = synth->frameIndex;
                voiceOff(events[i].voice);
            }
        }

        int nextEventSample = INT_MAX;
        for (int i = 0; i < eventCount; i++) {
            if (events[i].startSample > currentSample && events[i].startSample < nextEventSample) {
                nextEventSample = events[i].startSample;
            }
            if (events[i].endSample > currentSample && events[i].endSample < nextEventSample) {
                nextEventSample = events[i].endSample;
            }
        }

        int frames = BLOCK_SIZE;
        if (nextEventSample != INT_MAX) {
            int untilEvent = nextEventSample - currentSample;
            if (untilEvent < frames) {
                frames = untilEvent;
            }
        }
        if (frames <= 0) {
            continue;
        }

        renderFrames(file, synth, frames);
        currentSample += frames;
    }
}

static RenderEvent* buildDemoSong(int* eventCount, Instrument* bass, Instrument* lead) {
    static const int bassNotes[] = {
        38, 38, 41, 41, 43, 43, 36, 36,
        38, 38, 41, 41, 45, 45, 43, 43
    };
    static const int melody[] = {
        62, 60, 62, 65, 69, 65, 62, 60,
        62, 60, 58, 60, 62, 65, 69, 72
    };
    static const int chord1[] = {62, 65, 69};
    static const int chord2[] = {60, 64, 67};
    static const int chord3[] = {58, 62, 65};
    static const int chord4[] = {65, 69, 72};
    const int chordCount = 3;

    *eventCount = 16 + 16 + (4 * chordCount);
    RenderEvent* events = calloc((size_t)*eventCount, sizeof(*events));
    int cursor = 0;

    if (events == NULL) {
        *eventCount = 0;
        return NULL;
    }

    for (int i = 0; i < 16; i++) {
        events[cursor++] = (RenderEvent){
            .instrument = bass,
            .startSeconds = beatsToSeconds((double)i),
            .durationSeconds = beatsToSeconds(0.85),
            .midiNote = bassNotes[i],
            .velocity = 1.0f,
            .volume = 0.95f,
        };
    }
    for (int i = 0; i < 16; i++) {
        events[cursor++] = (RenderEvent){
            .instrument = lead,
            .startSeconds = beatsToSeconds(i * 0.5),
            .durationSeconds = beatsToSeconds(0.45),
            .midiNote = melody[i],
            .velocity = 1.0f,
            .volume = 0.85f,
        };
    }

    const int* chords[] = {chord1, chord2, chord3, chord4};
    for (int chordIndex = 0; chordIndex < 4; chordIndex++) {
        for (int noteIndex = 0; noteIndex < chordCount; noteIndex++) {
            events[cursor++] = (RenderEvent){
                .instrument = lead,
                .startSeconds = beatsToSeconds(chordIndex * 2.0),
                .durationSeconds = beatsToSeconds(2.0),
                .midiNote = chords[chordIndex][noteIndex],
                .velocity = 0.9f,
                .volume = chordIndex == 0 ? 0.30f : (chordIndex == 3 ? 0.32f : 0.28f),
            };
        }
    }

    return events;
}

static RenderEvent* buildMidiSong(const MidiSong* midiSong, int* eventCount, Instrument* instrument) {
    RenderEvent* events = calloc((size_t)midiSong->count, sizeof(*events));
    if (events == NULL) {
        *eventCount = 0;
        return NULL;
    }

    *eventCount = midiSong->count;
    for (int i = 0; i < midiSong->count; i++) {
        events[i] = (RenderEvent){
            .instrument = instrument,
            .startSeconds = midiSong->events[i].startSeconds,
            .durationSeconds = midiSong->events[i].durationSeconds,
            .midiNote = midiSong->events[i].midiNote,
            .velocity = midiSong->events[i].velocity,
            .volume = 0.9f,
        };
    }
    return events;
}

static int parseFloatOption(const char* text, float* out) {
    char* end = NULL;
    *out = strtof(text, &end);
    return end != text && *end == '\0';
}

int main(int argc, char** argv) {
    const float sampleRate = 44100.0f;
    const char* outputPath = "audio6.wav";
    const char* midiPath = NULL;
    const char* loadPresetPath = NULL;
    const char* savePresetPath = NULL;
    const char* instrumentName = "lead";
    const char* settings[64] = {0};
    int settingCount = 0;
    Instrument primaryInstrument;
    Instrument warmBass;
    char presetName[128] = "lead";
    RenderEvent* events = NULL;
    int eventCount = 0;
    MidiSong midiSong = {0};
    Synth synth;
    WavFile wavFile;

    srand(67);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            printUsage();
            return 0;
        }
        if (strcmp(argv[i], "--list-instruments") == 0) {
            printInstrumentList();
            return 0;
        }
        if (strcmp(argv[i], "--instrument") == 0 && i + 1 < argc) {
            instrumentName = argv[++i];
            snprintf(presetName, sizeof(presetName), "%s", instrumentName);
            continue;
        }
        if (strcmp(argv[i], "--load-preset") == 0 && i + 1 < argc) {
            loadPresetPath = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--save-preset") == 0 && i + 1 < argc) {
            savePresetPath = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--midi") == 0 && i + 1 < argc) {
            midiPath = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            outputPath = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--bpm") == 0 && i + 1 < argc) {
            if (!parseFloatOption(argv[++i], &bpm)) {
                fprintf(stderr, "Invalid bpm value.\n");
                return 1;
            }
            continue;
        }
        if (strcmp(argv[i], "--set") == 0 && i + 1 < argc) {
            if (settingCount >= 64) {
                fprintf(stderr, "Too many --set arguments.\n");
                return 1;
            }
            settings[settingCount++] = argv[++i];
            continue;
        }

        outputPath = argv[i];
    }

    if (loadPresetPath != NULL) {
        if (!loadInstrumentFile(loadPresetPath, &primaryInstrument, presetName, sizeof(presetName))) {
            fprintf(stderr, "Failed to load preset: %s\n", loadPresetPath);
            return 1;
        }
    } else if (!loadBuiltInInstrument(instrumentName, &primaryInstrument)) {
        fprintf(stderr, "Unknown instrument: %s\n", instrumentName);
        return 1;
    }

    for (int i = 0; i < settingCount; i++) {
        if (!applyInstrumentSetting(&primaryInstrument, settings[i])) {
            fprintf(stderr, "Invalid instrument setting: %s\n", settings[i]);
            return 1;
        }
    }

    if (savePresetPath != NULL && !saveInstrumentFile(savePresetPath, &primaryInstrument, presetName)) {
        fprintf(stderr, "Failed to save preset: %s\n", savePresetPath);
        return 1;
    }

    initWarmBass(&warmBass);
    if (midiPath != NULL) {
        if (!loadMidiFile(midiPath, &midiSong)) {
            fprintf(stderr, "Failed to load MIDI file: %s\n", midiPath);
            return 1;
        }
        events = buildMidiSong(&midiSong, &eventCount, &primaryInstrument);
    } else {
        events = buildDemoSong(&eventCount, &warmBass, &primaryInstrument);
    }
    if (events == NULL) {
        fprintf(stderr, "Failed to build render events.\n");
        freeMidiSong(&midiSong);
        return 1;
    }

    if (!openWavFile(&wavFile, outputPath, sampleRate)) {
        free(events);
        freeMidiSong(&midiSong);
        return 1;
    }

    synthInit(&synth, sampleRate);
    renderArrangement(&wavFile, &synth, events, eventCount);
    finalizeWavFile(&wavFile);

    free(events);
    freeMidiSong(&midiSong);
    return 0;
}
