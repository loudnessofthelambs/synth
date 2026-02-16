

#include "../headers/modroutes.h"



void applyModRow(ModRow* modrow, float sr) {
    *modrow->destination = *modrow->base; 
    for (int i = 0; i < modrow->routesNum; i++) {
        float next = modrow->routes[i].next(modrow->routes[i].preset, modrow->routes[i].state, *modrow->destination, sr);
        if (modrow->routes[i].mode == ADD) {
            *modrow->destination += next * modrow->routes[i].amount;
        } else if (modrow->routes[i].mode == MULT) {
            *modrow->destination *= next * modrow->routes[i].amount;
        }

    }


    return;
}

void applyModMatrix(ModMatrix modroutes, int length, float sr) {
    for (int i = 0; i < length; i++) {
        applyModRow(&modroutes[i], sr);
    }
    return;
}