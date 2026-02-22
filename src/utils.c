#include "../headers/utils.h"
#include <math.h>



float midiToFreq(float num) {
    return 440.0*pow(2.0,(num-69)/12.0)/2.0;
}