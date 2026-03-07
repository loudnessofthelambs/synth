#include "../headers/panner.h"
#include <math.h>

void pannerProcess(Node* node, float sr) {
    float pan = ((PannerParams*)node->params)->pan;
    float gl = sqrtf((1.0-pan)/2.0);
    float gr = sqrtf((1.0+pan)/2.0);
    node->output = (Signal){node->inputs[0]->l*gl, node->inputs[0]->r*gr};
}