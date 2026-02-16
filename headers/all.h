#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif

#define SEMITONE 1.0594630943592952646

float constOne(void*, void*, float, float);

float midiToFreq(float num);