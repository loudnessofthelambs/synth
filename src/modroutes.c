#include "../headers/modroutes.h"

void applyModRoute(ModRoute* modrow, float sr) {
    float next = modrow->next(modrow->preset, modrow->state, modrow->params, *modrow->dest, sr);
    if (modrow->mode == ADD) {
        *modrow->dest += next * modrow->amount;
    } else if (modrow->mode == MULT) {
        *modrow->dest *= next * modrow->amount;
    } else if (modrow->mode == ASSIGN) {
        *modrow->dest = next * modrow->amount;
    } else if (modrow->mode == MULTADD) {
        *modrow->dest *= 1 + next * modrow->amount;
    }
}


void applyModMatrix(ModMatrix modroutes, int length, float sr) {

    for (int i = 0; i < length; i++) {
        applyModRoute(&modroutes[i], sr);
        
    }
    return;
}
