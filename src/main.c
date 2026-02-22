#include <stdlib.h>
#define HEADER_SIZE 44
#define BLOCK_SIZE 512
#define MAX_VOICES 16
#define MAX_OSCI 8

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../headers/oscillator.h"
#include "../headers/utils.h"
#include "../headers/adsr.h"
#include "../headers/lpf.h"
#include "../headers/voice.h"
#include "../headers/modroutes.h"


typedef struct WavHeader WavHeader;

typedef struct WavFile WavFile;

float midiToFreq(float num);
void voiceOn(Voice* voice, Instrument* instr, int midiNote, float velocity, float volume, float sampleRate);
void voiceOff(Voice* voice);
void fillBlockVoice(Signal* block, Voice* voice, int fillamt);
void writeBlock(WavFile* file, Signal* block, int amount);
int timeToSample(float time, float sr);
float sampleToTime(int sample, float sr);
Voice initializeVoice();
Voice* findFreeVoice(Synth* synth);
Voice* getVoice(Synth* synth);
float bpm;
int numChannels;

uint64_t sampleCount = 0;
//Wav header struct
struct WavHeader{
    char     riff[4];        // "RIFF"
    uint32_t file_size;      // file size - 8
    char     wave[4];        // "WAVE"

    char     fmt[4];         // "fmt "
    uint32_t fmt_size;       // 16
    uint16_t audio_format;   // 1 = PCM
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;

    char     data[4];        // "data"
    uint32_t data_size;
};

struct WavFile{
    WavHeader* header;
    FILE *fp;
    uint32_t bytesWritten;
};










void fillBlockVoice(Signal* block, Voice* voice, int fillamt) {
    if (fillamt >= BLOCK_SIZE) fillamt = BLOCK_SIZE;
    if (!voice->active) {
        return;
    };
    
    for (int i = 0; i < fillamt; i++) {
        block[i] = addSignals(voiceNext(voice), block[i]);
        if (fabs(block[i].l) < 1e-15f) block[i].l = 0.0f;
        if (fabs(block[i].r) < 1e-15f) block[i].r = 0.0f;

    }
}


void writeBlock(WavFile* file, Signal* block, int amount) {
    if (amount > BLOCK_SIZE) amount = BLOCK_SIZE;
    int16_t pcm[BLOCK_SIZE*numChannels];
    for (int i = 0; i < amount*numChannels; i+=numChannels) {
        
        Signal sample = block[i];
        if (sample.l < -1) {
            //printf("%f\n", sample.l);
        }
        if (sample.l > 1.0)  sample.l = 1.0;
        if (sample.l < -1.0) sample.l = -1.0;
        if (sample.r > 1.0)  sample.r = 1.0;
        if (sample.r < -1.0) sample.r = -1.0;
        if (numChannels == 1) {
            pcm[i] = (int16_t)((sample.l + sample.r)/2.0 * 32767.0f);
        }
        if (numChannels == 2) {
            pcm[i] = (int16_t)(sample.l * 32767.0f);
            pcm[i+1] = (int16_t)(sample.r * 32767.0f);
        }
        //printf("%f\n", block[i]);
    }
    fwrite(pcm, sizeof(int16_t), amount, file->fp);
    file->bytesWritten += amount * sizeof(int16_t);
    return;
}
int timeToSample(float time, float sr) {
    return time * sr * numChannels;
}
float sampleToTime(int sample, float sr) {
    return (float)(sample / (float)sr);
}
Voice initializeVoice() {
    Voice ret;
    ret.active = 0;
    return ret;

}
Voice* findFreeVoice(Synth* synth) {
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!synth->voices[i].active || synth->voices[i].params.volume <= 0.01) {
            //printf("Voice: %d\n", i);
            return &synth->voices[i];
        }
    }
    return NULL;
}
Voice* getVoice(Synth* synth) {
    Voice* v = findFreeVoice(synth);
    if (v) return v;
    //TODO: fix this shit
    return &synth->voices[0];
}
void generateAudio(
    Synth* synth, float time, Instrument* instrument,int note, 
    float velocity, float volume, WavFile* file
) {
    Voice* v = getVoice(synth);
    //printf("The address of num is: %p\n", (void*)v);
    //printf("The address of 0 is: %p\n", (void*)&synth->voices[0]);
    voiceOn(v, instrument, note, velocity, volume, synth->sampleRate);
    
    int64_t totSamples = timeToSample(time*60/bpm, synth->sampleRate);
    for (int64_t i = 0; i < totSamples; i+= BLOCK_SIZE) {
        Signal buffer[BLOCK_SIZE] = {0};
        int fillamt = totSamples - i;
        if (fillamt > BLOCK_SIZE) fillamt = BLOCK_SIZE;

        for (int j = 0; j < MAX_VOICES; j++) {
            //printf("%d\n", j);
            fillBlockVoice(buffer, &synth->voices[j], fillamt);
            
        }


        writeBlock(file, buffer, fillamt);
    }
    voiceOff(v);
}


