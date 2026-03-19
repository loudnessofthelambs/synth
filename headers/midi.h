#pragma once

#include <stdint.h>

typedef struct MidiNoteEvent MidiNoteEvent;
typedef struct MidiSong MidiSong;

struct MidiNoteEvent {
    double startSeconds;
    double durationSeconds;
    int midiNote;
    float velocity;
    uint8_t channel;
};

struct MidiSong {
    MidiNoteEvent* events;
    int count;
    double durationSeconds;
};

int loadMidiFile(const char* path, MidiSong* song);
void freeMidiSong(MidiSong* song);
