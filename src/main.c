
#include <stdio.h>

#include <string.h>

#include "../headers/oscillator.h"
#include "../headers/all.h"
#include "../headers/adsr.h"
#include "../headers/lpf.h"
#include "../headers/voice.h"
#include "../headers/modroutes.h"

#define HEADER_SIZE 44
#define BLOCK_SIZE 512
#define MAX_VOICES 16
#define MAX_OSCI 8
typedef struct WavHeader WavHeader;

typedef struct WavFile WavFile;

float midiToFreq(float num);
float voiceNext(Voice* voice);
void voiceOn(Voice* voice, Instrument* instr, int midiNote, float velocity, float volume, float sampleRate);
void voiceOff(Voice* voice);
void fillBlockVoice(float* block, Voice* voice, int fillamt);
void writeBlock(WavFile* file, float* block, int amount);
int timeToSample(float time, float sr);
float sampleToTime(int sample, float sr);
Voice initializeVoice();
Voice* findFreeVoice(Synth* synth);
Voice* getVoice(Synth* synth);



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











void fillBlockVoice(float* block, Voice* voice, int fillamt) {
    if (fillamt >= BLOCK_SIZE) fillamt = BLOCK_SIZE;
    if (!voice->active) {
        return;
    };
    
    for (int i = 0; i < fillamt; i++) {
        block[i] += voiceNext(voice);
        if (fabs(block[i]) < 1e-15f) block[i] = 0.0f;

    }
}


void writeBlock(WavFile* file, float* block, int amount) {
    if (amount > BLOCK_SIZE) amount = BLOCK_SIZE;
    int16_t pcm[BLOCK_SIZE];
    for (int i = 0; i < amount; i++) {
        
        float sample = block[i];
        if (sample > 1) {
            printf("%f\n", sample);
        }
        if (sample > 1.0)  sample = 1.0;
        if (sample < -1.0) sample = -1.0;
        pcm[i] = (int16_t)(sample * 32767.0f);
        //printf("%f\n", block[i]);
    }
    fwrite(pcm, sizeof(int16_t), amount, file->fp);
    file->bytesWritten += amount * sizeof(int16_t);
    return;
}
int timeToSample(float time, float sr) {
    return time * sr;
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
        if (!synth->voices[i].active) {
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
    int64_t totSamples = timeToSample(time, synth->sampleRate);
    for (int64_t i = 0; i < totSamples; i+= BLOCK_SIZE) {
        float buffer[BLOCK_SIZE] = {0};
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
    FILE* fp;
    fp = fopen("audio3.wav", "wb+");
    if (fp == NULL) {
        printf("Error opening file for writing.\n");
        return 1;
    }
    
    float samplerat = 44100;

    
    

    WavHeader h = {0};
    WavFile file = {&h, fp, 0};
    memcpy(h.riff, "RIFF", 4);
    memcpy(h.wave, "WAVE", 4);
    memcpy(h.fmt,  "fmt ", 4);
    memcpy(h.data, "data", 4);

    h.fmt_size        = 16;
    h.audio_format    = 1;
    h.num_channels    = 1;
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
    /*
    Instrument simplesine = {};
    Instrument simplesaw = {};

    Oscillator osci = {sawWave, -2.0, 0.0, 0.6};
    Oscillator osci2 = {squareWave, -1205.0, 0.0, 0.3};
    Oscillator osci3 = {sineWave, 700.0, 0.0, 0.1};
    simplesine.numOscs = 3;

    simplesine.oscs[0]=osci;
    simplesine.oscs[1]=osci2;
    simplesine.oscs[2] = osci3;
    ADSREnv env = {0.1, 0.35, 0.1, 0.2, 1, 1};
    ADSREnv env2 = {0.1, 0.2, 0.85, 0.2, 1, 1};
    simplesine.envs[0] = env;
    simplesine.envs[1] = env2;
    simplesine.numEnvs = 2;
    VoiceParams param = {1.0};
    ModRoute audioEnv = {adsrNext, 1.0, &env2, initializeEnv(&env2, samplerat), MULT};
    ModRow audioRow = {{audioEnv}, 1, &simplesine.gain};
    
    LPF lpf = {4, {1500.0, 0.15}};
    ModRoute filterEnv = {adsrNext, 1500.0, &env2, initializeEnv(&env2, samplerat), MULT};
    
    simplesine.lpf = lpf;
    simplesaw.lpf = lpf;
    simplesine.gain = 1.0;
    simplesaw.gain = 1.0;
    simplesaw = simplesine;

    ///*
    generateAudio(&synth, 1, &simplesine, 74, 0, 0, &file);
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
    generateAudio(&synth, 0.5, &simplesaw, 74, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesaw, 72, 1, 1, &file);
    generateAudio(&synth, 1.5, &simplesaw, 70, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesaw, 65, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesaw, 74, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesaw, 75, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesaw, 72, 1, 1, &file);
    generateAudio(&synth, 0.5, &simplesaw, 70, 1, 1, &file);
    generateAudio(&synth, 1, &simplesaw, 70, 1, 1, &file);
    //*/
    

    //END ENTERING OF AUDIO DATA
    int datasize = file.bytesWritten;
    int filesize = datasize + 36;

    fseek(fp, 4, SEEK_SET);
    fwrite(&filesize, 4, 1, fp);

    fseek(fp, 40, SEEK_SET);
    fwrite(&datasize, 4, 1, fp);
    
    

}