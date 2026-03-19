#pragma once

#include "signal.h"

typedef struct Panner Panner;
typedef union PannerParams PannerParams;

union PannerParams {
    struct {
        float pan;
    };
    float paramsArray[1];
};

struct Panner {
    PannerParams baseParams;
};




void pannerProcess(Node* node, float sr);
