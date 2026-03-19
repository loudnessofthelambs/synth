#pragma once

#include <stddef.h>

#include "voice.h"

int saveInstrumentFile(const char* path, const Instrument* instrument, const char* name);
int loadInstrumentFile(const char* path, Instrument* instrument, char* nameBuffer, size_t nameBufferSize);
int applyInstrumentSetting(Instrument* instrument, const char* assignment);
