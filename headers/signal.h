#pragma once

#include <stdint.h>
#define MAX_INPUTS 8
#define MAX_NODES 16

typedef struct Node Node;
typedef struct NodePreset NodePreset;
typedef struct Signal Signal;

typedef enum {
    SOSCILLATOR,
    SFILTER, 
    SMIXER,
    SPANNER,
} NodeType;

struct Signal {
    float l;
    float r;
};

struct Node {

    NodeType type;

    Signal* inputs[MAX_INPUTS];
    int8_t numInputs;

    Signal output;
    
    void* preset;
    void* params;
    void* state[2]; //two because two channels


};

struct NodePreset {

    NodeType type;
    int index;

    int8_t numInputs;

    int8_t inputIndexes[MAX_INPUTS]; //for initializing

};

void processNode(Node* node, float sr);
Signal addSignals(Signal signal1, Signal signal2);