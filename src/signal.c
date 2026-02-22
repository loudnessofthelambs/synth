#include "../headers/signal.h"
#include "../headers/lpf.h"
#include "../headers/oscillator.h"
#include <stdio.h>

static void filterProcess(Node* node, float sr);
static void mixerProcess(Node* node);
static void oscillatorProcess(Node* node, float sr);

void processNode(Node* node, float sr) {

    switch (node->type) {
        case SOSCILLATOR:
            oscillatorProcess(node, sr);
            break;
        case SMIXER:
            mixerProcess(node);
            break;
        case SFILTER:
            filterProcess(node, sr);
            break;
        case SPANNER:
            filterProcess(node, sr);
            break;
    }
}

static void oscillatorProcess(Node* node, float sr) {
    float val = osciNext(
        (Oscillator*)node->preset, 
        (OscillatorState*)node->state[0], 
        (OscillatorParams*)node->params, 
        0.0, 
        sr
    );
    if (val > 1 || val < -1) {
        printf("%p\n", node->state[0]);
    }
    node->output.l = val;
    node->output.r = val;
    //printf("sample: %f\n", node->output.r);

}

static void filterProcess(Node* node, float sr) {
    node->output = (Signal){
        LPFNext(
            (LPF*)node->preset,
            (LPFState*)node->state[0],
            (LPFParams*)node->params,
            (*node->inputs[0]).l,
            sr
        ),
        LPFNext(
            (LPF*)node->preset,
            (LPFState*)node->state[1],
            (LPFParams*)node->params,
            (*node->inputs[0]).r,
            sr
        )
    };
    return;
}

static void mixerProcess(Node* node) {
    node->output = (Signal){0.0,0.0};
    for (int i = 0; i < node->numInputs; i++) {
        node->output = addSignals(node->output, *node->inputs[i]);
    }
    
}

Signal addSignals(Signal signal1, Signal signal2) {
    Signal ret;
    ret.l = signal1.l + signal2.l;
    ret.r = signal1.r + signal2.r;
    return ret;
}