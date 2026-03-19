#include "../headers/instrumentio.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void trim(char* text) {
    size_t len = strlen(text);
    while (len > 0 && isspace((unsigned char)text[len - 1])) {
        text[--len] = '\0';
    }
    size_t start = 0;
    while (text[start] != '\0' && isspace((unsigned char)text[start])) {
        start += 1;
    }
    if (start > 0) {
        memmove(text, text + start, strlen(text + start) + 1);
    }
}

static int parseInt(const char* text, int* out) {
    char* end = NULL;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0') {
        return 0;
    }
    *out = (int)value;
    return 1;
}

static int parseFloat(const char* text, float* out) {
    char* end = NULL;
    float value = strtof(text, &end);
    if (end == text || *end != '\0') {
        return 0;
    }
    *out = value;
    return 1;
}

static const char* waveToName(float (*wave)(OscillatorState*, OscillatorParams*, float)) {
    if (wave == sineWave) return "sine";
    if (wave == sawWave) return "saw";
    if (wave == squareWave) return "square";
    if (wave == triangleWave) return "triangle";
    if (wave == polyblepSaw) return "polyblepSaw";
    if (wave == polyblepSquare) return "polyblepSquare";
    if (wave == polyblepTriangle) return "polyblepTriangle";
    if (wave == noise) return "noise";
    return "sine";
}

static float (*nameToWave(const char* name))(OscillatorState*, OscillatorParams*, float) {
    if (strcmp(name, "sine") == 0) return sineWave;
    if (strcmp(name, "saw") == 0) return sawWave;
    if (strcmp(name, "square") == 0) return squareWave;
    if (strcmp(name, "triangle") == 0) return triangleWave;
    if (strcmp(name, "polyblepSaw") == 0) return polyblepSaw;
    if (strcmp(name, "polyblepSquare") == 0) return polyblepSquare;
    if (strcmp(name, "polyblepTriangle") == 0) return polyblepTriangle;
    if (strcmp(name, "noise") == 0) return noise;
    return NULL;
}

static const char* modeToName(MODROUTE_MODE mode) {
    switch (mode) {
        case ADD: return "ADD";
        case MULT: return "MULT";
        case MULTADD: return "MULTADD";
        case ASSIGN: return "ASSIGN";
    }
    return "ADD";
}

static int nameToMode(const char* name, MODROUTE_MODE* mode) {
    if (strcmp(name, "ADD") == 0) *mode = ADD;
    else if (strcmp(name, "MULT") == 0) *mode = MULT;
    else if (strcmp(name, "MULTADD") == 0) *mode = MULTADD;
    else if (strcmp(name, "ASSIGN") == 0) *mode = ASSIGN;
    else return 0;
    return 1;
}

static const char* modifierToName(Modifier modifier) {
    switch (modifier) {
        case MLFO: return "MLFO";
        case MENV: return "MENV";
        case MFILTER: return "MFILTER";
        case MCONSTANT: return "MCONSTANT";
    }
    return "MCONSTANT";
}

static int nameToModifier(const char* name, Modifier* modifier) {
    if (strcmp(name, "MLFO") == 0) *modifier = MLFO;
    else if (strcmp(name, "MENV") == 0) *modifier = MENV;
    else if (strcmp(name, "MFILTER") == 0) *modifier = MFILTER;
    else if (strcmp(name, "MCONSTANT") == 0) *modifier = MCONSTANT;
    else return 0;
    return 1;
}

static const char* destinationToName(Destination destination) {
    switch (destination) {
        case DLFO: return "DLFO";
        case DENV: return "DENV";
        case DFILTER: return "DFILTER";
        case DROUTE: return "DROUTE";
        case DOSCILLATOR: return "DOSCILLATOR";
        case DVOLUME: return "DVOLUME";
        case DPANNER: return "DPANNER";
        case DPITCH: return "DPITCH";
    }
    return "DVOLUME";
}

