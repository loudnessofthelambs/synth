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
    MLFO,
    MENV,
    MFILTER,
    MCONSTANT,
} Modifier;
typedef enum {
    DLFO,
    DENV,
    DFILTER,
    DROUTE,
    DOSCILLATOR,
    DVOLUME,
    DCONSTANT, //temporary
} Destination;


struct ModDest {
    Destination modifier;
    int index;
    int field;
};
struct ModRoute {
    float (*next)(void*, void*, void*, float, float);
    float amount;
    float* dest;
    void* preset;
    void* params;
    void* state;
    Modifier modifier;
    int index;
    ModDest destination;
    MODROUTE_MODE mode;
};



typedef ModRoute ModMatrix[MAX_ROWS];



void applyModRoute(ModRoute* modroute, float sr);

void applyModMatrix(ModMatrix modroutes, int length, float sr);
