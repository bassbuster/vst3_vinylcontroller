#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "pluginterfaces/vst/ivstevents.h"

#include "helpers/sampleentry.h"
#include "helpers/parameterreader.h"
#include "helpers/padentry.h"
#include "helpers/ringbuffer.h"
#include "helpers/filtred.h"

#include "vinylconfigconst.h"

namespace Steinberg {
namespace Vst {

//------------------------------------------------------------------------
// AVinyl: directly derived from the helper class AudioEffect
//------------------------------------------------------------------------
class AVinyl: public AudioEffect
{
public:
	AVinyl ();
	virtual ~AVinyl ();	// dont forget virtual here

    //------------------------------------------------------------------------
    // create function required for plug-in factory,
    // it will be called to create new instances of this plug-in
    //------------------------------------------------------------------------
	static FUnknown* createInstance (void* context)
	{
		return (IAudioProcessor*)new AVinyl;
	}

    //------------------------------------------------------------------------
    // AudioEffect overrides:
    //------------------------------------------------------------------------
	/** Called at first after constructor */
    tresult PLUGIN_API initialize(FUnknown* context) override;
	
	/** Called at the end before destructor */
    tresult PLUGIN_API terminate() override;
	
	/** Switch the plug-in on/off */
    tresult PLUGIN_API setActive(TBool state) override;

	/** Here we go...the process call */
    tresult PLUGIN_API process(ProcessData& data) override;

	/** Test of a communication channel between controller and component */
    tresult receiveText(const char* text) override;
    tresult PLUGIN_API notify(IMessage* message) override;
	
	/** For persistence */
    tresult PLUGIN_API setState(IBStream* state) override;
    tresult PLUGIN_API getState(IBStream* state) override;

	/** Will be called before any process call */
    tresult PLUGIN_API setupProcessing(ProcessSetup& newSetup) override;

	/** Bus arrangement managing: in this example the 'again' will be mono for mono input/output and stereo for other arrangements. */
    tresult PLUGIN_API setBusArrangements(SpeakerArrangement* inputs, int32 numIns, SpeakerArrangement* outputs, int32 numOuts) override;

    tresult PLUGIN_API setProcessing(TBool state) override;
    tresult PLUGIN_API canProcessSampleSize(int32 symbolicSampleSize) override;
    uint32 PLUGIN_API getLatencySamples() override;
    uint32 PLUGIN_API getTailSamples() override;

private:

    void calcDirectionTimeCodeAmplitude();
    void calcAbsSpeed();
    void currentEntry(int64_t newentry);

    bool padWork(int padId, double paramValue);
    bool padSet(int currentSample);
    void padRemove(int currentSample);

    // SampleBase manipulation
    void addSampleMessage(SampleEntry<Sample64>* newSample);
    void delSampleMessage(SampleEntry<Sample64>* delSample);
    void initSamplesMessage(void);
    void updateSpeedMessage(Sample64 speed);
    void updatePositionMessage(Sample64 speed);
    void updatePadsMessage(void);

    void debugFftMessage(Sample64 *fft, size_t len);
    void debugInputMessage(Sample64 *input, size_t len);

    void processEvent(const Event &event);
    void reset(bool state);

    RingBuffer<Sample64, EFilterFrame> filterBufferLeft_;
    RingBuffer<Sample64, EFilterFrame> filterBufferRight_;

    Filtred<Sample64, EFilterFrame> filtredLoLeft_;
    Filtred<Sample64, EFilterFrame> filtredLoRight_;

    Sample64 originalBuffer_[EFFTFrame];
    Sample64 fftBuffer_[EFFTFrame];

    size_t speedFrameIndex_;

    Filtred<Sample64, 10> absAvgSpeed_;
    Filtred<Sample64> deltaLeft_;
    Filtred<Sample64> deltaRight_;
    Sample64 oldSignalLeft_;
    Sample64 oldSignalRight_;
    Filtred<Sample64> filtredHiLeft_;
    Filtred<Sample64> filtredHiRight_;
    Filtred<Sample64, 64> timeCodeAmplytude_;
    size_t speedCounter_;
    bool statusR_;
    bool statusL_;
    bool oldStatusR_;
    bool oldStatusL_;
    bool pStatusR_;
    bool pStatusL_;
    bool pOldStatusR_;
    bool pOldStatusL_;

    uint32_t directionBits_;
    Sample64 direction_;

    int32 effectorSet_;

    // SoftCurves
    Filtred<Sample32, ESoftEffectSamples> softVolume_;
    Filtred<Sample32, ESoftEffectSamples> softPreRoll_;
    Filtred<Sample32, ESoftEffectSamples> softPostRoll_;
    Filtred<Sample32, ESoftEffectSamples> softDistorsion_;
    Filtred<Sample32, ESoftEffectSamples> softHold_;
    Filtred<Sample32, ESoftEffectSamples> softFreeze_;
    Filtred<Sample32, ESoftEffectSamples> softVintage_;

    Sample64 lockSpeed_;
    Sample64 lockVolume_;
    Sample64 lockTune_;

    bool keyHold_;
    bool keyFreeze_;
    bool keyLockTone_;

	// our model values
    Sample32 vuLeft_;
    Sample32 vuRight_;
    Sample32 position_;
    Sample32 gain_;          //0..+1
    Sample32 volume_;        //0..+1
    Sample32 pitch_;         //0..+1
    uint32_t currentEntry_;  //0..MaximumSamples - 1
    uint32_t currentScene_;  //0..MaximumScenes - 1
    Sample32 switch_;        //0..+1
    Sample32 curve_;         //0..+1
    bool bypass_;
    bool timeCodeLearn_;

    std::vector<std::unique_ptr<SampleEntry<Sample64>>> samplesArray_;
    std::unique_ptr<SampleEntry<Sample64>> vintageSample_;

    PadEntry padStates_[EMaximumScenes][ENumberOfPads];

    int32 currentProcessMode_;
    bool currentProcessStatus_;

    bool dirtyParams_;

    double sampleRate_;
    double tempo_;
    size_t noteLength_;

    Sample64 realPitch_;
    Sample64 realSpeed_;
    Sample64 realVolume_;
    Filtred<Sample64, 16> volCoeff_;
    Filtred<Sample64, 256> avgTimeCodeCoeff_;
    size_t timecodeLearnCounter_;
    size_t holdCounter_;
    size_t freezeCounter_;
    SampleEntry<Sample64>::CuePoint holdCue_;
    SampleEntry<Sample64>::CuePoint endHoldCue_;
    SampleEntry<Sample64>::CuePoint beginFreezeCue_;
    SampleEntry<Sample64>::CuePoint endFreezeCue_;
    SampleEntry<Sample64>::CuePoint freezeCue_;

    ReaderManager params_;
};


}} // namespaces