static int nameToDestination(const char* name, Destination* destination) {
    if (strcmp(name, "DLFO") == 0) *destination = DLFO;
    else if (strcmp(name, "DENV") == 0) *destination = DENV;
    else if (strcmp(name, "DFILTER") == 0) *destination = DFILTER;
    else if (strcmp(name, "DROUTE") == 0) *destination = DROUTE;
    else if (strcmp(name, "DOSCILLATOR") == 0) *destination = DOSCILLATOR;
    else if (strcmp(name, "DVOLUME") == 0) *destination = DVOLUME;
    else if (strcmp(name, "DPANNER") == 0) *destination = DPANNER;
    else if (strcmp(name, "DPITCH") == 0) *destination = DPITCH;
    else return 0;
    return 1;
}

static const char* priorityToName(Priority priority) {
    return priority == VOICE ? "VOICE" : "OTHER";
}

static int nameToPriority(const char* name, Priority* priority) {
    if (strcmp(name, "VOICE") == 0) {
        *priority = VOICE;
        return 1;
    }
    if (strcmp(name, "OTHER") == 0) {
        *priority = OTHER;
        return 1;
    }
    return 0;
}

static const char* nodeTypeToName(NodeType type) {
    switch (type) {
        case SOSCILLATOR: return "SOSCILLATOR";
        case SFILTER: return "SFILTER";
        case SMIXER: return "SMIXER";
        case SPANNER: return "SPANNER";
    }
    return "SMIXER";
}

static int nameToNodeType(const char* name, NodeType* type) {
    if (strcmp(name, "SOSCILLATOR") == 0) *type = SOSCILLATOR;
    else if (strcmp(name, "SFILTER") == 0) *type = SFILTER;
    else if (strcmp(name, "SMIXER") == 0) *type = SMIXER;
    else if (strcmp(name, "SPANNER") == 0) *type = SPANNER;
    else return 0;
    return 1;
}

static int parseIndexedField(const char* key, const char* prefix, int* index, const char** field) {
    size_t prefixLen = strlen(prefix);
    if (strncmp(key, prefix, prefixLen) != 0) {
        return 0;
    }

    const char* cursor = key + prefixLen;
    if (!isdigit((unsigned char)*cursor)) {
        return 0;
    }

    *index = 0;
    while (isdigit((unsigned char)*cursor)) {
        *index = (*index * 10) + (*cursor - '0');
        cursor += 1;
    }

    if (*cursor != '.') {
        return 0;
    }

    *field = cursor + 1;
    return 1;
}

static int applyOscillatorSetting(Oscillator* oscillator, const char* field, const char* value) {
    float floatValue = 0.0f;

    if (strcmp(field, "wave") == 0) {
        float (*wave)(OscillatorState*, OscillatorParams*, float) = nameToWave(value);
        if (wave == NULL) {
            return 0;
        }
        oscillator->get = wave;
        return 1;
    }
    if (!parseFloat(value, &floatValue)) {
        return 0;
    }
    if (strcmp(field, "detune") == 0) oscillator->detune = floatValue;
    else if (strcmp(field, "phaseOffset") == 0) oscillator->phaseOffset = floatValue;
    else if (strcmp(field, "freq") == 0) oscillator->baseParams.freq = floatValue;
    else if (strcmp(field, "gain") == 0) oscillator->baseParams.gain = floatValue;
    else return 0;
    return 1;
}

static int applyEnvelopeSetting(ADSREnv* env, const char* field, const char* value) {
    float floatValue = 0.0f;
    int intValue = 0;

    if (strcmp(field, "active") == 0) {
        if (!parseInt(value, &intValue)) {
            return 0;
        }
        env->active = (int8_t)intValue;
        return 1;
    }
    if (!parseFloat(value, &floatValue)) {
        return 0;
    }
    if (strcmp(field, "attack") == 0) env->attack = floatValue;
    else if (strcmp(field, "decay") == 0) env->decay = floatValue;
    else if (strcmp(field, "sustain") == 0) env->sustain = floatValue;
    else if (strcmp(field, "release") == 0) env->release = floatValue;
    else return 0;
    return 1;
}

