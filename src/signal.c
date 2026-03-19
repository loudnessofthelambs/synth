#include "../headers/signal.h"
#include "../headers/fx.h"
#include "../headers/lpf.h"
#include "../headers/oscillator.h"
#include "../headers/panner.h"
#include <stdio.h>

static void filterProcess(Node* node, float sr);
static void mixerProcess(Node* node);
static void oscillatorProcess(Node* node, float sr);
static void distortionNodeProcess(Node* node);
static void delayNodeProcess(Node* node, float sr);
static void chorusNodeProcess(Node* node, float sr);

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
            pannerProcess(node, sr);
            break;
        case SDISTORTION:
            distortionNodeProcess(node);
            break;
        case SDELAY:
            delayNodeProcess(node, sr);
            break;
        case SCHORUS:
            chorusNodeProcess(node, sr);
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

static void distortionNodeProcess(Node* node) {
    distortionProcess(
        node->preset,
        (DistortionParams*)node->params,
        NULL,
        node->inputs[0]->l,
        node->inputs[0]->r,
        &node->output.l,
        &node->output.r
    );
}

static void delayNodeProcess(Node* node, float sr) {
    delayProcess(
        node->preset,
        (DelayParams*)node->params,
        (DelayState*)node->state[0],
        sr,
        node->inputs[0]->l,
        node->inputs[0]->r,
        &node->output.l,
        &node->output.r
    );
}

static void chorusNodeProcess(Node* node, float sr) {
    chorusProcess(
        node->preset,
        (ChorusParams*)node->params,
        (ChorusState*)node->state[0],
        sr,
        node->inputs[0]->l,
        node->inputs[0]->r,
        &node->output.l,
        &node->output.r
    );
}

Signal addSignals(Signal signal1, Signal signal2) {
    Signal ret;
    ret.l = signal1.l + signal2.l;
    ret.r = signal1.r + signal2.r;
    return ret;
}
