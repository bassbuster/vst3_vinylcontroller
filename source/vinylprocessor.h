#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "pluginterfaces/vst/ivstevents.h"

#include "helpers/sampleentry.h"
#include "helpers/parameterreader.h"
#include "helpers/padentry.h"

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
    tresult PLUGIN_API notify (IMessage* message) override;
	
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

        Filtred& operator +=(SampleType val) {
            value = (SampleType(round - 1) * value + val) / SampleType(round);
            return *this;
        }

        operator SampleType() const {
            return value;
        }

    private:
        SampleType value;
    };

	Sample32 SignalL[EFilterFrame];
	Sample32 SignalR[EFilterFrame];
    Filtred<Sample32, EFilterFrame> FSignalL;
    Filtred<Sample32, EFilterFrame> FSignalR;
	Sample32 FFTPre[EFFTFrame];
	Sample32 FFT[EFFTFrame];

    size_t SCursor;
    size_t FCursor;
    size_t FFTCursor;

    Filtred<Sample32, 10> absAVGSpeed;
	short Direction;
    Filtred<Sample32> DeltaL;
    Filtred<Sample32> DeltaR;
	Sample32 OldSignalL;
	Sample32 OldSignalR;
    Filtred<Sample32> filtredL;
    Filtred<Sample32> filtredR;
	Sample32 TimeCodeAmplytude;
    size_t SpeedCounter;
	bool StatusR;
	bool StatusL;
	bool OldStatusR;
	bool OldStatusL;
	bool PStatusR;
	bool PStatusL;
	bool POldStatusR;
	bool POldStatusL;
	bool Direction0;
	bool Direction1;
	bool Direction2;
	bool Direction3;

	int32 effectorSet;

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

    std::vector<std::unique_ptr<SampleEntry<Sample32>>> SamplesArray;
    std::unique_ptr<SampleEntry<Sample32>> VintageSample;

    PadEntry padStates[EMaximumScenes][ENumberOfPads];

	int32 currentProcessMode;
	bool currentProcessStatus;

    void CalcDirectionTimeCodeAmplitude(void);
    void CalcAbsSpeed(void);
    void SetCurrentEntry(int _newentry);

    void SetPadState(PadEntry * _Pad);

    bool PadWork(int _padId, Sample32 _paramValue);
    bool PadSet(int _currentSample);
    bool PadRemove(int _currentSample);
    float GetNormalizeTag(int _tag);

    // SampleBase manipulation
    void addSampleMessage(SampleEntry<Sample32>* newSample);
    void delSampleMessage(SampleEntry<Sample32>* delSample);
    void initSamplesMessage(void);
    void updateSpeedMessage(Sample64 speed);
    void updatePositionMessage(Sample64 speed);
    void updatePadsMessage(void);

    bool dirtyParams;

	double dSampleRate;
	double dTempo;
	double dNoteLength;

	Sample32 fRealPitch;
	Sample32 fRealSpeed;
	Sample32 fRealVolume;
    Filtred<Sample32, 16> fVolCoeff;
    Filtred<Sample32, 256> avgTimeCodeCoeff;
	int TimecodeLearnCounter;
	int HoldCounter;
	int FreezeCounter;
    SampleEntry<Sample32>::CuePoint HoldCue;
    SampleEntry<Sample32>::CuePoint AfterHoldCue;
    SampleEntry<Sample32>::CuePoint FreezeCue;
    SampleEntry<Sample32>::CuePoint AfterFreezeCue;
    SampleEntry<Sample32>::CuePoint FreezeCueCur;
    SampleEntry<Sample32>::CuePoint Cue;

    void processEvent(const Event &event);
    void reset(bool state);

    ReaderManager params;
};


}} // namespaces