static int applyFilterSetting(LPF* filter, const char* field, const char* value) {
    float floatValue = 0.0f;
    int intValue = 0;

    if (strcmp(field, "poles") == 0) {
        if (!parseInt(value, &intValue)) {
            return 0;
        }
        filter->poles = (int8_t)intValue;
        return 1;
    }
    if (!parseFloat(value, &floatValue)) {
        return 0;
    }
    if (strcmp(field, "cutoff") == 0) filter->baseParams.cutoff = floatValue;
    else if (strcmp(field, "resonance") == 0) filter->baseParams.resonance = floatValue;
    else return 0;
    return 1;
}

static int applyPannerSetting(Panner* panner, const char* field, const char* value) {
    if (strcmp(field, "pan") != 0) {
        return 0;
    }
    return parseFloat(value, &panner->baseParams.pan);
}

static int applyRouteSetting(ModRoute* route, const char* field, const char* value) {
    int intValue = 0;

    if (strcmp(field, "mode") == 0) return nameToMode(value, &route->mode);
    if (strcmp(field, "modifier") == 0) return nameToModifier(value, &route->modifier);
    if (strcmp(field, "priority") == 0) return nameToPriority(value, &route->priority);
    if (strcmp(field, "destModifier") == 0) return nameToDestination(value, &route->destination.modifier);

    if (strcmp(field, "amount") == 0) {
        return parseFloat(value, &route->amount);
    }

    if (!parseInt(value, &intValue)) {
        return 0;
    }
    if (strcmp(field, "index") == 0) route->index = intValue;
    else if (strcmp(field, "destIndex") == 0) route->destination.index = intValue;
    else if (strcmp(field, "destField") == 0) route->destination.field = intValue;
    else return 0;
    return 1;
}

static int applyNodeSetting(NodePreset* node, const char* field, const char* value) {
    int intValue = 0;

    if (strcmp(field, "type") == 0) {
        return nameToNodeType(value, &node->type);
    }
    if (!parseInt(value, &intValue)) {
        return 0;
    }
    if (strcmp(field, "index") == 0) node->index = intValue;
    else if (strcmp(field, "numInputs") == 0) node->numInputs = (int8_t)intValue;
    else if (strncmp(field, "input", 5) == 0) {
        int inputIndex = 0;
        if (!parseInt(field + 5, &inputIndex)) {
            return 0;
        }
        node->inputIndexes[inputIndex] = (int8_t)intValue;
    } else {
        return 0;
    }
    return 1;
}

