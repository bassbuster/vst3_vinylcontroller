#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "pluginterfaces/vst/ivstevents.h"

#include "helpers/sampleentry.h"
#include "helpers/parameterreader.h"
#include "helpers/padentry.h"
#include "helpers/speedprocessor.h"
#include "effects/effector.h"

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

    SpeedProcessor<Sample64, ESpeedFrame, EFFTFrame, EFilterFrame> speedProcessor_;
    int32_t effectorSet_;

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

    std::vector<std::unique_ptr<SampleEntry<Sample64>>> samplesArray_;

    PadEntry padStates_[EMaximumScenes][ENumberOfPads];

    int32 currentProcessMode_;
    bool currentProcessStatus_;

    bool dirtyParams_;

    double sampleRate_;
    double tempo_;
    size_t noteLength_;

    Sample64 realPitch_;
    Sample64 realVolume_;

    ReaderManager params_;
    Effector effector_;
};


}} // namespaces
