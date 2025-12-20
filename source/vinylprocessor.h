#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "pluginterfaces/vst/ivstevents.h"

#include "helpers/sampleentry.h"
#include "helpers/parameterreader.h"
#include "helpers/padentry.h"
#include "helpers/fft.h"

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

//------------------------------------------------------------------------
private:

    template<typename SampleType, int round = 4>
    class Filtred {
    public:

        Filtred(SampleType val = 0):
            value(val)
        {}

        SampleType set(SampleType val) {
            value = (SampleType(round - 1) * value + val) / SampleType(round);
            return value;
        }

        Filtred& append(SampleType val) {
            value = (SampleType(round - 1) * value + val) / SampleType(round);
            return *this;
        }

        operator SampleType() const {
            return value;
        }

    private:
        SampleType value;
    };

    Sample64 SignalL[EFilterFrame];
    Sample64 SignalR[EFilterFrame];
    Filtred<Sample64, EFilterFrame> FSignalL;
    Filtred<Sample64, EFilterFrame> FSignalR;
    Sample64 FFTPre[EFFTFrame];
    Sample64 FFT[EFFTFrame];

    //Sample64 filtred_[EFFTFrame];
    //Complex<double> fft_[EFFTFrame];

    size_t SCursor;
    size_t FCursor;
    size_t FFTCursor;

    Filtred<Sample64, 10> absAVGSpeed;
//	short Direction;
    Filtred<Sample64> DeltaL;
    Filtred<Sample64> DeltaR;
    Sample64 OldSignalL;
    Sample64 OldSignalR;
    Filtred<Sample64> filtredL;
    Filtred<Sample64> filtredR;
    Sample64 TimeCodeAmplytude;
    size_t SpeedCounter;
	bool StatusR;
	bool StatusL;
	bool OldStatusR;
	bool OldStatusL;
	bool PStatusR;
	bool PStatusL;
	bool POldStatusR;
	bool POldStatusL;

    uint16_t direction_;
    // bool Direction0;
    // bool Direction1;
    // bool Direction2;
    // bool Direction3;

    int32 effectorSet_;

    // SoftCurves
    Filtred<Sample32, ESoftEffectSamples> softVolume;
    Filtred<Sample32, ESoftEffectSamples> softPreRoll;
    Filtred<Sample32, ESoftEffectSamples> softPostRoll;
    Filtred<Sample32, ESoftEffectSamples> softDistorsion;
    Filtred<Sample32, ESoftEffectSamples> softHold;
    Filtred<Sample32, ESoftEffectSamples> softFreeze;
    Filtred<Sample32, ESoftEffectSamples> softVintage;

	Sample64 lockSpeed;
	Sample64 lockVolume;
	Sample64 lockTune;

	bool keyHold;
	bool keyFreeze;
	bool keyLockTone;

	// our model values
	Sample32 fVuOldL;
	Sample32 fVuOldR;
	Sample32 fPosition;
	Sample32 fGain;          //0..+1
	Sample32 fVolume;        //0..+1
	Sample32 fPitch;         //0..+1
    uint32_t iCurrentEntry;  //0..MaximumSamples - 1
    uint32_t iCurrentScene;  //0..MaximumScenes - 1
	Sample32 fSwitch;        //0..+1
	Sample32 fCurve;         //0..+1
    bool bBypass;
	bool bTCLearn;

    std::vector<std::unique_ptr<SampleEntry<Sample64>>> SamplesArray;
    std::unique_ptr<SampleEntry<Sample64>> VintageSample;

    PadEntry padStates[EMaximumScenes][ENumberOfPads];

	int32 currentProcessMode;
	bool currentProcessStatus;

    void CalcDirectionTimeCodeAmplitude(void);
    void CalcAbsSpeed(void);
    void currentEntry(int64_t newentry);

    //void padState(PadEntry &pad); 

    bool padWork(int padId, double paramValue);
    bool padSet(int currentSample);
    bool padRemove(int currentSample);
    //double normalizeTag(int _tag);

    // SampleBase manipulation
    void addSampleMessage(SampleEntry<Sample64>* newSample);
    void delSampleMessage(SampleEntry<Sample64>* delSample);
    void initSamplesMessage(void);
    void updateSpeedMessage(Sample64 speed);
    void updatePositionMessage(Sample64 speed);
    void updatePadsMessage(void);

    void debugFftMessage(Sample64 *fft, size_t len);
    void debugInputMessage(Sample64 *input, size_t len);

    bool dirtyParams;

    Sample64 dSampleRate;
    Sample64 dTempo;
    Sample64 dNoteLength;

    Sample64 fRealPitch;
    Sample64 fRealSpeed;
    Sample64 fRealVolume;
    Filtred<Sample64, 16> fVolCoeff;
    Filtred<Sample64, 256> avgTimeCodeCoeff;
	int TimecodeLearnCounter;
	int HoldCounter;
	int FreezeCounter;
    SampleEntry<Sample64>::CuePoint HoldCue;
    SampleEntry<Sample64>::CuePoint AfterHoldCue;
    SampleEntry<Sample64>::CuePoint FreezeCue;
    SampleEntry<Sample64>::CuePoint AfterFreezeCue;
    SampleEntry<Sample64>::CuePoint FreezeCueCur;
    SampleEntry<Sample64>::CuePoint Cue;

    void processEvent(const Event &event);
    void reset(bool state);

    ReaderManager params;
};


}} // namespaces