static int applyInstrumentSettingKV(Instrument* instrument, const char* key, const char* value) {
    int index = 0;
    const char* field = NULL;
    int intValue = 0;
    float floatValue = 0.0f;

    if (strcmp(key, "gain") == 0) {
        return parseFloat(value, &instrument->gain);
    }

    if (parseIndexedField(key, "osc", &index, &field)) {
        if (index >= MAX_OSCI || !applyOscillatorSetting(&instrument->oscs[index], field, value)) {
            return 0;
        }
        if (instrument->numOscs <= index) instrument->numOscs = (int8_t)(index + 1);
        return 1;
    }
    if (parseIndexedField(key, "env", &index, &field)) {
        if (index >= MAX_OTHER || !applyEnvelopeSetting(&instrument->envs[index], field, value)) {
            return 0;
        }
        if (instrument->numEnvs <= index) instrument->numEnvs = (int8_t)(index + 1);
        return 1;
    }
    if (parseIndexedField(key, "filter", &index, &field)) {
        if (index >= MAX_OTHER || !applyFilterSetting(&instrument->filters[index], field, value)) {
            return 0;
        }
        if (instrument->numFilters <= index) instrument->numFilters = (int8_t)(index + 1);
        return 1;
    }
    if (parseIndexedField(key, "lfo", &index, &field)) {
        if (index >= MAX_OTHER || !applyOscillatorSetting(&instrument->lfos[index], field, value)) {
            return 0;
        }
        if (instrument->numLFOS <= index) instrument->numLFOS = (int8_t)(index + 1);
        return 1;
    }
    if (parseIndexedField(key, "panner", &index, &field)) {
        if (index >= MAX_OTHER || !applyPannerSetting(&instrument->panners[index], field, value)) {
            return 0;
        }
        if (instrument->numPanners <= index) instrument->numPanners = (int8_t)(index + 1);
        return 1;
    }
    if (parseIndexedField(key, "route", &index, &field)) {
        if (index >= MAX_ROWS || !applyRouteSetting(&instrument->matrix[index], field, value)) {
            return 0;
        }
        if (instrument->routeNum <= index) instrument->routeNum = index + 1;
        return 1;
    }
    if (parseIndexedField(key, "node", &index, &field)) {
        if (index >= MAX_NODES || !applyNodeSetting(&instrument->signalChain[index], field, value)) {
            return 0;
        }
        if (instrument->nodeNum <= index) instrument->nodeNum = index + 1;
        return 1;
    }

    if (!parseInt(value, &intValue)) {
        return 0;
    }
    if (strcmp(key, "numOscs") == 0) instrument->numOscs = (int8_t)intValue;
    else if (strcmp(key, "numEnvs") == 0) instrument->numEnvs = (int8_t)intValue;
    else if (strcmp(key, "numFilters") == 0) instrument->numFilters = (int8_t)intValue;
    else if (strcmp(key, "numLFOS") == 0) instrument->numLFOS = (int8_t)intValue;
    else if (strcmp(key, "numPanners") == 0) instrument->numPanners = (int8_t)intValue;
    else if (strcmp(key, "routeNum") == 0) instrument->routeNum = intValue;
    else if (strcmp(key, "nodeNum") == 0) instrument->nodeNum = intValue;
    else {
        if (!parseFloat(value, &floatValue)) {
            return 0;
        }
        return 0;
    }
    return 1;
}

int applyInstrumentSetting(Instrument* instrument, const char* assignment) {
    char buffer[256];
    char* equals = NULL;

    if (strlen(assignment) >= sizeof(buffer)) {
        return 0;
    }
    strcpy(buffer, assignment);
    equals = strchr(buffer, '=');
    if (equals == NULL) {
        return 0;
    }

    *equals = '\0';
    trim(buffer);
    trim(equals + 1);
    return applyInstrumentSettingKV(instrument, buffer, equals + 1);
}

