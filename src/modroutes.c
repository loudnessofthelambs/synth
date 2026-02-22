

#include "../headers/modroutes.h"
#include "../headers/lpf.h"
//#include <stdio.h>



void applyModRow(ModRoute* modrow, float sr) {
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wcompare-distinct-pointer-types"
    if (modrow->next == LPFNext) {
        //printf("Here%f\n", *modrow->dest);
    }
    float next = modrow->next(modrow->preset, modrow->state, modrow->params, *modrow->dest, sr);
    if (modrow->next == LPFNext) {
        //printf("Here%f\n", *modrow->dest);
    }
    //printf("Here\n");
    if (modrow->mode == ADD) {
        *modrow->dest += next * modrow->amount;
        
    } else if (modrow->mode == MULT) {
        //printf("Here%f\n", next);
        *modrow->dest *= next * modrow->amount;

        
    } else if (modrow->mode == ASSIGN) {
        if (modrow->next == LPFNext) {
            //printf("Here%f\n", next);
        }
        *modrow->dest = next * modrow->amount;
        if (modrow->next == LPFNext) {
            //printf("Here%f\n", *modrow->dest);
        }
        
    }
    else if (modrow->mode == MULTADD) {
        //printf("Here%f\n", next);
        *modrow->dest *= 1+ next * modrow->amount;

        
    }
    

    return;
    #pragma clang diagnostic pop
}


void applyModMatrix(ModMatrix modroutes, int length, float sr) {

    for (int i = 0; i < length; i++) {
        applyModRow(&modroutes[i], sr);
        
    }
    return;
}