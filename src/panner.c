#include "../headers/panner.h"
#include <math.h>

void pannerProcess(Node* node, float sr) {
    (void)sr;
    float pan = ((PannerParams*)node->params)->pan;
    float mono = (node->inputs[0]->l + node->inputs[0]->r) * 0.5f;
    float gl = sqrtf((1.0f - pan) * 0.5f);
    float gr = sqrtf((1.0f + pan) * 0.5f);
    node->output = (Signal){mono * gl, mono * gr};
}
