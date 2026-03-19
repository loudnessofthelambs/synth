#include "../headers/midi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct TempoEvent TempoEvent;
typedef struct NoteRecord NoteRecord;
typedef struct ActiveNote ActiveNote;

struct TempoEvent {
    uint32_t tick;
    uint32_t microsecondsPerQuarter;
    double secondsAtTick;
};

struct NoteRecord {
    uint32_t startTick;
    uint32_t endTick;
    uint8_t note;
    uint8_t channel;
    float velocity;
};

struct ActiveNote {
    uint32_t startTick;
    uint8_t note;
    uint8_t channel;
    float velocity;
    int open;
};

static uint16_t readU16BE(const unsigned char* data) {
    return (uint16_t)((data[0] << 8) | data[1]);
}

static uint32_t readU32BE(const unsigned char* data) {
    return ((uint32_t)data[0] << 24) |
           ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) |
           (uint32_t)data[3];
}

static int appendTempoEvent(TempoEvent** events, int* count, int* capacity, TempoEvent event) {
    if (*count >= *capacity) {
        int newCapacity = *capacity == 0 ? 8 : *capacity * 2;
        TempoEvent* resized = realloc(*events, sizeof(**events) * (size_t)newCapacity);
        if (resized == NULL) {
            return 0;
        }
        *events = resized;
        *capacity = newCapacity;
    }
    (*events)[*count] = event;
    *count += 1;
    return 1;
}

static int appendNoteRecord(NoteRecord** notes, int* count, int* capacity, NoteRecord note) {
    if (*count >= *capacity) {
        int newCapacity = *capacity == 0 ? 32 : *capacity * 2;
        NoteRecord* resized = realloc(*notes, sizeof(**notes) * (size_t)newCapacity);
        if (resized == NULL) {
            return 0;
        }
        *notes = resized;
        *capacity = newCapacity;
    }
    (*notes)[*count] = note;
    *count += 1;
    return 1;
}

static int appendActiveNote(ActiveNote** notes, int* count, int* capacity, ActiveNote note) {
    if (*count >= *capacity) {
        int newCapacity = *capacity == 0 ? 32 : *capacity * 2;
        ActiveNote* resized = realloc(*notes, sizeof(**notes) * (size_t)newCapacity);
        if (resized == NULL) {
            return 0;
        }
        *notes = resized;
        *capacity = newCapacity;
    }
    (*notes)[*count] = note;
    *count += 1;
    return 1;
}

static int readVLQ(const unsigned char* data, size_t size, size_t* offset, uint32_t* value) {
    uint32_t result = 0;
    int count = 0;

    do {
        if (*offset >= size || count == 4) {
            return 0;
        }
        result = (result << 7) | (uint32_t)(data[*offset] & 0x7F);
        count += 1;
    } while (data[(*offset)++] & 0x80);

    *value = result;
    return 1;
}

static int compareTempoEvents(const void* lhs, const void* rhs) {
    const TempoEvent* left = lhs;
    const TempoEvent* right = rhs;
    if (left->tick < right->tick) return -1;
    if (left->tick > right->tick) return 1;
    return 0;
}

static int compareNoteRecords(const void* lhs, const void* rhs) {
    const NoteRecord* left = lhs;
    const NoteRecord* right = rhs;
    if (left->startTick < right->startTick) return -1;
    if (left->startTick > right->startTick) return 1;
    if (left->note < right->note) return -1;
    if (left->note > right->note) return 1;
    return 0;
}

static double tickToSeconds(uint32_t tick, const TempoEvent* tempos, int tempoCount, uint16_t division) {
    const TempoEvent* current = &tempos[0];

    for (int i = 1; i < tempoCount; i++) {
        if (tempos[i].tick > tick) {
            break;
        }
        current = &tempos[i];
    }

    return current->secondsAtTick +
        ((double)(tick - current->tick) * (double)current->microsecondsPerQuarter) /
        ((double)division * 1000000.0);
}

