//------------------------------------------------------------------------
// Project     : VST SDK
// Version     : 3.1.0
//
// Category    : Examples
// Filename    : public.sdk/samples/vst/again/source/again.h
// Created by  : Steinberg, 04/2005
// Description : AGain Example for VST SDK 3.0
//               Simple gain plug-in with gain, bypass values and 1 midi input
//               and the same plug-in with sidechain 
//
//-----------------------------------------------------------------------------
// LICENSE
// (c) 2010, Steinberg Media Technologies GmbH, All Rights Reserved
//-----------------------------------------------------------------------------
// This Software Development Kit may not be distributed in parts or its entirety  
// without prior written agreement by Steinberg Media Technologies GmbH. 
// This SDK must not be used to re-engineer or manipulate any technology used  
// in any Steinberg or Third-party application or software module, 
// unless permitted by law.
// Neither the name of the Steinberg Media Technologies nor the names of its
// contributors may be used to endorse or promote products derived from this 
// software without specific prior written permission.
// 
// THIS SDK IS PROVIDED BY STEINBERG MEDIA TECHNOLOGIES GMBH "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL STEINBERG MEDIA TECHNOLOGIES GMBH BE LIABLE FOR ANY DIRECT, 
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
//-----------------------------------------------------------------------------

#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "pluginterfaces/vst/ivstevents.h"

#include "helpers/sampleentry.h"
#include "helpers/parameterreader.h"

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
protected:

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
	DWORD SCursor;
	DWORD FCursor;
	DWORD FFTCursor;
    Filtred<Sample32, 10> absAVGSpeed;
	short Direction;
    Filtred<Sample32> DeltaL;
    Filtred<Sample32> DeltaR;
	Sample32 OldSignalL;
	Sample32 OldSignalR;
    Filtred<Sample32> filtredL;
    Filtred<Sample32> filtredR;
	Sample32 TimeCodeAmplytude;
	DWORD SpeedCounter;
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
	//int32 iOldEffectorSet;

	//////////SoftCurves
    Filtred<Sample32, ESoftEffectSamples> softVolume;  ///PunchIn PunchOut
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
	DWORD iCurrentEntry;  //0..MaximumSamples-1
	//DWORD iCurrentPad;
	DWORD iCurrentScene;  //0..MaximumScenes-1
	Sample32 fSwitch;        //0..+1
	Sample32 fCurve;         //0..+1
	bool  bBypass;        //thru.false
	bool bTCLearn;

    std::vector<std::unique_ptr<SampleEntry>> SamplesArray;
    std::unique_ptr<SampleEntry> VintageSample;

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

    static Sample32 aproxParamValue(int32 currentSampleIndex,double currentValue, int32 nextSampleIndex, double nextValue);
    static Sample32 CalcRealPitch(Sample32 _normalPitch, Sample32 _normalKoeff);
    static Sample32 CalcRealVolume(Sample32 _normalGain, Sample32 _normalVolume, Sample32 _normalKoeff);

	///SampleBase manipulation
    void addSampleMessage(SampleEntry* newSample);
    void delSampleMessage(SampleEntry* delSample);
    void initSamplesMessage(void);
    void updateSpeedMessage(Sample64 speed);
    void updatePositionMessage(Sample64 speed);
    void updatePadsMessage(void);
    bool dirtyParams;
private:

	double dSampleRate;
	double dTempo;
	double dNoteLength;
	//long lBeatLength;
	//long lBeatCount;
	//long lCurrentBeat;
	//CuePoint beatCue;

	//double dRollOffset;
	Sample32 fRealPitch;
	Sample32 fRealSpeed;
	Sample32 fRealVolume;
    Filtred<Sample32, 16> fVolCoeff;
    Filtred<Sample32, 256> avgTimeCodeCoeff;
	int TimecodeLearnCounter;
	int HoldCounter;
	int FreezeCounter;
	CuePoint HoldCue;
	CuePoint AfterHoldCue;
	CuePoint FreezeCue;
	CuePoint AfterFreezeCue;
	CuePoint FreezeCueCur;

	CuePoint Cue;
	//bool MidiKeyLearn;

    void processEvent(const Event &event);
    void reset(bool state);

    ReaderManager params;
};


}} // namespaces