int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    srand(67);
    FILE* fp;
    fp = fopen("audio6.wav", "wb+");
    if (fp == NULL) {
        printf("Error opening file for writing.\n");
        return 1;
    }
    
    float samplerat = 44100;

    numChannels = 1;
    

    WavHeader h = {0};
    WavFile file = {&h, fp, 0};
    memcpy(h.riff, "RIFF", 4);
    memcpy(h.wave, "WAVE", 4);
    memcpy(h.fmt,  "fmt ", 4);
    memcpy(h.data, "data", 4);

    h.fmt_size        = 16;
    h.audio_format    = 1;
    h.num_channels    = numChannels;
    h.sample_rate     = (int)samplerat;
    h.bits_per_sample = 16;

    h.block_align = h.num_channels * (h.bits_per_sample / 8);
    h.byte_rate   = h.sample_rate * h.block_align;


    h.data_size = 0;
    h.file_size = 0;

    fwrite(&h, sizeof(h), 1, fp);
    Synth synth;
    synth.sampleRate = samplerat;
    
    for (int i = 0; i < MAX_VOICES; i++) {
        synth.voices[i] = initializeVoice();
    }


    //BEGIN ENTERING OF AUDIO DATA
    ///*
    
    Instrument warmBass = {};
    Instrument lead = {};
    /*
    Oscillator osci = {sawWave, -2.0, 0.0, {0,0,0.6}};
    Oscillator osci2 = {squareWave, -1205.0, 0.0, {0,0,0.3}};
    Oscillator osci3 = {sineWave, 700.0, 0.0, {0,0,0.1}};
    Oscillator lfo = {sineWave, 0.0, 0.0, {3.0, 5.5, 0.5}};
    simplesine.numOscs = 1;

    simplesine.oscs[0]=osci;
    simplesine.oscs[1]=osci2;
    simplesine.oscs[2] = osci3;
    ADSREnv env2 = {0.1, 0.55, 0.1, 0.1, 1};
    ADSREnv env = {0.1, 0.2, 0.85, 0.1, 1};
    simplesine.envs[0] = env;
    simplesine.envs[1] = env2;
    simplesine.numEnvs = 2;
    
    simplesine.lfos[0] = lfo;
    simplesine.numLFOS = 1;
    
    LPF lpf = {4, {1500.0, 0.25}};
    
    
    simplesine.filters[0] = lpf;
    simplesine.numFilters = 1;
    simplesine.gain = 3.0;
    setDefaultModRoutes(&simplesine);
    addModRoute(&simplesine, ADD, ENV, 1, 1500.0, (ModDest){FILTER, 1, 0});
    addModRoute(&simplesine, ASSIGN, FILTER, 0, 1.0, (ModDest){CONSTANT, 0, 0});
    addModRoute(&simplesine, MULT, ENV, 0, 1.0, (ModDest){VOLUME, 0, 0});
    addModRoute(&simplesine, MULTADD, LFO, 0, SEMITONE-1, (ModDest){OSCILLATOR, 0, 0});
    addModRoute(&simplesine, MULTADD, LFO, 0, SEMITONE-1, (ModDest){OSCILLATOR, 1, 0});
    addModRoute(&simplesine, MULTADD, LFO, 0, SEMITONE-1, (ModDest){OSCILLATOR, 2, 0});
    */

    // WARM BASS PRESET
    Oscillator Bosci = {sawWave, 0.0, 0.0, {{0,0,0.5}}};
    Oscillator Bosci2 = {sawWave, -5.0, 0.0, {{0,0,0.5}}};
    
    warmBass.numOscs = 2;

    warmBass.oscs[0]=Bosci;
    warmBass.oscs[1]=Bosci2;
    ADSREnv Benv2 = {0.002, 0.2, 0.0, 0.15, 1};
    ADSREnv Benv = {0.005, 0.15, 0.1, 0.1, 1};
    warmBass.envs[0] = Benv;
    warmBass.envs[1] = Benv2;
    warmBass.numEnvs = 2;

    
    LPF Blpf = {4, {{800.0, 0.2}}};
    
    
    warmBass.filters[0] = Blpf;
    warmBass.numFilters = 1;
    warmBass.gain = 1.0;
    setDefaultModRoutes(&warmBass);
    addModRoute(&warmBass, ASSIGN, MFILTER, 0, 1.0, (ModDest){DCONSTANT, 0, 0});
    addModRoute(&warmBass, ADD, MENV, 1, 2000.0, (ModDest){DFILTER, 1, 0});
    addModRoute(&warmBass, MULT, MENV, 0, 1.0, (ModDest){DVOLUME, 0, 0});



    //LEAD PRESET
    Oscillator Losci = {polyblepSaw, 0.0, 0.1, {{0,0,0.2}}};
    Oscillator Losci2 = {polyblepSaw, -5.0, 0.03, {{0,0,0.2}}};
    Oscillator Losci3 = {polyblepSaw, -2.0, 0.35, {{0,0,0.2}}};
    Oscillator Losci4 = {polyblepSaw, 2.0, 0.8, {{0,0,0.2}}};
    Oscillator Losci5 = {polyblepSaw, 5.0, 0.62, {{0,0,0.2}}};
    Oscillator Losci6 = {noise, 12.0, 0.62, {{0,0,0.02}}};

    Oscillator Llfo = {sineWave, 0.0, 0.0, {{5.0, 5.0, 0.1}}};
    lead.numOscs = 6;

    lead.oscs[0]=Losci;
    lead.oscs[1]=Losci2;
    lead.oscs[2]=Losci3;
    lead.oscs[3]=Losci4;
    lead.oscs[4]=Losci5;
    lead.oscs[5]=Losci6;

    ADSREnv Lenv = {0.01, 0.05, 0.7, 0.1, 1};
    ADSREnv Lenv2 = {0.005, 0.2, 0.2, 0.15, 1};
    lead.envs[0] = Lenv;
    lead.envs[1] = Lenv2;
    lead.numEnvs = 2;
    
    lead.lfos[0] = Llfo;
    lead.numLFOS = 1;
    
    LPF Llpf = {4, {{2500.0, 0.2}}};
    
    
    lead.filters[0] = Llpf;
    lead.numFilters = 1;
    lead.gain = 1;
    setDefaultModRoutes(&lead);
    addModRoute(&lead, ADD, MENV, 1, 3500.0, (ModDest){DFILTER, 1, 0});
    //addModRoute(&lead, ASSIGN, MFILTER, 0, 1.0, (ModDest){DCONSTANT, 0, 0});
    addModRoute(&lead, MULT, MENV, 0, 1.0, (ModDest){DVOLUME, 0, 0});
    addModRoute(&lead, MULTADD, MLFO, 0, SEMITONE-1, (ModDest){DOSCILLATOR, 0, 0});
    addModRoute(&lead, MULTADD, MLFO, 0, SEMITONE-1, (ModDest){DOSCILLATOR, 1, 0});
    addModRoute(&lead, MULTADD, MLFO, 0, SEMITONE-1, (ModDest){DOSCILLATOR, 2, 0});
    addModRoute(&lead, MULTADD, MLFO, 0, SEMITONE-1, (ModDest){DOSCILLATOR, 3, 0});
    addModRoute(&lead, MULTADD, MLFO, 0, SEMITONE-1, (ModDest){DOSCILLATOR, 4, 0});
    addSignalNode(&lead, SOSCILLATOR, 0);
    addSignalNode(&lead, SOSCILLATOR, 1);
    addSignalNode(&lead, SOSCILLATOR, 2);
    addSignalNode(&lead, SOSCILLATOR, 3);
    addSignalNode(&lead, SOSCILLATOR, 4);
    addSignalNode(&lead, SOSCILLATOR, 5);
    addSignalNode(&lead, SMIXER, 0);
    addSignalNodeInput(&lead, 6, 0);
    addSignalNodeInput(&lead, 6, 1);
    addSignalNodeInput(&lead, 6, 2);
    addSignalNodeInput(&lead, 6, 3);
    addSignalNodeInput(&lead, 6, 4);
    addSignalNodeInput(&lead, 6, 5);
    addSignalNode(&lead, SFILTER, 0);
    addSignalNodeInput(&lead, 7, 6);

    bpm = 125;
    

    Instrument simplesine = lead;

    /*
    generateAudio(&synth, 0.25, &simplesine, 55, 1, 1.5, &file);
    generateAudio(&synth, 0.25, &simplesine, 67, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 55, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 67, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 50, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 55, 1, 1.5, &file);
    generateAudio(&synth, 0.25, &simplesine, 67, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 55, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 67, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 50, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 55, 1, 1.5, &file);
    generateAudio(&synth, 0.25, &simplesine, 67, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 55, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 67, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 50, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 55, 1, 1.5, &file);
    generateAudio(&synth, 0.25, &simplesine, 67, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 58, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 55, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 60, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 53, 1, 1, &file);

    generateAudio(&synth, 0.25, &simplesine, 55, 1, 1.5, &file);
    generateAudio(&synth, 0.25, &simplesine, 67, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 55, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 67, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 50, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 55, 1, 1.5, &file);
    generateAudio(&synth, 0.25, &simplesine, 67, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 55, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 67, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 50, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 55, 1, 1.5, &file);
    generateAudio(&synth, 0.25, &simplesine, 67, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 55, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 67, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 50, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 55, 1, 1.5, &file);
    generateAudio(&synth, 0.25, &simplesine, 67, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 58, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 55, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 60, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 53, 1, 1, &file);

    generateAudio(&synth, 0.25, &simplesine, 51, 1, 1.5, &file);
    generateAudio(&synth, 0.25, &simplesine, 63, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 58, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 51, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 63, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 58, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 50, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 51, 1, 1.5, &file);
    generateAudio(&synth, 0.25, &simplesine, 63, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 58, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 51, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 63, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 58, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 50, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 51, 1, 1.5, &file);
    generateAudio(&synth, 0.25, &simplesine, 63, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 58, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 51, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 63, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 58, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 50, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 51, 1, 1.5, &file);
    generateAudio(&synth, 0.25, &simplesine, 64, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 58, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 51, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 53, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 60, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 53, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 51, 1, 1.5, &file);
    generateAudio(&synth, 0.25, &simplesine, 63, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 58, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 51, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 63, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 58, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 50, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 51, 1, 1.5, &file);
    generateAudio(&synth, 0.25, &simplesine, 63, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 58, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 51, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 63, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 58, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 50, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 51, 1, 1.5, &file);
    generateAudio(&synth, 0.25, &simplesine, 63, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 58, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 51, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 63, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 58, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 50, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 51, 1, 1.5, &file);
    generateAudio(&synth, 0.25, &simplesine, 63, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 58, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 51, 1, 1, &file);
    */
   //END BASS

    //MELODY
    //*
    generateAudio(&synth, 0.5, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 1, &simplesine, 62, 0, 0, &file);
    generateAudio(&synth, 0.5, &simplesine, 60, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 1, &simplesine, 62, 0, 0, &file);
    generateAudio(&synth, 0.5, &simplesine, 60, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 1, &simplesine, 62, 0, 0, &file);
    generateAudio(&synth, 0.5, &simplesine, 60, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 0.125, &simplesine, 60, 1, 1, &file);
    generateAudio(&synth, 0.125, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 60, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 58, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 60, 1, 1, &file);

    generateAudio(&synth, 0.5, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 1, &simplesine, 62, 0, 0, &file);
    generateAudio(&synth, 0.5, &simplesine, 60, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 1, &simplesine, 62, 0, 0, &file);
    generateAudio(&synth, 0.5, &simplesine, 60, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 1, &simplesine, 62, 0, 0, &file);
    generateAudio(&synth, 0.5, &simplesine, 60, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 0.125, &simplesine, 60, 1, 1, &file);
    generateAudio(&synth, 0.125, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 60, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 58, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 60, 1, 1, &file);

    generateAudio(&synth, 0.5, &simplesine, 65, 1, 1, &file);
    generateAudio(&synth, 1, &simplesine, 69, 0, 0, &file);
    generateAudio(&synth, 0.5, &simplesine, 69, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 65, 1, 1, &file);
    generateAudio(&synth, 1, &simplesine, 62, 0, 0, &file);
    generateAudio(&synth, 0.5, &simplesine, 69, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 65, 1, 1, &file);
    generateAudio(&synth, 1, &simplesine, 62, 0, 0, &file);
    generateAudio(&synth, 0.5, &simplesine, 69, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 65, 1, 1, &file);
    generateAudio(&synth, 0.125, &simplesine, 60, 1, 1, &file);
    generateAudio(&synth, 0.125, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 60, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 58, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 60, 1, 1, &file);
    
    generateAudio(&synth, 0.5, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 1, &simplesine, 62, 0, 0, &file);
    generateAudio(&synth, 0.5, &simplesine, 60, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 1, &simplesine, 62, 0, 0, &file);
    generateAudio(&synth, 0.5, &simplesine, 60, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 1, &simplesine, 62, 0, 0, &file);
    generateAudio(&synth, 0.5, &simplesine, 60, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 0.125, &simplesine, 60, 1, 1, &file);
    generateAudio(&synth, 0.125, &simplesine, 62, 1, 1, &file);
    generateAudio(&synth, 0.25, &simplesine, 60, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 58, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 60, 1, 1, &file);
    //*/
    // END MELODY



    /*
    
    
    generateAudio(&synth, 1, &simplesine, 74, 1, 1, &file);
    generateAudio(&synth, 1, &simplesine, 75, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 77, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 77, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 77, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 77, 1, 1, &file);
    generateAudio(&synth, 1, &simplesine, 77, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 70, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 74, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 72, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 72, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 72, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 72, 1, 1, &file);
    generateAudio(&synth, 1, &simplesine, 72, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 74, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 72, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 70, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 70, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 70, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 70, 1, 1, &file);
    generateAudio(&synth, 1, &simplesine, 70, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 81, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 82, 1, 1, &file);
    generateAudio(&synth, 1.5, &simplesine, 81, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 77, 1, 1, &file);
    generateAudio(&synth, 1, &simplesine, 74, 1, 1, &file);
    
    generateAudio(&synth, 0.5, &simplesine, 74, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 75, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 77, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 77, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 77, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 77, 1, 1, &file);
    generateAudio(&synth, 1, &simplesine, 77, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 70, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 74, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 72, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 72, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 72, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 72, 1, 1, &file);
    generateAudio(&synth, 1, &simplesine, 72, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 74, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 72, 1, 1, &file);
    generateAudio(&synth, 1.5, &simplesine, 70, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 65, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 74, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 75, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 72, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesine, 70, 1, 1, &file);
    generateAudio(&synth, 1, &simplesine, 70, 1, 1, &file);
    */
    

    //END ENTERING OF AUDIO DATA
    int datasize = file.bytesWritten;
    int filesize = datasize + 36;

    fseek(fp, 4, SEEK_SET);
    fwrite(&filesize, 4, 1, fp);

    fseek(fp, 40, SEEK_SET);
    fwrite(&datasize, 4, 1, fp);
    
    

}