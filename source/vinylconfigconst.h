#pragma once
#define EFFTFrame 512
#define ESpeedFrame 128
#define EFilterFrame 80
#define ETimeCodeCoeff 22.9
//#define ETimeCodeCoeff 45.48
#define ETimeCodeMinAmplytude 0.009
#define EMaximumSamples 128
#define EMaximumScenes 10
#define ENumberOfPads 16
#define EEmptyBaseTitle "Empty"
#define ETimecodeLearnCount 1024
#define EDefaultTempo 120
#define EDefaultSampleRate 44100
#define ERollNote 1.0/32.0
#define ERollCount 8
#define ESoftEffectSamples 24
//#define EHoldSamples 2205

//////initial MIDIControls config
#define gGain 0x07
#define gScene 0x15
#define gMix 0x16
#define gPitch 0x17
#define gVolume 0x14
#define gTune 0x18
const int gPad[ENumberOfPads] ={0x3d,0x45,0x41,0x3f,
                                0x3c,0x3b,0x39,0x37,
                                0x31,0x33,0x44,0x38,
                                0x30,0x34,0x36,0x3a};


