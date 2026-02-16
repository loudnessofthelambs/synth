#include "../headers/all.h"

float constOne(void*, void*, float, float) {
    return 1.0;
}

float midiToFreq(float num) {
    return 440.0*pow(2.0,(num-69)/12.0)/2.0;
}