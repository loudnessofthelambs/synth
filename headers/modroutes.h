#pragma once

#define MAX_ROUTES_PER_ROW 16
#define MAX_ROWS 32

typedef struct ModRoute ModRoute;
typedef struct ModRow ModRow;
typedef struct ModDest ModDest;
typedef enum {
    ADD,
    MULT,
    MULTADD, // basically adds a percentage instead of multiplying the whole thing
    ASSIGN,
} MODROUTE_MODE;
typedef enum {
    LFO,
    ENV,
    FILTER,
    CONSTANT, //alternatively equivalent to voice volume in the case of ModDest
    ROUTE,
    OSCILLATOR,
    VOLUME,
} Modifier;


struct ModDest {
    Modifier modifier;
    int index;
    int field;
};
struct ModRoute {
    float (*next)(void*, void*, float, float);
    float amount;
    float* dest;
    void* preset;
    void* state;
    Modifier modifier;
    int index;
    ModDest destination;
    MODROUTE_MODE mode;
};



typedef ModRoute ModMatrix[MAX_ROWS];



void applyModRoute(ModRoute* modroute, float sr);

void applyModMatrix(ModMatrix modroutes, int length, float sr);