static int finalizeTempoMap(TempoEvent** events, int* count, int* capacity, uint16_t division) {
    if (*count == 0 || (*events)[0].tick != 0) {
        TempoEvent defaultTempo = {0, 500000, 0.0};
        if (!appendTempoEvent(events, count, capacity, defaultTempo)) {
            return 0;
        }
    }

    qsort(*events, (size_t)*count, sizeof(**events), compareTempoEvents);

    int writeIndex = 1;
    for (int i = 1; i < *count; i++) {
        if ((*events)[i].tick == (*events)[writeIndex - 1].tick) {
            (*events)[writeIndex - 1] = (*events)[i];
        } else {
            (*events)[writeIndex++] = (*events)[i];
        }
    }
    *count = writeIndex;
    (*events)[0].secondsAtTick = 0.0;

    for (int i = 1; i < *count; i++) {
        uint32_t deltaTicks = (*events)[i].tick - (*events)[i - 1].tick;
        (*events)[i].secondsAtTick = (*events)[i - 1].secondsAtTick +
            ((double)deltaTicks * (double)(*events)[i - 1].microsecondsPerQuarter) /
            ((double)division * 1000000.0);
    }
    return 1;
}

static int parseTrack(
    const unsigned char* data,
    size_t size,
    TempoEvent** tempos,
    int* tempoCount,
    int* tempoCapacity,
    NoteRecord** notes,
    int* noteCount,
    int* noteCapacity,
    ActiveNote** activeNotes,
    int* activeCount,
    int* activeCapacity
) {
    size_t offset = 0;
    uint32_t absoluteTick = 0;
    unsigned char runningStatus = 0;

    while (offset < size) {
        uint32_t delta = 0;
        if (!readVLQ(data, size, &offset, &delta)) {
            return 0;
        }
        absoluteTick += delta;
        if (offset >= size) {
            return 0;
        }

        unsigned char status = data[offset];
        if (status < 0x80) {
            if (runningStatus == 0) {
                return 0;
            }
            status = runningStatus;
        } else {
            offset += 1;
            runningStatus = status;
        }

        if (status == 0xFF) {
            if (offset >= size) {
                return 0;
            }
            unsigned char metaType = data[offset++];
            uint32_t metaLength = 0;
            if (!readVLQ(data, size, &offset, &metaLength) || offset + metaLength > size) {
                return 0;
            }
            if (metaType == 0x51 && metaLength == 3) {
                TempoEvent event = {
                    .tick = absoluteTick,
                    .microsecondsPerQuarter = ((uint32_t)data[offset] << 16) |
                                              ((uint32_t)data[offset + 1] << 8) |
                                              (uint32_t)data[offset + 2],
                    .secondsAtTick = 0.0,
                };
                if (!appendTempoEvent(tempos, tempoCount, tempoCapacity, event)) {
                    return 0;
                }
            }
            offset += metaLength;
            continue;
        }

        if (status == 0xF0 || status == 0xF7) {
            uint32_t sysexLength = 0;
            if (!readVLQ(data, size, &offset, &sysexLength) || offset + sysexLength > size) {
                return 0;
            }
            offset += sysexLength;
            continue;
        }

        const unsigned char eventType = status & 0xF0;
        const unsigned char channel = status & 0x0F;
        unsigned char data1 = 0;
        unsigned char data2 = 0;

        if (status == runningStatus && data[offset] < 0x80) {
            data1 = data[offset++];
        } else {
            data1 = data[offset++];
        }

        if (eventType != 0xC0 && eventType != 0xD0) {
            if (offset >= size) {
                return 0;
            }
            data2 = data[offset++];
        }

        if (eventType == 0x90 && data2 > 0) {
            ActiveNote note = {
                .startTick = absoluteTick,
                .note = data1,
                .channel = channel,
                .velocity = (float)data2 / 127.0f,
                .open = 1,
            };
            if (!appendActiveNote(activeNotes, activeCount, activeCapacity, note)) {
                return 0;
            }
            continue;
        }

        if (eventType == 0x80 || (eventType == 0x90 && data2 == 0)) {
            for (int i = *activeCount - 1; i >= 0; i--) {
                if ((*activeNotes)[i].open &&
                    (*activeNotes)[i].channel == channel &&
                    (*activeNotes)[i].note == data1) {
                    (*activeNotes)[i].open = 0;
                    NoteRecord record = {
                        .startTick = (*activeNotes)[i].startTick,
                        .endTick = absoluteTick,
                        .note = data1,
                        .channel = channel,
                        .velocity = (*activeNotes)[i].velocity,
                    };
                    if (!appendNoteRecord(notes, noteCount, noteCapacity, record)) {
                        return 0;
                    }
                    break;
                }
            }
        }
    }

    return 1;
}