int saveInstrumentFile(const char* path, const Instrument* instrument, const char* name) {
    FILE* file = fopen(path, "w");
    if (file == NULL) {
        return 0;
    }

    fprintf(file, "format=audio-utils-preset-v1\n");
    fprintf(file, "name=%s\n", name != NULL ? name : "instrument");
    fprintf(file, "gain=%g\n", instrument->gain);
    fprintf(file, "numOscs=%d\n", instrument->numOscs);
    fprintf(file, "numEnvs=%d\n", instrument->numEnvs);
    fprintf(file, "numFilters=%d\n", instrument->numFilters);
    fprintf(file, "numLFOS=%d\n", instrument->numLFOS);
    fprintf(file, "numPanners=%d\n", instrument->numPanners);
    fprintf(file, "routeNum=%d\n", instrument->routeNum);
    fprintf(file, "nodeNum=%d\n", instrument->nodeNum);

    for (int i = 0; i < instrument->numOscs; i++) {
        fprintf(file, "osc%d.wave=%s\n", i, waveToName(instrument->oscs[i].get));
        fprintf(file, "osc%d.detune=%g\n", i, instrument->oscs[i].detune);
        fprintf(file, "osc%d.phaseOffset=%g\n", i, instrument->oscs[i].phaseOffset);
        fprintf(file, "osc%d.freq=%g\n", i, instrument->oscs[i].baseParams.freq);
        fprintf(file, "osc%d.gain=%g\n", i, instrument->oscs[i].baseParams.gain);
    }
    for (int i = 0; i < instrument->numEnvs; i++) {
        fprintf(file, "env%d.attack=%g\n", i, instrument->envs[i].attack);
        fprintf(file, "env%d.decay=%g\n", i, instrument->envs[i].decay);
        fprintf(file, "env%d.sustain=%g\n", i, instrument->envs[i].sustain);
        fprintf(file, "env%d.release=%g\n", i, instrument->envs[i].release);
        fprintf(file, "env%d.active=%d\n", i, instrument->envs[i].active);
    }
    for (int i = 0; i < instrument->numFilters; i++) {
        fprintf(file, "filter%d.poles=%d\n", i, instrument->filters[i].poles);
        fprintf(file, "filter%d.cutoff=%g\n", i, instrument->filters[i].baseParams.cutoff);
        fprintf(file, "filter%d.resonance=%g\n", i, instrument->filters[i].baseParams.resonance);
    }
    for (int i = 0; i < instrument->numLFOS; i++) {
        fprintf(file, "lfo%d.wave=%s\n", i, waveToName(instrument->lfos[i].get));
        fprintf(file, "lfo%d.detune=%g\n", i, instrument->lfos[i].detune);
        fprintf(file, "lfo%d.phaseOffset=%g\n", i, instrument->lfos[i].phaseOffset);
        fprintf(file, "lfo%d.freq=%g\n", i, instrument->lfos[i].baseParams.freq);
        fprintf(file, "lfo%d.gain=%g\n", i, instrument->lfos[i].baseParams.gain);
    }
    for (int i = 0; i < instrument->numPanners; i++) {
        fprintf(file, "panner%d.pan=%g\n", i, instrument->panners[i].baseParams.pan);
    }
    for (int i = 0; i < instrument->routeNum; i++) {
        fprintf(file, "route%d.amount=%g\n", i, instrument->matrix[i].amount);
        fprintf(file, "route%d.mode=%s\n", i, modeToName(instrument->matrix[i].mode));
        fprintf(file, "route%d.modifier=%s\n", i, modifierToName(instrument->matrix[i].modifier));
        fprintf(file, "route%d.index=%d\n", i, instrument->matrix[i].index);
        fprintf(file, "route%d.priority=%s\n", i, priorityToName(instrument->matrix[i].priority));
        fprintf(file, "route%d.destModifier=%s\n", i, destinationToName(instrument->matrix[i].destination.modifier));
        fprintf(file, "route%d.destIndex=%d\n", i, instrument->matrix[i].destination.index);
        fprintf(file, "route%d.destField=%d\n", i, instrument->matrix[i].destination.field);
    }
    for (int i = 0; i < instrument->nodeNum; i++) {
        fprintf(file, "node%d.type=%s\n", i, nodeTypeToName(instrument->signalChain[i].type));
        fprintf(file, "node%d.index=%d\n", i, instrument->signalChain[i].index);
        fprintf(file, "node%d.numInputs=%d\n", i, instrument->signalChain[i].numInputs);
        for (int input = 0; input < instrument->signalChain[i].numInputs; input++) {
            fprintf(file, "node%d.input%d=%d\n", i, input, instrument->signalChain[i].inputIndexes[input]);
        }
    }

    fclose(file);
    return 1;
}

int loadInstrumentFile(const char* path, Instrument* instrument, char* nameBuffer, size_t nameBufferSize) {
    FILE* file = fopen(path, "r");
    char line[256];

    if (file == NULL) {
        return 0;
    }

    *instrument = (Instrument){0};
    if (nameBuffer != NULL && nameBufferSize > 0) {
        nameBuffer[0] = '\0';
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        char* equals = strchr(line, '=');
        if (equals == NULL) {
            continue;
        }

        *equals = '\0';
        trim(line);
        trim(equals + 1);

        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        if (strcmp(line, "format") == 0) {
            continue;
        }
        if (strcmp(line, "name") == 0) {
            if (nameBuffer != NULL && nameBufferSize > 0) {
                snprintf(nameBuffer, nameBufferSize, "%s", equals + 1);
            }
            continue;
        }
        if (!applyInstrumentSettingKV(instrument, line, equals + 1)) {
            fclose(file);
            return 0;
        }
    }

    fclose(file);
    return 1;
}
