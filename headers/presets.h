#pragma once

#include "voice.h"

void initWarmBass(Instrument* instrument);
void initLead(Instrument* instrument);
int loadBuiltInInstrument(const char* name, Instrument* instrument);