int loadMidiFile(const char* path, MidiSong* song) {
    FILE* file = fopen(path, "rb");
    unsigned char header[14];
    TempoEvent* tempos = NULL;
    NoteRecord* notes = NULL;
    ActiveNote* activeNotes = NULL;
    int tempoCount = 0;
    int tempoCapacity = 0;
    int noteCount = 0;
    int noteCapacity = 0;
    int activeCount = 0;
    int activeCapacity = 0;
    uint16_t tracks = 0;
    uint16_t division = 0;
    int ok = 0;

    *song = (MidiSong){0};
    if (file == NULL) {
        return 0;
    }
    if (fread(header, sizeof(header), 1, file) != 1) {
        goto cleanup;
    }
    if (memcmp(header, "MThd", 4) != 0 || readU32BE(header + 4) < 6) {
        goto cleanup;
    }

    tracks = readU16BE(header + 10);
    division = readU16BE(header + 12);
    if (division == 0 || (division & 0x8000) != 0) {
        goto cleanup;
    }

    for (uint16_t trackIndex = 0; trackIndex < tracks; trackIndex++) {
        unsigned char trackHeader[8];
        uint32_t trackLength = 0;
        unsigned char* trackData = NULL;

        if (fread(trackHeader, sizeof(trackHeader), 1, file) != 1) {
            goto cleanup;
        }
        if (memcmp(trackHeader, "MTrk", 4) != 0) {
            goto cleanup;
        }
        trackLength = readU32BE(trackHeader + 4);
        trackData = malloc(trackLength);
        if (trackData == NULL) {
            goto cleanup;
        }
        if (fread(trackData, 1, trackLength, file) != trackLength) {
            free(trackData);
            goto cleanup;
        }
        if (!parseTrack(
                trackData,
                trackLength,
                &tempos,
                &tempoCount,
                &tempoCapacity,
                &notes,
                &noteCount,
                &noteCapacity,
                &activeNotes,
                &activeCount,
                &activeCapacity)) {
            free(trackData);
            goto cleanup;
        }
        free(trackData);
    }

    if (!finalizeTempoMap(&tempos, &tempoCount, &tempoCapacity, division)) {
        goto cleanup;
    }

    qsort(notes, (size_t)noteCount, sizeof(*notes), compareNoteRecords);
    song->events = calloc((size_t)noteCount, sizeof(*song->events));
    if (song->events == NULL && noteCount > 0) {
        goto cleanup;
    }
    song->count = noteCount;
    song->durationSeconds = 0.0;

    for (int i = 0; i < noteCount; i++) {
        double startSeconds = tickToSeconds(notes[i].startTick, tempos, tempoCount, division);
        double endSeconds = tickToSeconds(notes[i].endTick, tempos, tempoCount, division);
        song->events[i] = (MidiNoteEvent){
            .startSeconds = startSeconds,
            .durationSeconds = endSeconds - startSeconds,
            .midiNote = notes[i].note,
            .velocity = notes[i].velocity,
            .channel = notes[i].channel,
        };
        if (endSeconds > song->durationSeconds) {
            song->durationSeconds = endSeconds;
        }
    }

    ok = 1;

cleanup:
    fclose(file);
    free(tempos);
    free(notes);
    free(activeNotes);
    if (!ok) {
        freeMidiSong(song);
    }
    return ok;
}

void freeMidiSong(MidiSong* song) {
    free(song->events);
    *song = (MidiSong){0};
}
