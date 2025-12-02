// ------------------------------------------------------------------------
// Project     : VST SDK
// Version     : 3.1.0
//
// Category    : Examples
// Filename    : public.sdk/samples/vst/again/source/again.cpp
// Created by  : Steinberg, 04/2005
// Description : AGain Example for VST SDK 3.0
//
// -----------------------------------------------------------------------------
// LICENSE
// (c) 2010, Steinberg Media Technologies GmbH, All Rights Reserved
// -----------------------------------------------------------------------------
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
// -----------------------------------------------------------------------------
#pragma hdrstop

#include "vinylprocessor.h"
#include "vinylparamids.h"
#include "vinylcids.h"	// for class ids
//#include "version.h"

#include "pluginterfaces/base/ibstream.h"
//#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <stdio.h>
#include <math.h>
#include "helpers/parameterwriter.h"

//#include <windows.h>
//#include <iostream>

//#pragma package(smart_init)

//extern void * hInstance;

#define READ_PARAMETER_FLOAT(savedParam)                              		\
float savedParam = 0.f;                                       	\
    if (state->read(&savedParam, sizeof(float)) != kResultOk) {   	\
        return kResultFalse;                                     	\
}

#define READ_PARAMETER_DWORD(savedParam)                                  	\
DWORD savedParam = 0;                                        	\
    if (state->read(&savedParam, sizeof(DWORD)) != kResultOk) {   	\
        return kResultFalse;                                      	\
}

// #define PARAMETER_CHANGES(a, b, c)                                         \
// if (a != b) {                                                   \
//     int32 index = 0;                                            \
//     IParamValueQueue* paramQueue =                              \
//                                    outParamChanges->addParameterData(c, index);               \
//     if (paramQueue) {                                           \
//         int32 index2 = 0;                                       \
//         paramQueue->addPoint(data.numSamples - 1, b, index2);                     \
//     }                                                           \
// }

//			if (xoredEffectSet & b) {
// #define PARAMETER_CHANGE(a,b)												\
// {                                                               \
//         int32 index = 0;                                            \
//         IParamValueQueue* paramQueue = outParamChanges->addParameterData(a, index);               \
//         if (paramQueue) {                                           \
//             int32 index2 = 0;                                       \
//             paramQueue->addPoint(data.numSamples - 1, ((effectorSet & b) > 0)?1:0, index2);                     \
//     }                                                           \
// }

// #define INIT_PARAMS_FOR_CHANGE(a,b,c,d,e)	\
// IParamValueQueue* a = 0;				\
//     int32 b = 0;                        	\
//     int32 c = 0;         					\
//     double d = e;

// #define GETPOINT_CASE(a,b,c,d,e)			\
// case a: b = paramQueue; paramQueue->getPoint (c ++,  d, e);	break;

namespace Steinberg {
namespace Vst {

// ------------------------------------------------------------------------
// AGain Implementation
// ------------------------------------------------------------------------
AVinyl::AVinyl() :
    fGain(0.8f),
    fRealVolume(0.8f),
    fVuOldL(0.f),
    fVuOldR(0.f),
    fVolume(1.0f),
    fPitch(.5f),
    fRealPitch(1.0f),
    iCurrentEntry(0),
    iCurrentScene(0),
    fSwitch(0),
    fCurve(0),
    currentProcessMode(-1), // -1 means not initialized
    bBypass(false),
    avgTimeCodeCoeff(ETimeCodeCoeff),
    bTCLearn(false),
    dSampleRate(EDefaultSampleRate),
    dTempo(EDefaultTempo),
    TimecodeLearnCounter(0),
    HoldCounter(0),
    FreezeCounter(0),
    dNoteLength(0),
    effectorSet(eNoEffects),
    softVolume(0),
    softPreRoll(0),
    softPostRoll(0),
    softDistorsion(0),
    softHold(0),
    softFreeze(0),
    softVintage(0),
    currentProcessStatus(false)
{
    // register its editor class (the same than used in againentry.cpp)
    setControllerClass(AVinylControllerUID);

    VintageSample = std::make_unique<SampleEntry>("vintage", "Resource/vintage.wav");
    if (VintageSample) {
        VintageSample->Loop = true;
        VintageSample->Sync = false;
        VintageSample->Reverse = false;
    }

    params.addReader(kBypassId, [this] () { return bBypass ? 1. : 0.; },
                     [this](Sample64 value) {
                         bBypass = value > 0.5;
                     });
    params.addReader(kGainId,   [this] () { return fGain; },
                     [this](Sample64 value) {
                         fGain = value;
                         fRealVolume = CalcRealVolume(fGain, fVolume, fCurve); },
                     [this] (int32 sampleOffset, int32 currentOffset, Sample64 currentValue) {
                         fGain = aproxParamValue(sampleOffset, fGain, currentOffset, currentValue);
                         fRealVolume = CalcRealVolume(fGain, fVolume, fCurve);
                     });
    params.addReader(kVolumeId, [this] () { return fVolume; },
                     [this](Sample64 value) {
                         fVolume = value;
                         fRealVolume = CalcRealVolume(fGain, fVolume, fCurve); },
                     [this] (int32 sampleOffset, int32 currentOffset, Sample64 currentValue) {
                         fVolume = aproxParamValue(sampleOffset, fVolume, currentOffset, currentValue);
                         fRealVolume = CalcRealVolume(fGain, fVolume, fCurve);
                     });
    params.addReader(kVolCurveId, [this] () { return fCurve; },
                     [this](Sample64 value) {
                         fCurve = value;
                         fRealVolume = CalcRealVolume(fGain, fVolume, fCurve); },
                     [this] (int32 sampleOffset, int32 currentOffset, Sample64 currentValue) {
                         fCurve = aproxParamValue(sampleOffset, fCurve, currentOffset, currentValue);
                         fRealVolume = CalcRealVolume(fGain, fVolume, fCurve);
                     });
    params.addReader(kPitchId, [this] () { return fPitch; },
                     [this](Sample64 value) { fPitch = value; fRealPitch = CalcRealPitch (fPitch, fSwitch); },
                     [this] (int32 sampleOffset, int32 currentOffset, Sample64 currentValue) {
                         fPitch = aproxParamValue(sampleOffset, fPitch, currentOffset, currentValue);
                         fRealPitch = CalcRealPitch (fPitch, fSwitch);
                     });
    params.addReader(kPitchSwitchId, [this] () { return fSwitch; },
                     [this](Sample64 value) {
                         fSwitch = value;
                         fRealPitch = CalcRealPitch (fPitch, fSwitch); },
                     [this] (int32 sampleOffset, int32 currentOffset, Sample64 currentValue) {
                         fSwitch = aproxParamValue(sampleOffset, fSwitch, currentOffset, currentValue);
                         fRealPitch = CalcRealPitch (fPitch, fSwitch);
                     });

    params.addReader(kCurrentEntryId, [this] () { return double(iCurrentEntry / (EMaximumSamples - 1)); },
                     [this](Sample64 value) {
                         SetCurrentEntry(std::floor(value * double(EMaximumSamples - 1) + 0.5));
                         dirtyParams |= PadSet(iCurrentEntry);
                     });
    params.addReader(kCurrentSceneId, [this] () { return double(iCurrentScene / (EMaximumScenes - 1)); },
                     [this](Sample64 value) {
                         iCurrentScene = std::floor(value * float(EMaximumScenes - 1) + 0.5f);
                         dirtyParams = true;
                     });

    params.addReader(kLoopId, [this] () { return (SamplesArray.size() > 0) ? (SamplesArray.at(iCurrentEntry)->Loop ? 1. : 0.) : 0.; },
                     [this](Sample64 value) {
                         if (SamplesArray.size() > iCurrentEntry) {
                             SamplesArray.at(iCurrentEntry)->Loop = value > 0.5;
                         }
                     });

    params.addReader(kSyncId, [this] () { return (SamplesArray.size() > 0) ? (SamplesArray.at(iCurrentEntry)->Sync ? 1. : 0.) : 0.; },
                     [this](Sample64 value) {
                         if (SamplesArray.size() > iCurrentEntry) {
                             SamplesArray.at(iCurrentEntry)->Sync = value > 0.5;
                         }
                     });

    params.addReader(kReverseId, [this] () { return (SamplesArray.size() > 0) ? (SamplesArray.at(iCurrentEntry)->Reverse ? 1. : 0.) : 0.; },
                     [this](Sample64 value) {
                         if (SamplesArray.size() > iCurrentEntry) {
                             SamplesArray.at(iCurrentEntry)->Reverse = value > 0.5;
                         }
                     });

    params.addReader(kAmpId, [this] () { return (SamplesArray.size() > 0) ? (SamplesArray.at(iCurrentEntry)->Level / 2.) : 0.5; },
                     [this](Sample64 value) {
                         if (SamplesArray.size() > iCurrentEntry) {
                             SamplesArray.at(iCurrentEntry)->Level = value * 2.0;
                         }
                     });

    params.addReader(kTuneId, [this] () { return (SamplesArray.size()>0) ? (SamplesArray.at(iCurrentEntry)->Tune > 1.0 ? SamplesArray.at(iCurrentEntry)->Tune / 2.0 : SamplesArray.at(iCurrentEntry)->Tune - 0.5): 0.5; },
                     [this](Sample64 value) {
                         if (SamplesArray.size() > iCurrentEntry) {
                             SamplesArray.at(iCurrentEntry)->Tune = value > 0.5 ? value * 2.0 : value + 0.5;
                         }
                     });

    params.addReader(kTimecodeLearnId, [this] () { return bTCLearn ? 1. : 0.; },
                     [this](Sample64 value) {
                         if (value>0.5) {
                             TimecodeLearnCounter = ETimecodeLearnCount;
                             bTCLearn = true;
                         }
                     });
}

// ------------------------------------------------------------------------
AVinyl::~AVinyl() {
    // delete FFT;
    //if (VintageSample) delete VintageSample;
    // nothing to do here yet..
}

// ------------------------------------------------------------------------
tresult PLUGIN_API AVinyl::initialize(FUnknown* context) {

    tresult result = AudioEffect::initialize(context);
    // if everything Ok, continue
    if (result != kResultOk) {
        return result;
    }

    // ---create Audio In/Out busses------
    // we want a stereo Input and a Stereo Output
    addAudioInput(STR16("Stereo In"), SpeakerArr::kStereo);
    addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);
    addAudioInput(STR16("Stereo Aux In"), SpeakerArr::kStereo, kAux, 0);

    // ---create Midi In/Out busses (1 bus with only 1 channel)------
    addEventInput(STR16("Midi In"), 1);

    // ---InitPads------
    for (int j = 0; j < EMaximumScenes; j++) {
        for (int i = 0; i < ENumberOfPads; i++) {
            padStates[j][i].padType = PadEntry::kSamplePad;
            padStates[j][i].padTag =j * ENumberOfPads + i;
            padStates[j][i].padState = false;
            padStates[j][i].padMidi = gPad[i];
        }
    }

    reset(true);
    dirtyParams = false;
    return kResultOk;
}

// ------------------------------------------------------------------------
tresult PLUGIN_API AVinyl::terminate() {
    // nothing to do here yet...except calling our parent terminate
    SamplesArray.clear();
    return AudioEffect::terminate();
}

// ------------------------------------------------------------------------
tresult PLUGIN_API AVinyl::setActive(TBool state) {
    reset(state);
    // call our parent setActive
    return AudioEffect::setActive(state);
}
/*
template<typename Callable>
class Finalizer {
public:

    Finalizer(Callable&& fin):
        fin(std::forward<Callable>(fin))
    {}

    ~Finalizer() {
        fin();
    }

private:
    Callable fin;
};

class Reader {
public:

    virtual ~Reader() = default;

    virtual void setQueue(IParamValueQueue* paramQueue) = 0;

    virtual void checkOffset(int32 sampleOffset) = 0;

    virtual void set(Sample64 initial) = 0;

    virtual void reset() = 0;

    virtual void flush() = 0;
};

template<typename Setter, typename Approximator>
class ParameterReader: public Reader {
public:

    ParameterReader(Setter && set,
                    Approximator &&approx):
        setter(std::forward<Setter>(set)),
        approximator(std::forward<Approximator>(approx)),
        value(0),
        queue(nullptr),
        index(0),
        offset(0)
    {}

    ~ParameterReader() override {
        flush();
    }

    void setQueue(IParamValueQueue* paramQueue) override final {
        queue = paramQueue;
        paramQueue->getPoint (index++, offset, value);
    }

    void checkOffset(int32 sampleOffset) override final {
        if (queue && offset == sampleOffset){
            setter(value);
            if (queue->getPoint(index++, offset, value) != kResultTrue) {
                queue = nullptr;
            }
        } else if (queue && offset > sampleOffset){
            approximator(sampleOffset, offset, value);
        }
    }

    void set(Sample64 initial) override final {
        value = initial;
        queue = nullptr;
        index = 0;
        offset = 0;
    }

    void reset() override final {
        queue = nullptr;
        index = 0;
        offset = 0;
    }

    void flush() override final {
        while (queue){
            set(value);
            if (queue->getPoint(index++, offset, value) != kResultTrue) {
                queue = nullptr;
            }
        }
    }

    // int32 currentOffset() const {
    //     return offset;
    // }

    // Sample64 currentValue() const {
    //     return value;
    // }

private:

    Setter setter;
    Approximator approximator;
    Sample64 value = {};
    IParamValueQueue* queue = 0;
    int32 index = 0;
    int32 offset = 0;

};

class ReaderManager {
public:

    ReaderManager() = default;

    template<typename Setter, typename Approximator>
    void addReader(ParamID id,
                   Setter && setter,
                   Approximator &&approx) {
        readers_.emplace(id, std::make_unique<ParameterReader>(std::forward<Setter>(setter), std::forward<Approximator>(approx)));
    }

    template<typename Setter>
    void addReader(ParamID id,
                   Setter && setter) {
        readers_.emplace(id, std::make_unique<ParameterReader>(std::forward<Setter>(setter), [] (int32, int32, Sample64) {}));
    }

    void flush() {
        for(auto &[_, reader]: readers_) {
            reader->flush();
        }
    }

    void reset() {
        for(auto &[_, reader]: readers_) {
            reader->reset();
        }
    }

    void checkOffset(int32 sampleOffset) {
        for(auto &[_, reader]: readers_) {
            reader->checkOffset(sampleOffset);
        }
    }

    void set(ParamID id, Sample64 value) {
        auto found = readers_.find(id);
        if (found != readers_.end()) {
            found->second->set(value);
        }
    }

    void setQueue(IParameterChanges* paramChanges) {
        int32 numParamsChanged = paramChanges->getParameterCount ();
        for (int32 i = 0; i < numParamsChanged; i++) {
            IParamValueQueue* paramQueue = paramChanges->getParameterData (i);
            if (paramQueue) {
                auto found = readers_.find(paramQueue->getParameterId());
                if (found != readers_.end()) {
                    found->second->setQueue(paramQueue);
                }
            }
        }
    }

private:
    std::map<ParamID, std::unique_ptr<Reader>> readers_;
};
*/
// ------------------------------------------------------------------------
tresult PLUGIN_API AVinyl::process(ProcessData& data) {


    bool samplesParamsUpdate = false;
    if (data.processContext) {
        dSampleRate = data.processContext->sampleRate;
        dTempo = data.processContext->tempo;
        dNoteLength = noteLengthInSamples(ERollNote,dTempo,dSampleRate);
        //lBeatLength = noteLengthInSamples(1,dTempo,dSampleRate);
    }

    IParameterChanges* paramChanges = data.inputParameterChanges;
    IParameterChanges* outParamChanges = data.outputParameterChanges;
    IEventList* eventList = data.inputEvents;

    /*

    ParameterReader bypassReader(bBypass ? 1. : 0.,
                                 [this](Sample64 value) { bBypass = value > 0.5; },
                                 [] (int32, int32, Sample64) {});
    ParameterReader gainReader(fGain,
                               [this](Sample64 value) {
                                   fGain = value;
                                   fRealVolume = CalcRealVolume(fGain, fVolume, fCurve); },
                               [this] (int32 sampleOffset, int32 currentOffset, Sample64 currentValue) {
                                   fGain = aproxParamValue(sampleOffset, fGain, currentOffset, currentValue);
                                   fRealVolume = CalcRealVolume(fGain, fVolume, fCurve);
                               });
    ParameterReader volumeReader(fVolume,
                                 [this](Sample64 value) {
                                     fVolume = value;
                                     fRealVolume = CalcRealVolume(fGain, fVolume, fCurve); },
                                 [this] (int32 sampleOffset, int32 currentOffset, Sample64 currentValue) {
                                     fVolume = aproxParamValue(sampleOffset, fVolume, currentOffset, currentValue);
                                     fRealVolume = CalcRealVolume(fGain, fVolume, fCurve);
                                 });
    ParameterReader curveReader(fCurve,
                                [this](Sample64 value) {
                                    fCurve = value;
                                    fRealVolume = CalcRealVolume(fGain, fVolume, fCurve); },
                                [this] (int32 sampleOffset, int32 currentOffset, Sample64 currentValue) {
                                    fCurve = aproxParamValue(sampleOffset, fCurve, currentOffset, currentValue);
                                    fRealVolume = CalcRealVolume(fGain, fVolume, fCurve);
                                });
    ParameterReader pitchReader(fPitch,
                                [this](Sample64 value) { fPitch = value; fRealPitch = CalcRealPitch (fPitch, fSwitch); },
                                [this] (int32 sampleOffset, int32 currentOffset, Sample64 currentValue) {
                                    fPitch = aproxParamValue(sampleOffset, fPitch, currentOffset, currentValue);
                                    fRealPitch = CalcRealPitch (fPitch, fSwitch);
                                });
    ParameterReader switchReader(fSwitch,
                                 [this](Sample64 value) {
                                     fSwitch = value;
                                     fRealPitch = CalcRealPitch (fPitch, fSwitch); },
                                 [this] (int32 sampleOffset, int32 currentOffset, Sample64 currentValue) {
                                     fSwitch = aproxParamValue(sampleOffset, fSwitch, currentOffset, currentValue);
                                     fRealPitch = CalcRealPitch (fPitch, fSwitch);
                                 });

    ParameterReader entryReader(double(iCurrentEntry / (EMaximumSamples - 1)),
                                [this](Sample64 value) {
                                    SetCurrentEntry(std::floor(value * double(EMaximumSamples - 1) + 0.5));
                                    dirtyParams |= PadSet(iCurrentEntry);
                                },
                                [] (int32, int32, Sample64) {});
    ParameterReader sceneReader(double(iCurrentScene / (EMaximumScenes - 1)),
                                [this](Sample64 value) {
                                    iCurrentScene = std::floor(value * float(EMaximumScenes - 1) + 0.5f);
                                    dirtyParams = true;
                                },
                                [] (int32, int32, Sample64) {});

    ParameterReader loopReader((SamplesArray.size() > 0) ? (SamplesArray.at(iCurrentEntry)->Loop ? 1. : 0.) : 0.,
                               [this](Sample64 value) {
                                   if (SamplesArray.size() > iCurrentEntry) {
                                       SamplesArray.at(iCurrentEntry)->Loop = value > 0.5;
                                   }
                               },
                               [] (int32, int32, Sample64) {});

    ParameterReader syncReader((SamplesArray.size() > 0) ? (SamplesArray.at(iCurrentEntry)->Sync ? 1. : 0.) : 0.,
                               [this](Sample64 value) {
                                   if (SamplesArray.size() > iCurrentEntry) {
                                       SamplesArray.at(iCurrentEntry)->Sync = value > 0.5;
                                   }
                               },
                               [] (int32, int32, Sample64) {});

    ParameterReader reverseReader((SamplesArray.size() > 0) ? (SamplesArray.at(iCurrentEntry)->Reverse ? 1. : 0.) : 0.,
                                  [this](Sample64 value) {
                                      if (SamplesArray.size() > iCurrentEntry) {
                                          SamplesArray.at(iCurrentEntry)->Reverse = value > 0.5;
                                      }
                                  },
                                  [] (int32, int32, Sample64) {});

    ParameterReader levelReader((SamplesArray.size() > 0) ? (SamplesArray.at(iCurrentEntry)->Level / 2.) : 0.5,
                                [this](Sample64 value) {
                                    if (SamplesArray.size() > iCurrentEntry) {
                                        SamplesArray.at(iCurrentEntry)->Level = value * 2.0;
                                    }
                                },
                                [] (int32, int32, Sample64) {});

    ParameterReader tuneReader((SamplesArray.size()>0) ? (SamplesArray.at(iCurrentEntry)->Tune > 1.0 ? SamplesArray.at(iCurrentEntry)->Tune / 2.0 : SamplesArray.at(iCurrentEntry)->Tune - 0.5): 0.5,
                               [this](Sample64 value) {
                                   if (SamplesArray.size() > iCurrentEntry) {
                                       SamplesArray.at(iCurrentEntry)->Tune = value > 0.5 ? value * 2.0 : value + 0.5;
                                   }
                               },
                               [] (int32, int32, Sample64) {});

    ParameterReader timecodeLearnReader(bTCLearn ? 1. : 0.,
                                        [this](Sample64 value) {
                                            if (value>0.5) {
                                                TimecodeLearnCounter = ETimecodeLearnCount;
                                                bTCLearn = true;
                                            }
                                        },
                                        [] (int32, int32, Sample64) {});*/

    //INIT_PARAMS_FOR_CHANGE(BypassQueue,BypassPointIndex,offsetBypassChange, newBypassValue,bBypass ? 1 : 0)
    //INIT_PARAMS_FOR_CHANGE(GainQueue,GainPointIndex,offsetGainChange,newGainValue,fGain)
    //INIT_PARAMS_FOR_CHANGE(VolumeQueue,VolumePointIndex,offsetVolumeChange,newVolumeValue,fVolume)
    //INIT_PARAMS_FOR_CHANGE(PitchQueue,PitchPointIndex,offsetPitchChange,newPitchValue,fPitch)
    //INIT_PARAMS_FOR_CHANGE(EntryQueue,EntryPointIndex,offsetEntryChange,newEntryValue,(float)iCurrentEntry/(EMaximumSamples-1))
    //INIT_PARAMS_FOR_CHANGE(SceneQueue,ScenePointIndex,offsetSceneChange,newSceneValue,(float)iCurrentScene/(EMaximumScenes-1))
    //INIT_PARAMS_FOR_CHANGE(LoopQueue,LoopPointIndex,offsetLoopChange, newLoopValue,(SamplesArray.size()>0)?(SamplesArray.at(iCurrentEntry)->Loop?1:0):0)
    //INIT_PARAMS_FOR_CHANGE(SyncQueue,SyncPointIndex,offsetSyncChange, newSyncValue,(SamplesArray.size()>0)?(SamplesArray.at(iCurrentEntry)->Sync?1:0):0)
    //INIT_PARAMS_FOR_CHANGE(AmpQueue,AmpPointIndex,offsetAmpChange, newAmpValue,(SamplesArray.size()>0)?(SamplesArray.at(iCurrentEntry)->Level/2.0):0.5)
    //INIT_PARAMS_FOR_CHANGE(TuneQueue,TunePointIndex,offsetTuneChange, newTuneValue,(SamplesArray.size()>0)?(SamplesArray.at(iCurrentEntry)->Tune>1.0?SamplesArray.at(iCurrentEntry)->Tune/2.0:SamplesArray.at(iCurrentEntry)->Tune-0.5):0.5)
    //INIT_PARAMS_FOR_CHANGE(ReverseQueue,ReversePointIndex,offsetReverseChange, newReverseValue,(SamplesArray.size()>0)?(SamplesArray.at(iCurrentEntry)->Reverse?1:0):0)
    //INIT_PARAMS_FOR_CHANGE(VCurveQueue,VCurvePointIndex,offsetVCurveChange,newVCurveValue,fCurve)
    //INIT_PARAMS_FOR_CHANGE(PCurveQueue,PCurvePointIndex,offsetPCurveChange,newPCurveValue,fSwitch)
    //INIT_PARAMS_FOR_CHANGE(TCLearnQueue,TCLearnPointIndex,offsetTCLearnChange, newTCLearnValue,bTCLearn?1:0)

    params.setQueue(paramChanges);

    ParameterWriter vuLeftWriter(kVuLeftId, outParamChanges);
    ParameterWriter vuRightWriter(kPositionId, outParamChanges);
    ParameterWriter positionWriter(kPositionId, outParamChanges);

    ParameterWriter loopWriter(kLoopId, outParamChanges);
    ParameterWriter syncWriter(kSyncId, outParamChanges);
    ParameterWriter levelWriter(kAmpId, outParamChanges);
    ParameterWriter reverseWriter(kReverseId, outParamChanges);
    ParameterWriter tuneWriter(kTuneId, outParamChanges);

    ParameterWriter entryWriter(kCurrentEntryId, outParamChanges);
    ParameterWriter sceneWriter(kCurrentSceneId, outParamChanges);

    ParameterWriter holdWriter(kHoldId, outParamChanges);
    ParameterWriter freezeWriter(kFreezeId, outParamChanges);
    ParameterWriter vintageWriter(kVintageId, outParamChanges);
    ParameterWriter distorsionWriter(kDistorsionId, outParamChanges);
    ParameterWriter preRollWriter(kPreRollId, outParamChanges);
    ParameterWriter postRollWriter(kPostRollId, outParamChanges);
    ParameterWriter lockWriter(kLockToneId, outParamChanges);
    ParameterWriter punchInWriter(kPunchInId, outParamChanges);
    ParameterWriter punchOutWriter(kPunchOutId, outParamChanges);

    ParameterWriter tcLearnWriter(kTimecodeLearnId, outParamChanges);

    /*if (paramChanges)
	{

        int32 numParamsChanged = paramChanges->getParameterCount ();
		for (int32 i = 0; i < numParamsChanged; i++)
		{
			IParamValueQueue* paramQueue = paramChanges->getParameterData (i);
			if (paramQueue)
			{
				switch (paramQueue->getParameterId ())
				{
                case kBypassId:
                    bypassReader.setQueue(paramQueue);
                    break;
                case kGainId:
                    gainReader.setQueue(paramQueue);
                    break;
                case kVolumeId:
                    volumeReader.setQueue(paramQueue);
                    break;
                case kVolCurveId:
                    curveReader.setQueue(paramQueue);
                    break;
                case kPitchId:
                    pitchReader.setQueue(paramQueue);
                    break;
                case kPitchSwitchId:
                    switchReader.setQueue(paramQueue);
                    break;
                case kCurrentEntryId:
                    entryReader.setQueue(paramQueue);
                    break;
                case kCurrentSceneId:
                    sceneReader.setQueue(paramQueue);
                    break;
                case kLoopId:
                    loopReader.setQueue(paramQueue);
                    break;
                case kSyncId:
                    syncReader.setQueue(paramQueue);
                    break;
                case kAmpId:
                    levelReader.setQueue(paramQueue);
                    break;
                case kTuneId:
                    tuneReader.setQueue(paramQueue);
                    break;
                case kReverseId:
                    reverseReader.setQueue(paramQueue);
                    break;
                case kTimecodeLearnId:
                    timecodeLearnReader.setQueue(paramQueue);
                    break;

                //GETPOINT_CASE(kGainId,GainQueue,GainPointIndex,offsetGainChange,newGainValue)
                //GETPOINT_CASE(kBypassId,BypassQueue,BypassPointIndex,offsetBypassChange, newBypassValue)
                //GETPOINT_CASE(kVolumeId,VolumeQueue,VolumePointIndex,offsetVolumeChange,newVolumeValue)
                //GETPOINT_CASE(kPitchId,PitchQueue,PitchPointIndex,offsetPitchChange,newPitchValue)
                //GETPOINT_CASE(kCurrentEntryId,EntryQueue,EntryPointIndex,offsetEntryChange,newEntryValue)
                //GETPOINT_CASE(kCurrentSceneId,SceneQueue,ScenePointIndex,offsetSceneChange,newSceneValue)
                //GETPOINT_CASE(kLoopId,LoopQueue,LoopPointIndex,offsetLoopChange, newLoopValue)
                //GETPOINT_CASE(kSyncId,SyncQueue,SyncPointIndex,offsetSyncChange, newSyncValue)
                //GETPOINT_CASE(kAmpId,AmpQueue,AmpPointIndex,offsetAmpChange, newAmpValue)
                //GETPOINT_CASE(kTuneId,TuneQueue,TunePointIndex,offsetTuneChange, newTuneValue)
                //GETPOINT_CASE(kReverseId,ReverseQueue,ReversePointIndex,offsetReverseChange, newReverseValue)
                //GETPOINT_CASE(kVolCurveId,VCurveQueue,VCurvePointIndex,offsetVCurveChange,newVCurveValue)
                //GETPOINT_CASE(kPitchSwitchId,PCurveQueue,PCurvePointIndex,offsetPCurveChange,newPCurveValue)
                //GETPOINT_CASE(kTimecodeLearnId,TCLearnQueue,TCLearnPointIndex,offsetTCLearnChange, newTCLearnValue)
				}
			}
        }
    }*/

    Event event;
    Event *eventP = nullptr;
    int32 eventIndex = 0;

    Finalizer readTheRest([&](){
        // while (GainQueue){
        //     fGain = newGainValue;
        //     if (GainQueue->getPoint(GainPointIndex++, offsetGainChange, newGainValue) != kResultTrue) {
        //         GainQueue = nullptr;
        //     }
        // }
        // while (VolumeQueue) {
        //     fVolume = newVolumeValue;
        //     if (VolumeQueue->getPoint(VolumePointIndex++, offsetVolumeChange, newVolumeValue) != kResultTrue) {
        //         VolumeQueue = nullptr;
        //     }
        // }
        // while (VCurveQueue) {
        //     fCurve = newVCurveValue;
        //     if (VCurveQueue->getPoint(VCurvePointIndex++, offsetVCurveChange, newVCurveValue) != kResultTrue) {
        //         VCurveQueue = nullptr;
        //     }
        // }
        // while (PitchQueue) {
        //     fPitch = newPitchValue;
        //     if (PitchQueue->getPoint(PitchPointIndex++, offsetPitchChange, newPitchValue) != kResultTrue) {
        //         PitchQueue = nullptr;
        //     }
        // }
        // while (PCurveQueue) {
        //     fSwitch = newPCurveValue;
        //     if (PCurveQueue->getPoint(PCurvePointIndex++, offsetPCurveChange, newPCurveValue) != kResultTrue) {
        //         PCurveQueue = nullptr;
        //     }
        // }
        // while (BypassQueue){
        //     bBypass = newBypassValue > 0.5;
        //     if (BypassQueue->getPoint(BypassPointIndex++, offsetBypassChange, newBypassValue) != kResultTrue) {
        //         BypassQueue = nullptr;
        //     }
        // }
        // while (LoopQueue && (SamplesArray.size() > iCurrentEntry)) {
        //     SamplesArray.at(iCurrentEntry)->Loop = newLoopValue > 0.5;
        //     if (LoopQueue->getPoint(LoopPointIndex++, offsetLoopChange, newLoopValue) != kResultTrue) {
        //         LoopQueue = nullptr;
        //     }
        // }
        // while (SyncQueue && (SamplesArray.size() > iCurrentEntry)) {
        //     SamplesArray.at(iCurrentEntry)->Sync = newSyncValue > 0.5;
        //     if (SyncQueue->getPoint(SyncPointIndex++, offsetSyncChange, newSyncValue) != kResultTrue) {
        //         SyncQueue = nullptr;
        //     }
        // }
        // while (ReverseQueue && (SamplesArray.size() > iCurrentEntry)) {
        //     SamplesArray.at(iCurrentEntry)->Reverse = newReverseValue > 0.5;
        //     if (ReverseQueue->getPoint(ReversePointIndex++, offsetReverseChange, newReverseValue) != kResultTrue) {
        //         ReverseQueue = nullptr;
        //     }
        // }
        // while (AmpQueue && (SamplesArray.size() > iCurrentEntry)) {
        //     SamplesArray.at(iCurrentEntry)->Level = newAmpValue * 2.0;
        //     if (AmpQueue->getPoint(AmpPointIndex++, offsetAmpChange, newAmpValue) != kResultTrue) {
        //         AmpQueue = nullptr;
        //     }
        // }
        // while (TuneQueue && (SamplesArray.size() > iCurrentEntry)) {
        //     SamplesArray.at(iCurrentEntry)->Tune = newTuneValue > 0.5 ? newTuneValue * 2.0 : newTuneValue + 0.5;
        //     if (TuneQueue->getPoint(TunePointIndex++, offsetTuneChange, newTuneValue) != kResultTrue) {
        //         TuneQueue = nullptr;
        //     }
        // }
        // while (EntryQueue) {
        //     SetCurrentEntry(std::floor(newEntryValue * (float)(EMaximumSamples-1)+0.5));
        //     dirtyParams |= PadSet(iCurrentEntry);
        //     if (EntryQueue->getPoint(EntryPointIndex++, offsetEntryChange, newEntryValue) != kResultTrue) {
        //         EntryQueue = nullptr;
        //     }
        // }
        // while (SceneQueue) {
        //     iCurrentScene = std::floor(newSceneValue * float(EMaximumScenes - 1) + 0.5f);
        //     dirtyParams = true;
        //     if (SceneQueue->getPoint(ScenePointIndex++, offsetSceneChange, newSceneValue) != kResultTrue) {
        //         SceneQueue = nullptr;
        //     }
        // }
        // while (TCLearnQueue) {
        //     if (newTCLearnValue > 0.5) {
        //         TimecodeLearnCounter = ETimecodeLearnCount;
        //         bTCLearn = true;
        //     }
        //     if (TCLearnQueue->getPoint(TCLearnPointIndex++, offsetTCLearnChange, newTCLearnValue) != kResultTrue) {
        //         TCLearnQueue = nullptr;
        //     }
        // }
        //fRealVolume = CalcRealVolume(fGain, fVolume, fCurve);
        //fRealPitch = CalcRealPitch (fPitch, fSwitch);

        params.flush();

        while (eventList) {
            if (eventList->getEvent(eventIndex++, event) != kResultOk) {
                eventList = nullptr;
            } else {
                processEvent(event);
            }
        }
    });


    // -------------------------------------
    // ---3) Process Audio---------------------
    // -------------------------------------
    if (data.numInputs == 0 || data.numOutputs == 0) {
        // nothing to do
        return kResultOk;
    }

    // (simplification) we suppose in this example that we have the same input channel count than the output
    int32 numChannels = data.inputs[0].numChannels;

    // ---get audio buffers----------------
    float** in = data.inputs[0].channelBuffers32;
    float** out = data.outputs[0].channelBuffers32;

    //{
    float fVuLeft = 0.f;
    float fVuRight = 0.f;
    float fOldPosition = fPosition;
    //float fOldDistorsion = softDistorsion;
    //float fOldPreRoll = softPreRoll;
    //float fOldPostRoll = softPostRoll;
    bool bOldTimecodeLearn = bTCLearn;
    Sample64 fOldSpeed = fRealSpeed;

    if (numChannels >= 2) {


        int32 sampleFrames = data.numSamples;
        int32 sampleOffset = 0;
        Sample32* ptrInLeft = in[0];
        Sample32* ptrOutLeft = out[0];
        Sample32* ptrInRight = in[1];
        Sample32* ptrOutRight = out[1];
        // Sample32 inL;
        // Sample32 inR;

        while (--sampleFrames >= 0) {
            ////////////Get Param Changes	 ////////////////////////////////////////////////////////////

            params.checkOffset(sampleOffset);

            // bypassReader.checkOffset(sampleOffset);
            // gainReader.checkOffset(sampleOffset);
            // volumeReader.checkOffset(sampleOffset);
            // curveReader.checkOffset(sampleOffset);
            // pitchReader.checkOffset(sampleOffset);
            // switchReader.checkOffset(sampleOffset);
            // entryReader.checkOffset(sampleOffset);
            // sceneReader.checkOffset(sampleOffset);
            // loopReader.checkOffset(sampleOffset);
            // syncReader.checkOffset(sampleOffset);
            // timecodeLearnReader.checkOffset(sampleOffset);
            // reverseReader.checkOffset(sampleOffset);
            // levelReader.checkOffset(sampleOffset);
            // tuneReader.checkOffset(sampleOffset);

            // if(GainQueue){
            // 	if (offsetGainChange == sampleOffset) {
            // 		fGain = newGainValue;
            // 		fRealVolume = CalcRealVolume(fGain, fVolume, fCurve);
            // 		if (GainQueue->getPoint (GainPointIndex++,  offsetGainChange, newGainValue) != kResultTrue)
            //                            GainQueue = nullptr;
            //                    } else if (offsetGainChange > sampleOffset) {
            // 		fGain = aproxParamValue(sampleOffset,fGain,offsetGainChange,newGainValue);
            // 	}
            // }
            // if(VolumeQueue){
            // 	if (offsetVolumeChange == sampleOffset) {
            // 		fVolume = newVolumeValue;
            // 		fRealVolume = CalcRealVolume(fGain, fVolume, fCurve);
            // 		if (VolumeQueue->getPoint (VolumePointIndex++,  offsetVolumeChange, newVolumeValue) != kResultTrue)
            //                            VolumeQueue = nullptr;
            // 	}else
            // 	if (offsetVolumeChange > sampleOffset) {
            // 		fVolume = aproxParamValue(sampleOffset,fVolume,offsetVolumeChange,newVolumeValue);
            // 	}
            // }
            // if(VCurveQueue){
            // 	if (offsetVCurveChange == sampleOffset) {
            // 		fCurve = newVCurveValue;
            // 		fRealVolume = CalcRealVolume(fGain, fVolume, fCurve);
            // 		if (VCurveQueue->getPoint (VCurvePointIndex++,  offsetVCurveChange, newVCurveValue) != kResultTrue)
            //                            VCurveQueue = nullptr;
            // 	}else
            // 	if (offsetVCurveChange > sampleOffset) {
            // 		fCurve = aproxParamValue(sampleOffset,fCurve,offsetVCurveChange,newVCurveValue);
            // 	}
            // }
            // if(PitchQueue){
            // 	if (offsetPitchChange == sampleOffset) {
            // 		fPitch = newPitchValue;
            // 		fRealPitch = CalcRealPitch (fPitch,fSwitch);
            // 		if (PitchQueue->getPoint (PitchPointIndex++,  offsetPitchChange, newPitchValue) != kResultTrue)
            //                            PitchQueue = nullptr;
            // 	}else
            // 	if (offsetPitchChange > sampleOffset) {
            // 		fPitch = aproxParamValue(sampleOffset,fPitch,offsetPitchChange,newPitchValue);
            // 		fRealPitch = CalcRealPitch (fPitch,fSwitch);
            // 	}
            // }
            // if(PCurveQueue){
            // 	if (offsetPCurveChange == sampleOffset) {
            // 		fSwitch = newPCurveValue;
            // 		fRealPitch = CalcRealPitch (fPitch,fSwitch);
            // 		if (PCurveQueue->getPoint (PCurvePointIndex++,  offsetPCurveChange, newPCurveValue) != kResultTrue)
            //                            PCurveQueue = nullptr;
            // 	}else
            // 	if (offsetPCurveChange > sampleOffset) {
            // 		fSwitch = aproxParamValue(sampleOffset,fSwitch,offsetPCurveChange,newPCurveValue);
            // 		fRealPitch = CalcRealPitch (fPitch,fSwitch);
            // 	}
            // }

            // if(BypassQueue){
            // 	if (offsetBypassChange == sampleOffset) {
            //                        bBypass = newBypassValue > 0.5;
            // 		if (BypassQueue->getPoint (BypassPointIndex++,  offsetBypassChange, newBypassValue) != kResultTrue)
            //                            BypassQueue = nullptr;
            // 	}
            // }
            // if(LoopQueue){
            // 	if (offsetLoopChange == sampleOffset) {
            //                        if (SamplesArray.size() > iCurrentEntry)
            // 			SamplesArray.at(iCurrentEntry)->Loop = newLoopValue>0.5;
            // 		if (LoopQueue->getPoint (LoopPointIndex++,  offsetLoopChange, newLoopValue) != kResultTrue)
            //                            LoopQueue = nullptr;
            // 	}
            // }

            // if(SyncQueue){
            // 	if (offsetSyncChange == sampleOffset) {
            //                        if (SamplesArray.size() > iCurrentEntry)
            // 			SamplesArray.at(iCurrentEntry)->Sync = newSyncValue>0.5;
            // 		if (SyncQueue->getPoint (SyncPointIndex++,  offsetSyncChange, newSyncValue) != kResultTrue)
            //                            SyncQueue = nullptr;
            // 	}
            // }

            // if(ReverseQueue){
            // 	if (offsetReverseChange == sampleOffset) {
            //                        if (SamplesArray.size() > iCurrentEntry)
            // 			SamplesArray.at(iCurrentEntry)->Reverse = newReverseValue>0.5;
            // 		if (ReverseQueue->getPoint (ReversePointIndex++,  offsetReverseChange, newReverseValue) != kResultTrue)
            //                            ReverseQueue = nullptr;
            // 	}
            // }

            // if(AmpQueue){
            // 	if (offsetAmpChange == sampleOffset) {
            //                        if (SamplesArray.size() > iCurrentEntry)
            // 			SamplesArray.at(iCurrentEntry)->Level = newAmpValue*2.0;
            // 		if (AmpQueue->getPoint (AmpPointIndex++,  offsetAmpChange, newAmpValue) != kResultTrue)
            //                            AmpQueue = nullptr;
            // 	}
            // }

            // if(TuneQueue){
            // 	if (offsetTuneChange == sampleOffset) {
            //                        if (SamplesArray.size() > iCurrentEntry)
            // 			SamplesArray.at(iCurrentEntry)->Tune = newTuneValue>0.5?newTuneValue*2.0:newTuneValue+0.5;
            // 		if (TuneQueue->getPoint (TunePointIndex++,  offsetTuneChange, newTuneValue) != kResultTrue)
            //                            TuneQueue = nullptr;
            // 	}
            // }

            // if(EntryQueue){
            // 	if (offsetEntryChange == sampleOffset) {
            // 		SetCurrentEntry (floor(newEntryValue * (float)(EMaximumSamples-1)+0.5));
            // 		samplesParamsUpdate = true;
            // 		dirtyParams |= PadSet(iCurrentEntry);
            // 		if (EntryQueue->getPoint (EntryPointIndex++,  offsetEntryChange, newEntryValue) != kResultTrue)
            //                            EntryQueue = nullptr;
            // 	}
            // }

            // if(SceneQueue){
            // 	if (offsetSceneChange == sampleOffset) {
            // 		iCurrentScene = floor(newSceneValue * (float)(EMaximumScenes-1)+0.5);
            // 		dirtyParams = true;
            // 		if (SceneQueue->getPoint (ScenePointIndex++,  offsetSceneChange, newSceneValue) != kResultTrue)
            //                            SceneQueue = nullptr;
            // 	}
            // }

            // if(TCLearnQueue){
            // 	if (offsetTCLearnChange == sampleOffset) {
            // 		if (newTCLearnValue>0.5) {
            // 			TimecodeLearnCounter = ETimecodeLearnCount;
            // 			bTCLearn = true;
            // 		}
            // 		if (TCLearnQueue->getPoint (TCLearnPointIndex++,  offsetTCLearnChange, newTCLearnValue) != kResultTrue)
            //                            TCLearnQueue = nullptr;
            // 	}
            // }


            ////////////Get Offsetted Events ////////////////////////////////////////////////////////////
            if (eventList) {
                if (eventP == nullptr){
                    if (eventList->getEvent(eventIndex++, event) != kResultOk) {
                        eventList = nullptr;
                        eventP = nullptr;
                    } else {
                        eventP = &event;
                    }
                }
                if ((eventP != nullptr) && (event.sampleOffset == sampleOffset)) {
                    processEvent(event);
                    eventP = nullptr;
                }
            }
            //////////////////////////////////////////////////////////////////////////////////////////////

            if (!bBypass) {


                Sample32 inL = *ptrInLeft++;
                Sample32 inR = *ptrInRight++;

                SignalL[FCursor] = (filtredL += inL);
                SignalR[FCursor] = (filtredL += inR);
                // FSignalL = ((Sample64)(EFilterFrame - 1) * FSignalL + inL)
                // 	/ (Sample64)EFilterFrame;
                // FSignalR = ((Sample64)(EFilterFrame - 1) * FSignalR + inR)
                // 	/ (Sample64)EFilterFrame;
                SignalL[SCursor] = SignalL[SCursor] - (FSignalL += inL);
                SignalR[SCursor] = SignalR[SCursor] - (FSignalR += inR);

                DeltaL += (SignalL[SCursor] - OldSignalL);
                DeltaR += (SignalR[SCursor] - OldSignalR);

                //*ptrOutLeft = SignalL[SCursor];
                //*ptrOutRight = SignalR[SCursor];

                CalcDirectionTimeCodeAmplitude();

                OldSignalL = SignalL[SCursor];
                OldSignalR = SignalR[SCursor];
                FCursor++;
                if (FCursor >= EFilterFrame) {
                    FCursor = 0;
                }
                SCursor++;
                if (SCursor >= EFilterFrame) {
                    SCursor = 0;
                }


                Sample64 SmoothCoef =
                    0.5 - 0.5 * cos((2.0 * Pi * Sample64(FFTCursor + EFFTFrame - ESpeedFrame) / EFFTFrame));
                FFTPre[FFTCursor + EFFTFrame - ESpeedFrame] = (OldSignalL - OldSignalR);
                FFT[FFTCursor + EFFTFrame - ESpeedFrame] = (SmoothCoef * FFTPre[FFTCursor + EFFTFrame - ESpeedFrame]);

                FFTCursor++;
                if (FFTCursor >= ESpeedFrame) {
                    FFTCursor = 0;
                    ///////////FFT!!!!!
                    //fPosition = 0.5;//TimeCodeAmplytude;
                    //String tttmm;
                    //tttmm = tttmm.printf("%f",fPosition);
                    //MessageBox(0,tttmm.text16(),STR("pos"),MB_OK);



                    if (TimeCodeAmplytude >= ETimeCodeMinAmplytude) {

                        //*ptrOutLeft = SignalL[SCursor];
                        //*ptrOutRight = SignalR[SCursor];

                        fastsinetransform(FFT, EFFTFrame);
                        // absAVGSpeed = absavgspeed(FFT,EFFTFrame,absAVGSpeed);
                        CalcAbsSpeed();
                        if (TimecodeLearnCounter>0) {
                            TimecodeLearnCounter--;
                            avgTimeCodeCoeff += (Direction * absAVGSpeed);
                            fRealSpeed = 1.0;
                        } else {
                            fRealSpeed = absAVGSpeed / avgTimeCodeCoeff;
                            bTCLearn = false;
                        }
                        fVolCoeff += sqrt(fabs(fRealSpeed));
                        fRealSpeed = (Sample64)Direction * fRealSpeed;
                    }

                    for (int i = 0; i < EFFTFrame-ESpeedFrame; i++) {
                        Sample64 SmoothCoef =
                            0.5 - 0.5 * cos((2.0 * Pi * Sample64(i) / EFFTFrame));
                        FFTPre[i] =	FFTPre[ESpeedFrame + i];
                        FFT[i] = (SmoothCoef * FFTPre[ESpeedFrame + i]);
                    }

                }


                if (TimeCodeAmplytude < ETimeCodeMinAmplytude) {


                    if (fVolCoeff >= 0.00001) {
                        absAVGSpeed = absAVGSpeed / 1.07;
                        fRealSpeed = absAVGSpeed / avgTimeCodeCoeff;
                        fVolCoeff += sqrt(fabs(fRealSpeed));
                        fRealSpeed = Sample64(Direction) * fRealSpeed;
                    } else {
                        absAVGSpeed = 0.0;
                        fRealSpeed = 0.0;
                        fVolCoeff = 0.0;
                    }

                }


                if ((fVolCoeff >= 0.00001) && (SamplesArray.size() > 0)) {

                    ////////////////////EFFECTOR BEGIN//////////////////////////////

                    //SoftCoeffs
                    // if (effectorSet & ePunchIn) {
                    //     softVolume += (fGain * fVolCoeff);
                    // } else if (effectorSet & ePunchOut) {
                    //     softVolume += 0.;
                    // } else {
                    //     softVolume += (fRealVolume * fVolCoeff);
                    // }

                    softVolume += ((effectorSet & ePunchIn) ? (fGain * fVolCoeff) :
                                       ((effectorSet & ePunchOut) ? 0. : (fRealVolume * fVolCoeff)));
                    softPreRoll += (effectorSet & ePreRoll ? 1. : 0.);
                    softPostRoll += (effectorSet & ePostRoll ? 1. : 0.);
                    softDistorsion += (effectorSet & eDistorsion ? 1. : 0.);
                    softVintage += (effectorSet & eVintage ? 1. : 0.);

                    // if (effectorSet & ePreRoll) {
                    //     softPreRoll += 1.0;
                    // } else {
                    //     softPreRoll += 0.;
                    // }
                    // if (effectorSet & ePostRoll) {
                    //     softPostRoll += 1.0;
                    // } else {
                    //     softPostRoll += 0.;
                    // }
                    // if (effectorSet & eDistorsion) {
                    //     softDistorsion += 1.0;
                    // } else {
                    //     softDistorsion += 0;
                    // }
                    // if (effectorSet & eVintage) {
                    //     softVintage += 1.0;
                    // } else {
                    //     softVintage += 0.;
                    // }

                    if (effectorSet & eHold) {
                        if (!keyHold) {
                            HoldCue = SamplesArray.at(iCurrentEntry)->GetCue();
                            keyHold = true;
                            //SamplesArray.at(iCurrentEntry)->Hold(dNoteLength);
                        }
                        softHold += 1.;
                    } else {
                        if (keyHold) {
                            //SamplesArray.at(iCurrentEntry)->Resume();
                            keyHold = false;
                        }
                        softHold += 0.;
                    }

                    if (effectorSet & eFreeze) {
                        if (!keyFreeze) {
                            FreezeCue = SamplesArray.at(iCurrentEntry)->GetCue();
                            FreezeCueCur = FreezeCue;
                            keyFreeze = true;
                        }
                        softFreeze += 1.;
                    } else {
                        if (keyFreeze) {
                            keyFreeze = false;
                            AfterFreezeCue = FreezeCueCur;
                        }
                        softFreeze += 0.;
                    }

                    if (effectorSet & eLockTone) {
                        if (!keyLockTone) {
                            keyLockTone = true;
                            lockSpeed = fabs(fRealSpeed * fRealPitch);
                            lockVolume = fVolCoeff;
                            lockTune = SamplesArray.at(iCurrentEntry)->Tune;
                            SamplesArray.at(iCurrentEntry)->BeginLockStrobe();
                            //lockSpeed = fRealSpeed;
                        }
                        fVolCoeff = lockVolume;
                    } else {
                        if (keyLockTone) {
                            keyLockTone = false;
                        }
                    }


                    //////////////////////////PLAYER////////////////////////////////////////////////
                    Sample64 _speed = 0;//fRealSpeed * fRealPitch;
                    Sample64 _tempo = 0;

                    if (keyLockTone) {
                        _speed = (Sample64)Direction * lockSpeed;
                        _tempo = SamplesArray.at(iCurrentEntry)->Sync?fabs(dTempo * fRealSpeed * fRealPitch):SamplesArray.at(iCurrentEntry)->GetTempo() * fabs(fRealSpeed * fRealPitch) * lockTune;
                        SamplesArray.at(iCurrentEntry)->PlayStereoSampleTempo(ptrOutLeft,
                                                                              ptrOutRight, _speed, _tempo, dSampleRate,true);
                    }else{
                        _speed = fRealSpeed * fRealPitch;
                        _tempo = dTempo;
                        SamplesArray.at(iCurrentEntry)->PlayStereoSample(ptrOutLeft,
                                                                         ptrOutRight, _speed, _tempo, dSampleRate,true);
                    }

                    if (keyFreeze) {
                        FreezeCounter++;
                        bool pushSync = SamplesArray.at(iCurrentEntry)->Sync;
                        SamplesArray.at(iCurrentEntry)->Sync = false;
                        CuePoint PushCue = SamplesArray.at(iCurrentEntry)->GetCue();
                        SamplesArray.at(iCurrentEntry)->SetCue(FreezeCueCur);
                        SamplesArray.at(iCurrentEntry)->PlayStereoSample(ptrOutLeft,
                                                                         ptrOutRight, _speed, _tempo, dSampleRate,true);
                        FreezeCueCur =  SamplesArray.at(iCurrentEntry)->GetCue();
                        if ((softFreeze>0.00001)&&(softFreeze<0.99)) {
                            Sample32 fLeft = 0;
                            Sample32 fRight = 0;
                            //CuePoint pPushCue = SamplesArray.at(iCurrentEntry)->GetCue();
                            SamplesArray.at(iCurrentEntry)->SetCue(AfterFreezeCue);
                            SamplesArray.at(iCurrentEntry)->PlayStereoSample(&fLeft,
                                                                             &fRight, _speed, _tempo, dSampleRate,true);
                            *ptrOutLeft = *ptrOutLeft * softFreeze + fLeft * (1.0 - softFreeze);
                            *ptrOutRight = *ptrOutRight * softFreeze + fRight * (1.0 - softFreeze);
                            AfterFreezeCue = SamplesArray.at(iCurrentEntry)->GetCue();
                            //SamplesArray.at(iCurrentEntry)->SetCue(pPushCue);
                        }
                        if (_speed != 0.0){
                            if (FreezeCounter>=dNoteLength){//fabs(dNoteLength / _speed)){
                                FreezeCounter = 0;
                                AfterFreezeCue = FreezeCueCur;
                                FreezeCueCur = FreezeCue;
                                softFreeze = 0;
                            }
                        }
                        SamplesArray.at(iCurrentEntry)->Sync = pushSync;
                        SamplesArray.at(iCurrentEntry)->SetCue(PushCue);
                    }

                    if (keyHold) {
                        HoldCounter++;
                        if ((softHold>0.00001)&&(softHold<0.99)) {
                            Sample32 fLeft = 0;
                            Sample32 fRight = 0;
                            //bool pushSync = SamplesArray.at(iCurrentEntry)->Sync;
                            //SamplesArray.at(iCurrentEntry)->Sync = false;
                            CuePoint PushCue = SamplesArray.at(iCurrentEntry)->GetCue();
                            SamplesArray.at(iCurrentEntry)->SetCue(AfterHoldCue);
                            SamplesArray.at(iCurrentEntry)->PlayStereoSample(&fLeft,
                                                                             &fRight, _speed, SamplesArray.at(iCurrentEntry)->GetTempo(), dSampleRate,true);
                            *ptrOutLeft = *ptrOutLeft * softHold + fLeft * (1.0 - softHold);
                            *ptrOutRight = *ptrOutRight * softHold + fRight * (1.0 - softHold);
                            AfterHoldCue = SamplesArray.at(iCurrentEntry)->GetCue();
                            //SamplesArray.at(iCurrentEntry)->Sync = pushSync;
                            SamplesArray.at(iCurrentEntry)->SetCue(PushCue);
                        }
                        if (HoldCounter>=dNoteLength) {
                            AfterHoldCue = SamplesArray.at(iCurrentEntry)->GetCue();
                            SamplesArray.at(iCurrentEntry)->SetCue(HoldCue);
                            HoldCounter = 0;
                            softHold = 0;
                        }
                    }


                    ////////////////////////////////////////////////////////////////////////////


                    if ((softPreRoll > 0.0001) || (softPostRoll > 0.0001)) {

                        Sample64 offset = SamplesArray.at(iCurrentEntry)->GetNoteLength(ERollNote,dTempo);
                        Sample64 pre_offset = offset;
                        Sample64 pos_offset = -offset;

                        Sample32 fLeft = 0;
                        Sample32 fRight = 0;
                        for (int i = 0; i < ERollCount; i++) {
                            Sample32 rollVolume = (1.0 -(Sample32)i/(Sample32)ERollCount)*.6;
                            if (softPreRoll>0.0001) {
                                SamplesArray.at(iCurrentEntry)->PlayStereoSample(&fLeft,&fRight,pos_offset,false);
                                *ptrOutLeft = *ptrOutLeft * (1.0 - softPreRoll * 0.1) + fLeft * rollVolume * softPreRoll;
                                *ptrOutRight = *ptrOutRight * (1.0 - softPreRoll * 0.1) + fRight * rollVolume * softPreRoll;
                                pos_offset = pos_offset + offset;
                            }
                            if (softPostRoll>0.0001) {
                                SamplesArray.at(iCurrentEntry)->PlayStereoSample(&fLeft,&fRight,pre_offset,false);
                                *ptrOutLeft = *ptrOutLeft * (1.0 - softPostRoll * 0.1) + fLeft * rollVolume * softPostRoll;
                                *ptrOutRight = *ptrOutRight * (1.0 - softPostRoll * 0.1) + fRight * rollVolume * softPostRoll;
                                pre_offset = pre_offset - offset;
                            }
                        }
                    }
                    //else {
                    //	softPreRoll = 0;
                    //	softPostRoll = 0;
                    //}

                    if (softDistorsion > 0.0001) {
                        if (*ptrOutLeft>0) *ptrOutLeft = *ptrOutLeft * (1.0 - softDistorsion * 0.6) + 0.4*sqrt(*ptrOutLeft)*softDistorsion;
                        else *ptrOutLeft = *ptrOutLeft * (1.0 - softDistorsion * 0.6) - 0.3*sqrt(-*ptrOutLeft)*softDistorsion;
                        if (*ptrOutRight>0) *ptrOutRight = *ptrOutRight * (1.0 - softDistorsion * 0.6) + 0.4*sqrt(*ptrOutRight)*softDistorsion;
                        else *ptrOutRight = *ptrOutRight * (1.0 - softDistorsion * 0.6) - 0.3*sqrt(-*ptrOutRight)*softDistorsion;
                    }
                    //else {
                    //	softDistorsion = 0;
                    //}

                    if (softVintage > 0.0001) {
                        Sample32 VintageLeft = 0;
                        Sample32 VintageRight = 0;
                        if (VintageSample) {
                            VintageSample->PlayStereoSample(&VintageLeft,&VintageRight, _speed, dTempo, dSampleRate, true);
                        }
                        *ptrOutLeft = *ptrOutLeft * (1.0 - softVintage * .3) + (-sqr(*ptrOutRight*.5) + VintageLeft) * softVintage;
                        *ptrOutRight = *ptrOutRight * (1.0 - softVintage * .3)  + (-sqr(*ptrOutLeft*.5) +  VintageRight)*softVintage;
                    }
                    //else {
                    //	softVintage = 0;
                    //}

                    /////////////////////////END OF EFFECTOR//////////////////


                    *ptrOutLeft = *ptrOutLeft * softVolume;
                    *ptrOutRight = *ptrOutRight * softVolume;

                    fPosition = (float)SamplesArray.at(iCurrentEntry)->GetCue().IntegerPart / SamplesArray.at(iCurrentEntry)->GetBufferLength();
                    if (*ptrOutLeft > fVuLeft) {
                        fVuLeft = *ptrOutLeft;
                    }
                    if (*ptrOutRight > fVuRight) {
                        fVuRight = *ptrOutRight;
                    }

                }
                else {
                    *ptrOutLeft = 0;
                    *ptrOutRight = 0;
                }

                // check only positive values
            }else{
                *ptrOutLeft = 0;
                *ptrOutRight = 0;

            }

            ptrOutLeft++;
            ptrOutRight++;
            sampleOffset++;

        }
    }

    if (outParamChanges) {

        if (fVuOldL != fVuLeft ) {
            vuLeftWriter.store(data.numSamples - 1, fVuLeft);
        }
        if (fVuOldR != fVuRight) {
            vuRightWriter.store(data.numSamples - 1, fVuRight);
        }
        if (fOldPosition != fPosition) {
            positionWriter.store(data.numSamples - 1, fPosition);
        }
        //          PARAMETER_CHANGES(fVuOldL, fVuLeft, kVuLeftId)
        // PARAMETER_CHANGES(fVuOldR, fVuRight, kVuRightId)
        // PARAMETER_CHANGES(fOldPosition, fPosition, kPositionId)

        if ((samplesParamsUpdate || dirtyParams) && (SamplesArray.size() > iCurrentEntry)){

            loopWriter.store(data.numSamples - 1, SamplesArray.at(iCurrentEntry)->Loop ? 1. : 0.);
            syncWriter.store(data.numSamples - 1, SamplesArray.at(iCurrentEntry)->Sync ? 1. : 0.);
            levelWriter.store(data.numSamples - 1, SamplesArray.at(iCurrentEntry)->Level / 2.);
            reverseWriter.store(data.numSamples - 1, SamplesArray.at(iCurrentEntry)->Reverse ? 1. : 0.);
            tuneWriter.store(data.numSamples - 1, SamplesArray.at(iCurrentEntry)->Tune > 1. ? SamplesArray.at(iCurrentEntry)->Tune / 2. : SamplesArray.at(iCurrentEntry)->Tune - 0.5);

            // int32 index = 0;
            // IParamValueQueue* paramQueue =
            // 	outParamChanges->addParameterData(kLoopId, index);
            // if (paramQueue) {
            // 	int32 index2 = 0;
            // 	paramQueue->addPoint(data.numSamples - 1, SamplesArray.at(iCurrentEntry)->Loop?1:0, index2);
            // }
            // index = 0;
            // paramQueue = outParamChanges->addParameterData(kSyncId, index);
            // if (paramQueue) {
            // 	int32 index2 = 0;
            // 	paramQueue->addPoint(data.numSamples - 1, SamplesArray.at(iCurrentEntry)->Sync?1:0, index2);
            // }
            // index = 0;
            // paramQueue = outParamChanges->addParameterData(kReverseId, index);
            // if (paramQueue) {
            // 	int32 index2 = 0;
            // 	paramQueue->addPoint(data.numSamples - 1, SamplesArray.at(iCurrentEntry)->Reverse?1:0, index2);
            // }
            // index = 0;
            // paramQueue = outParamChanges->addParameterData(kAmpId, index);
            // if (paramQueue) {
            // 	int32 index2 = 0;
            // 	paramQueue->addPoint(data.numSamples - 1, SamplesArray.at(iCurrentEntry)->Level/2.0, index2);
            // }
            // index = 0;
            // paramQueue = outParamChanges->addParameterData(kTuneId, index);
            // if (paramQueue) {
            // 	int32 index2 = 0;
            // 	paramQueue->addPoint(data.numSamples - 1, SamplesArray.at(iCurrentEntry)->Tune>1.0?SamplesArray.at(iCurrentEntry)->Tune/2.0:SamplesArray.at(iCurrentEntry)->Tune-0.5, index2);
            // }

        }

        if (dirtyParams) {
            sceneWriter.store(data.numSamples - 1, iCurrentScene / double(EMaximumScenes - 1.)); //????
            if (SamplesArray.size() > iCurrentEntry) {
                entryWriter.store(data.numSamples - 1, iCurrentEntry / double(EMaximumSamples - 1.));
                // int32 index = 0;
                // IParamValueQueue* paramQueue =
                // 	outParamChanges->addParameterData(kCurrentEntryId, index);
                // if (paramQueue) {
                // 	int32 index2 = 0;
                // 	paramQueue->addPoint(data.numSamples - 1, (float)iCurrentEntry/(EMaximumSamples-1), index2);
                // }

                updatePadsMessage();


            }

            holdWriter.store(data.numSamples - 1, effectorSet & eHold ? 1. : 0.);
            freezeWriter.store(data.numSamples - 1, effectorSet & eFreeze ? 1. : 0.);
            vintageWriter.store(data.numSamples - 1, effectorSet & eVintage ? 1. : 0.);
            distorsionWriter.store(data.numSamples - 1, effectorSet & eDistorsion ? 1. : 0.);
            preRollWriter.store(data.numSamples - 1, effectorSet & ePreRoll ? 1. : 0.);
            postRollWriter.store(data.numSamples - 1, effectorSet & ePostRoll ? 1. : 0.);
            lockWriter.store(data.numSamples - 1, effectorSet & eLockTone ? 1. : 0.);
            punchInWriter.store(data.numSamples - 1, effectorSet & ePunchIn ? 1. : 0.);
            punchOutWriter.store(data.numSamples - 1, effectorSet & ePunchOut ? 1. : 0.);

            // PARAMETER_CHANGE (kHoldId, eHold)
            // PARAMETER_CHANGE (kFreezeId, eFreeze)
            // PARAMETER_CHANGE (kVintageId, eVintage)
            // PARAMETER_CHANGE (kDistorsionId, eDistorsion)
            // PARAMETER_CHANGE (kPreRollId, ePreRoll)
            // PARAMETER_CHANGE (kPostRollId, ePostRoll)
            // PARAMETER_CHANGE (kLockToneId, eLockTone)
            // PARAMETER_CHANGE (kPunchInId, ePunchIn)
            // PARAMETER_CHANGE (kPunchOutId, ePunchOut)
            dirtyParams = false;
        }

        if (bTCLearn != bOldTimecodeLearn) {
            tcLearnWriter.store(data.numSamples - 1, bTCLearn ? 1. :0. );
            // int32 index = 0;
            // IParamValueQueue* paramQueue =
            // outParamChanges->addParameterData(kTimecodeLearnId, index);
            // if (paramQueue) {
            // 	int32 index2 = 0;
            // 	paramQueue->addPoint(data.numSamples - 1, bTCLearn?1:0, index2);
            // }
        }
        if (fabs(fOldPosition-fPosition)>0.001) updatePositionMessage(fPosition);
        if (fabs(fOldSpeed-fRealSpeed)>0.001) updateSpeedMessage(fRealSpeed);

    }
    fVuOldL = fVuLeft;
    fVuOldR = fVuRight;

    return kResultOk;
}
// ------------------------------------------------------------------------
tresult PLUGIN_API AVinyl::setProcessing (TBool state){
    currentProcessStatus = state;
    //String tttmm;
    //tttmm = tttmm.printf("%d",state);
    //MessageBox(0,tttmm.text16(),STR("state"),MB_OK);
    return kResultTrue;
}
// ------------------------------------------------------------------------
tresult PLUGIN_API AVinyl::canProcessSampleSize (int32 symbolicSampleSize){
    if (kSample64 == symbolicSampleSize) {
        return kResultFalse;
    }
    return kResultTrue;
}
// ------------------------------------------------------------------------
uint32 PLUGIN_API AVinyl::getLatencySamples (){
    return ESpeedFrame;
}
// ------------------------------------------------------------------------
uint32 PLUGIN_API AVinyl::getTailSamples (){
    return ETimecodeLearnCount;
}

// ------------------------------------------------------------------------
void AVinyl::CalcDirectionTimeCodeAmplitude() {
    if ((StatusR) && (DeltaR < 0.0)) {
        OldStatusR = StatusR;
        StatusR = false;
        TimeCodeAmplytude = (63.0 * TimeCodeAmplytude + fabs(OldSignalR)) / 64.0;
    } else if ((!StatusR) && (DeltaR > 0.0)) {
        OldStatusR = StatusR;
        StatusR = true;
        TimeCodeAmplytude = (63.0 * TimeCodeAmplytude + fabs(OldSignalR)) / 64.0;
    }

    if ((StatusL) && (DeltaL < 0.0)) {
        OldStatusL = StatusL;
        StatusL = false;
        TimeCodeAmplytude = (63.0 * TimeCodeAmplytude + fabs(OldSignalL)) / 64.0;
    } else if ((!StatusL) && (DeltaL > 0.0)) {
        OldStatusL = StatusL;
        StatusL = true;
        TimeCodeAmplytude = (63.0 * TimeCodeAmplytude + fabs(OldSignalL)) / 64.0;
    }

    if (TimeCodeAmplytude >= ETimeCodeMinAmplytude) {
        if ((StatusL != PStatusL) || (StatusR != PStatusR) ||
            (OldStatusL != POldStatusL) || (OldStatusR != POldStatusR)) {

            if ((StatusL == PStatusR) && (StatusR ==
                                          POldStatusL) && (OldStatusL == POldStatusR) &&
                (OldStatusR == PStatusL) && (SpeedCounter >= 3)) {
                Direction3 = Direction2;
                Direction2 = Direction1;
                Direction1 = Direction0;
                Direction0 = true;
                SpeedCounter = 0;
            } else if ((StatusL == POldStatusR) && (StatusR == PStatusL) &&
                     (OldStatusL == PStatusR) && (OldStatusR == POldStatusL) &&
                     (SpeedCounter >= 3)) {
                Direction3 = Direction2;
                Direction2 = Direction1;
                Direction1 = Direction0;
                Direction0 = false;
                SpeedCounter = 0;
            }
            PStatusL = StatusL;
            PStatusR = StatusR;
            POldStatusL = OldStatusL;
            POldStatusR = OldStatusR;
        }

        if (!(Direction0 || Direction1 || Direction2 || Direction3)) {
            Direction = -1;
        } else if (Direction0 || Direction1 || Direction2 || Direction3) {
            Direction = 1;
        }

        if (SpeedCounter < 441000) {
            SpeedCounter++;
        }
    }
}

// ------------------------------------------------------------------------
void AVinyl::CalcAbsSpeed() {

    /*	Sample64 deltaF = 0;
	DWORD Cmaximums = 0;
	Sample64 maximumsX[EFFTFrame];
	Sample64 maximumsY[EFFTFrame];
	Sample64 oldmaxX = 0;
	Sample64 oldmaxY = 0;
	// Sample32 AvgSpeed;
	Sample64 AvgFrame;

	for (int i = 1; i < EFFTFrame; i++) {
		//if (i > 0) {
			if ((deltaF <= 0) && (fabs(FFT[i - 1]) - fabs(FFT[i]) > 0)) {
				if (oldmaxY > oldmaxX / 10.0) {
					maximumsX[Cmaximums] = oldmaxX;
					maximumsY[Cmaximums] = oldmaxY;
					Cmaximums++;
				}
				oldmaxX = i;
				oldmaxY = fabs(FFT[i - 1]);
			}
			deltaF = fabs(FFT[i - 1]) - fabs(FFT[i]);
		//}
	}


	//String tttmm;
	//tttmm = tttmm.printf("%d",Cmaximums);
	//MessageBox(0,tttmm.text16(),STR("speed"),MB_OK);


	Sample64 tmp = 0.0;
	Sample64 tmp1 = 0;
	for (int i = 0; i < Cmaximums; i++) {
		bool test = false;
		for (int j = 0; j < Cmaximums - 1; j++) {
			if (maximumsY[j] < maximumsY[j + 1]) {
				tmp = maximumsY[j];
				maximumsY[j] = maximumsY[j + 1];
				maximumsY[j + 1] = tmp;
				tmp1 = maximumsX[j];
				maximumsX[j] = maximumsX[j + 1];
				maximumsX[j + 1] = tmp1;
				test = true;
			}
		}
		for (int j = Cmaximums - 1; j > 1; j--) {
			if (maximumsY[j] > maximumsY[j - 1]) {
				tmp = maximumsY[j];
				maximumsY[j] = maximumsY[j - 1];
				maximumsY[j - 1] = tmp;
				tmp1 = maximumsX[j];
				maximumsX[j] = maximumsX[j - 1];
				maximumsX[j - 1] = tmp1;
				test = true;
			}
		}
		if (!test)
			break;
	}
	tmp = 0.0;
	tmp1 = 0;
	for (int i = 0; i < Cmaximums; i++) {
		if (maximumsY[i] > maximumsY[0] * 0.8) {
			tmp = tmp + maximumsX[i];
			tmp1++;
		}
		else
			break;
	}
	if (tmp1 > 0) {
		tmp = tmp / tmp1;
		if (fabs(tmp - absAVGSpeed) > 0.05) {
			absAVGSpeed = tmp;
		}
		else {
			absAVGSpeed = (2.0 * absAVGSpeed + tmp) / 3.0;
		}
	}
	else {
		absAVGSpeed = absAVGSpeed / 1.07;

	}
	*/


    int maxX = 0;
    float maxY = 0.f;

    for (int i = 0; i < EFFTFrame; i++) {
        if (maxY < fabs(FFT[i])) {
            maxY = fabs(FFT[i]);
            maxX = i;
        }
    }
    double tmp = (double)maxX;
    for (int i = maxX+1; i < maxX+3; i++) {
        if (i<EFFTFrame) {
            double koef = 100.f;
            if (FFT[i]!=0) koef = (maxY/FFT[i])*(maxY/FFT[i]);
            tmp = (koef*tmp + (double)i)/(koef+1);

        }
        else
            break;
    }
    for (int i = maxX-1; i > maxX-3; i--) {
        if (i>=0) {
            double koef = 100.f;
            if (FFT[i]!=0) koef = (maxY/FFT[i])*(maxY/FFT[i]);
            tmp = (koef*tmp + (double)i)/(koef+1);

        }
        else
            break;
    }

    if (fabs(tmp - absAVGSpeed) > 0.7f) {
        absAVGSpeed = tmp;
    } else {
        absAVGSpeed += tmp;
    }
}

// ------------------------------------------------------------------------
tresult AVinyl::receiveText(const char* text) {
    // received from Controller
    fprintf(stderr, "[Vinyl] received: ");
    fprintf(stderr, "%s", text);
    fprintf(stderr, "\n");

    // bHalfGain = !bHalfGain;

    return kResultOk;
}

// ------------------------------------------------------------------------
tresult PLUGIN_API AVinyl::setState(IBStream* state) {
    // called when we load a preset, the model has to be reloaded

    int8 byteOrder;
    if (state->read (&byteOrder, sizeof (int8)) == kResultTrue)
    {

        READ_PARAMETER_FLOAT(savedGain)
        READ_PARAMETER_FLOAT(savedVolume)
        READ_PARAMETER_FLOAT(savedPitch)
        READ_PARAMETER_DWORD(savedEntry)
        READ_PARAMETER_DWORD(savedScene)
        READ_PARAMETER_FLOAT(savedSwitch)
        READ_PARAMETER_FLOAT(savedCurve)
        READ_PARAMETER_DWORD(savedBypass)
        READ_PARAMETER_DWORD(savedEffector)
        READ_PARAMETER_FLOAT(savedTimeCodeCoeff)
        READ_PARAMETER_DWORD(savedPadCount)
        READ_PARAMETER_DWORD(savedSceneCount)
        READ_PARAMETER_DWORD(savedEntryCount)
        if (byteOrder != BYTEORDER) {
            //#if BYTEORDER == kBigEndian
            SWAP_32(savedGain)
            SWAP_32(savedVolume)
            SWAP_32(savedPitch)
            SWAP_32(savedEntry)
            SWAP_32(savedScene)
            SWAP_32(savedSwitch)
            SWAP_32(savedCurve)
            SWAP_32(savedTimeCodeCoeff)
            SWAP_32(savedBypass)
            SWAP_32(savedEffector)
            SWAP_32(savedPadCount)
            SWAP_32(savedSceneCount)
            SWAP_32(savedEntryCount)
            //#endif
        }
        fGain = savedGain;
        fVolume = savedVolume;
        fPitch = savedPitch;

        //String tttmm;
        //tttmm = tttmm.printf("FromSave Pitch %f",fPitch);
        //MessageBox(0,tttmm.text16(),STR("pos"),MB_OK);

        iCurrentEntry = savedEntry;
        iCurrentScene = savedScene;
        fSwitch = savedSwitch;
        fCurve = savedCurve;
        avgTimeCodeCoeff = savedTimeCodeCoeff;

        bBypass = savedBypass > 0;
        effectorSet = savedEffector;

        keyLockTone = (effectorSet & eLockTone) > 0;

        fRealPitch = CalcRealPitch (fPitch,fSwitch);
        fRealVolume = CalcRealVolume(fGain, fVolume, fCurve);

        //String tttmm;
        //tttmm = tttmm.printf("%d",savedSceneCount);
        //MessageBox(0,tttmm.text16(),STR("setState"),MB_OK);

        for (int j = 0; j < (int)savedSceneCount; j++)
            for (int i = 0; i < (int)savedPadCount; i++)
            {
                READ_PARAMETER_DWORD(savedType)
                READ_PARAMETER_DWORD(savedTag)
                READ_PARAMETER_DWORD(savedMidi)
                if (byteOrder != BYTEORDER) {
                    SWAP_32(savedType)
                    SWAP_32(savedTag)
                    SWAP_32(savedMidi)
                }
                if ((j < EMaximumScenes)&&(i < ENumberOfPads)) {
                    padStates[j][i].padType = PadEntry::TypePad(savedType);
                    padStates[j][i].padTag =savedTag;
                    padStates[j][i].padMidi = savedMidi;
                }
            }


        for (int i = 0; i < (int)savedEntryCount; i++) {
            READ_PARAMETER_DWORD(savedLoop)
            READ_PARAMETER_DWORD(savedReverse)
            READ_PARAMETER_DWORD(savedSync)
            READ_PARAMETER_FLOAT(savedTune)
            READ_PARAMETER_FLOAT(savedLevel)
            READ_PARAMETER_DWORD(savedNameLen)
            READ_PARAMETER_DWORD(savedFileNameLen)
            if (byteOrder != BYTEORDER) {
                SWAP_32(savedLoop)
                SWAP_32(savedReverse)
                SWAP_32(savedSync)
                SWAP_32(savedTune)
                SWAP_32(savedLevel)
                SWAP_32(savedNameLen)
                SWAP_32(savedFileNameLen)
            }
            tchar * bufname;
            bufname = (tchar *)malloc(sizeof(tchar)*(savedNameLen+1));
            tchar * buffile;
            buffile = (tchar *)malloc(sizeof(tchar)*(savedFileNameLen+1));
            state->read (bufname, sizeof(tchar)*(savedNameLen+1));
            state->read (buffile, sizeof(tchar)*(savedFileNameLen+1));
            if (byteOrder != BYTEORDER) {
                for (int32 i = 0; i < (int)savedNameLen; i++)
                {
                    SWAP_16 (bufname[i])
                }
                for (int32 i = 0; i < (int)savedFileNameLen; i++)
                {
                    SWAP_16 (buffile[i])
                }
            }
            String sName (bufname);
            String sFile (buffile);

            SamplesArray.push_back(std::make_unique<SampleEntry>(sName,sFile));
            SamplesArray.back()->SetIndex(SamplesArray.size());
            SamplesArray.back()->Loop = savedLoop > 0;
            SamplesArray.back()->Reverse = savedReverse > 0;
            SamplesArray.back()->Sync = savedSync > 0;
            SamplesArray.back()->Tune = savedTune;
            SamplesArray.back()->Level = savedLevel;

            //addSampleMessage(newSample);

            free (bufname);
            free (buffile);

        }

        for (int j = 0; j < (int)savedSceneCount; j++)
            for (int i = 0; i < (int)savedPadCount; i++)
            {
                if ((j < EMaximumScenes)&&(i < ENumberOfPads)) {
                    SetPadState(&padStates[j][i]);
                }
            }

        READ_PARAMETER_FLOAT(savedLockedSpeed)
        READ_PARAMETER_FLOAT(savedLockedVolume)
        if (byteOrder != BYTEORDER) {
            SWAP_32(savedLockedSpeed)
            SWAP_32(savedLockedVolume)
        }
        lockSpeed = savedLockedSpeed;
        lockVolume = savedLockedVolume;

        dirtyParams = true;
        return kResultOk;
    }else return kResultFalse;
}

// ------------------------------------------------------------------------
tresult PLUGIN_API AVinyl::getState(IBStream* state) {
    // here we need to save the model
    int8 byteOrder = BYTEORDER;
    if (state->write (&byteOrder, sizeof (int8)) == kResultTrue)
    {

        float toSaveGain = fGain;
        float toSaveVolume = fVolume;
        float toSavePitch = fPitch;
        DWORD toSaveEntry = iCurrentEntry;
        DWORD toSaveScene = iCurrentScene;
        float toSaveSwitch = fSwitch;
        float toSaveCurve = fCurve;
        float toSaveTimecodeCoeff = avgTimeCodeCoeff;
        DWORD toSaveBypass = bBypass ? 1 : 0;
        DWORD toSaveEffector = effectorSet;
        DWORD toSavePadCount = ENumberOfPads;
        DWORD toSaveSceneCount = EMaximumScenes;
        DWORD toSaveEntryCount = SamplesArray.size();
        float toSavelockSpeed = lockSpeed;
        float toSavelockVolume = lockVolume;

#if BYTEORDER == kBigEndian
//	SWAP_32(toSaveGain)
//	SWAP_32(toSaveVolume)
//	SWAP_32(toSavePitch)
//	SWAP_32(toSaveEntry)
//	SWAP_32(toSaveScene)
//	SWAP_32(toSaveSwitch)
//	SWAP_32(toSaveCurve)
//	SWAP_32(toSaveBypass)
//	SWAP_32(toSaveEntryCount)
#endif

        state->write(&toSaveGain, sizeof(float));
        state->write(&toSaveVolume, sizeof(float));
        state->write(&toSavePitch, sizeof(float));
        state->write(&toSaveEntry, sizeof(DWORD));
        state->write(&toSaveScene, sizeof(DWORD));
        state->write(&toSaveSwitch, sizeof(float));
        state->write(&toSaveCurve, sizeof(float));
        state->write(&toSaveBypass, sizeof(DWORD));
        state->write(&toSaveEffector, sizeof(DWORD));
        state->write(&toSaveTimecodeCoeff, sizeof(float));
        state->write(&toSavePadCount, sizeof(DWORD));
        state->write(&toSaveSceneCount, sizeof(DWORD));
        state->write(&toSaveEntryCount, sizeof(DWORD));

        for (int j = 0; j < EMaximumScenes; j++)
            for (int i = 0; i < ENumberOfPads; i++)
            {
                DWORD toSaveType = padStates[j][i].padType;
                DWORD toSaveTag = padStates[j][i].padTag;
                DWORD toSaveMidi = padStates[j][i].padMidi;
                state->write(&toSaveType, sizeof(DWORD));
                state->write(&toSaveTag, sizeof(DWORD));
                state->write(&toSaveMidi, sizeof(DWORD));
            }


        for (int i = 0; i < SamplesArray.size(); i++) {
            DWORD toSavedLoop = SamplesArray.at(i)->Loop ? 1 : 0;
            DWORD toSavedReverse = SamplesArray.at(i)->Reverse ? 1 : 0;
            DWORD toSavedSync = SamplesArray.at(i)->Sync ? 1 : 0;
            float toSavedTune = SamplesArray.at(i)->Tune;
            float toSavedLevel = SamplesArray.at(i)->Level;
            String tmpName (SamplesArray.at(i)->GetName());
            String tmpFile (SamplesArray.at(i)->GetFileName());
            DWORD toSaveNameLen = tmpName.length();
            DWORD toSaveFileNameLen = tmpFile.length();
#if BYTEORDER == kBigEndian
//		SWAP_32(toSavedLoop)
//		SWAP_32(toSavedReverse)
//		SWAP_32(toSavedSync)
//		SWAP_32(toSavedTune)
//		SWAP_32(toSavedLevel)
//		SWAP_32(toSavedLength)
#endif
            state->write(&toSavedLoop, sizeof(DWORD));
            state->write(&toSavedReverse, sizeof(DWORD));
            state->write(&toSavedSync, sizeof(DWORD));
            state->write(&toSavedTune, sizeof(float));
            state->write(&toSavedLevel, sizeof(float));
            state->write(&toSaveNameLen, sizeof(DWORD));
            state->write(&toSaveFileNameLen, sizeof(DWORD));
            state->write((void *)tmpName.text16(), (toSaveNameLen+1)*sizeof(TChar));
            state->write((void *)tmpFile.text16(), (toSaveFileNameLen+1)*sizeof(TChar));


        }

        state->write(&toSavelockSpeed, sizeof(float));
        state->write(&toSavelockVolume, sizeof(float));

        return kResultOk;
    }else return kResultFalse;
}

// ------------------------------------------------------------------------
tresult PLUGIN_API AVinyl::setupProcessing(ProcessSetup& newSetup) {
    // called before the process call, always in a disable state (not active)

    // here we keep a trace of the processing mode (offline,...) for example.
    currentProcessMode = newSetup.processMode;

    return AudioEffect::setupProcessing(newSetup);
}

// ------------------------------------------------------------------------
tresult PLUGIN_API AVinyl::setBusArrangements(SpeakerArrangement* inputs,
                                              int32 numIns, SpeakerArrangement* outputs, int32 numOuts) {
    if ((numIns == 1 || numIns == 2) && numOuts == 1) {
        if (SpeakerArr::getChannelCount(inputs[0])
                == 2 && SpeakerArr::getChannelCount(outputs[0]) == 2) {
            AudioBus* bus = FCast<AudioBus>(audioInputs.at(0));
            if (bus) {
                if (bus->getArrangement() != inputs[0]) {
                    removeAudioBusses();
                    addAudioInput(STR16("Stereo In"), inputs[0]);
                    addAudioOutput(STR16("Stereo Out"), outputs[0]);

                    // recreate the Stereo SideChain input bus
                    if ((numIns == 2) && (SpeakerArr::getChannelCount
                                          (inputs[1]) == 2))
                        addAudioInput(STR16("Stereo Aux In"), inputs[1],
                                      kAux, 0);
                }
            }
            return kResultTrue;
        }
    }
    return kResultFalse;
}

//-----------------------------------------------------------------------------
tresult PLUGIN_API AVinyl::notify (IMessage* message)
{
    if (strcmp (message->getMessageID(), "setPad") == 0)
    {
        int64 PadNumber;
        if (message->getAttributes()->getInt ("PadNumber", PadNumber) == kResultOk)
        {
            padStates[iCurrentScene][PadNumber-1].padState = false;
            int64 PadType;
            if (message->getAttributes()->getInt ("PadType", PadType) == kResultOk)
            {
                padStates[iCurrentScene][PadNumber-1].padType = PadEntry::TypePad(PadType);
                int64 PadTag;
                if (message->getAttributes()->getInt ("PadTag", PadTag) == kResultOk)
                {
                    padStates[iCurrentScene][PadNumber-1].padTag = PadTag;
                    SetPadState(&padStates[iCurrentScene][PadNumber-1]);
                    //String tttmm;
                    //tttmm = tttmm.printf("%d",PadTag);
                    //MessageBox(0,tttmm.text16(),STR("message11"),MB_OK);
                }
            }
            dirtyParams = true;
        }
        return kResultTrue;
    }
    if (strcmp (message->getMessageID(), "touchPad") == 0)
    {
        int64 PadNumber;
        if (message->getAttributes()->getInt ("PadNumber", PadNumber) == kResultOk)
        {
            //if ((padStates[iCurrentScene][PadNumber-1].padTag>=0)&&(padStates[iCurrentScene][PadNumber-1].padTag < SamplesArray.total())) {
            //	iCurrentEntry = padStates[iCurrentScene][PadNumber-1].padTag;
            //	PadSet(iCurrentEntry);
            //	dirtyParams = true;
            //}
            double PadValue;
            if (message->getAttributes()->getFloat ("PadValue", PadValue) == kResultOk)
            {
                dirtyParams |= PadWork(PadNumber,PadValue);
                //	String tttmm;
                //	tttmm = tttmm.printf("%d",PadNumber);
                //	MessageBox(0,tttmm.text16(),STR("message"),MB_OK);

            }

        }
        return kResultTrue;
    }
    if (strcmp (message->getMessageID(), "loadNewEntry") == 0)
    {
        TChar stringBuff[256] = {0};
        //TChar stringBuff2[256] = {0};
        //if (message->getAttributes()->getString ("FileName", stringBuff1, sizeof (stringBuff1) / sizeof (TChar)) == kResultOk)
        if (message->getAttributes()->getString ("File", stringBuff, sizeof (stringBuff) / sizeof (TChar)) == kResultOk)
        {
            String newFile(stringBuff);
            memset(stringBuff,0,256 * sizeof(tchar));
            if (message->getAttributes()->getString ("Sample", stringBuff, sizeof (stringBuff) / sizeof (TChar)) == kResultOk)
            {
                //MessageBox(0,STR("loadNewEntry"),STR("loadNewEntry"),MB_OK);
                String newName(stringBuff);

                SamplesArray.push_back(std::make_unique<SampleEntry>(newName,newFile));
                SamplesArray.back()->SetIndex(SamplesArray.size());
                if (iCurrentEntry == (SamplesArray.back()->GetIndex() - 1)) {
                    PadSet(iCurrentEntry);
                }
                addSampleMessage(SamplesArray.back().get());
                dirtyParams = true;
            }
        }

        return kResultTrue;
    }
    if (strcmp (message->getMessageID(), "replaceEntry") == 0)
    {
        TChar stringBuff[256] = {0};
        if (message->getAttributes()->getString ("File", stringBuff, sizeof (stringBuff) / sizeof (TChar)) == kResultOk)
        {
            String newFile(stringBuff);
            if (message->getAttributes()->getString ("Sample", stringBuff, sizeof (stringBuff) / sizeof (TChar)) == kResultOk)
            {
                //MessageBox(0,STR("loadNewEntry"),STR("loadNewEntry"),MB_OK);
                String newName(stringBuff);
                SamplesArray[iCurrentEntry] = std::make_unique<SampleEntry>(newName, newFile);
                SamplesArray[iCurrentEntry]->SetIndex(iCurrentEntry + 1);

                //dirtyParams = true;

            }
        }

        return kResultTrue;
    }
    if (strcmp (message->getMessageID(), "renameEntry") == 0)
    {
        TChar stringBuff[256] = {0};
        if (message->getAttributes()->getString ("SampleName", stringBuff, sizeof (stringBuff) / sizeof (TChar)) == kResultOk)
        {
            int64 sampleIndex;
            if (message->getAttributes()->getInt ("SampleNumber", sampleIndex) == kResultOk)
            {
                //MessageBox(0,STR("loadNewEntry"),STR("loadNewEntry"),MB_OK);
                String newName(stringBuff);
                SamplesArray.at(iCurrentEntry)->SetName(newName);
            }
        }

        return kResultTrue;
    }
    if (strcmp (message->getMessageID(), "deleteEntry") == 0)
    {
        int64 sampleIndex;
        if (message->getAttributes()->getInt ("SampleNumber", sampleIndex) == kResultOk)
        {
            delSampleMessage(SamplesArray.at(sampleIndex).get());
            SamplesArray.erase(SamplesArray.begin() + sampleIndex);
            for (int i = 0; i < SamplesArray.size(); i++) {
                SamplesArray.at(i)->SetIndex(i+1);
            }
            PadRemove(sampleIndex);
            dirtyParams = true;
        }

        return kResultTrue;
    }
    if (strcmp (message->getMessageID(), "initView") == 0)
    {
        //MessageBox(0,STR("init"),STR("init"),MB_OK);
        initSamplesMessage();
        dirtyParams = true;

        return kResultTrue;
    }
    return AudioEffect::notify (message);
}

////////////////////SampleBase Manipulation
void AVinyl::addSampleMessage(SampleEntry* newSample)
{
    if (newSample) {
        int64 sampleInt = (int64)newSample;
        IMessage* msg = allocateMessage ();
        if (msg)
        {
            //msg->setMessageID ("newEntry");
            msg->setMessageID ("addEntry");
            msg->getAttributes ()->setInt("Entry", sampleInt);
            sendMessage (msg);
            msg->release ();
        }

    }

}
void AVinyl::delSampleMessage(SampleEntry* delSample)
{
    if (delSample) {
        IMessage* msg = allocateMessage ();
        if (msg)
        {
            msg->setMessageID ("delEntry");
            msg->getAttributes ()->setInt("EntryIndex",delSample->GetIndex()-1);
            sendMessage (msg);
            msg->release ();
        }

    }

}
void AVinyl::initSamplesMessage(void)
{
    for (int32 i = 0; i < SamplesArray.size (); i++)
    {
        int64 sampleInt = int64(SamplesArray.at(i).get());
        IMessage* msg = allocateMessage ();
        if (msg) {
            msg->setMessageID ("addEntry");
            msg->getAttributes ()->setInt("Entry", sampleInt);
            sendMessage (msg);
            msg->release ();
        }

    }

}
void AVinyl::updateSpeedMessage(Sample64 speed)
{

    //MessageBox(0,STR("!speed"),STR("123"),MB_OK);
    IMessage* msg = allocateMessage ();
    if (msg) {
        msg->setMessageID ("updateSpeed");
        msg->getAttributes ()->setFloat("Speed",speed);
        sendMessage (msg);
        msg->release ();
    }

}

void AVinyl::updatePositionMessage(Sample64 _position)
{
    IMessage* msg = allocateMessage ();
    if (msg) {
        msg->setMessageID ("updatePosition");
        msg->getAttributes ()->setFloat("Position",_position);
        sendMessage (msg);
        msg->release ();
    }

}

void AVinyl::updatePadsMessage(void)
{
    IMessage* msg = allocateMessage ();
    if (msg) {
        msg->setMessageID ("updatePads");
        /*	msg->getAttributes ()->setInt("PadState00",padStates[iCurrentScene][0].padState?1:0);
			msg->getAttributes ()->setInt("PadState01",padStates[iCurrentScene][1].padState?1:0);
			msg->getAttributes ()->setInt("PadState02",padStates[iCurrentScene][2].padState?1:0);
			msg->getAttributes ()->setInt("PadState03",padStates[iCurrentScene][3].padState?1:0);
			msg->getAttributes ()->setInt("PadState04",padStates[iCurrentScene][4].padState?1:0);
			msg->getAttributes ()->setInt("PadState05",padStates[iCurrentScene][5].padState?1:0);
			msg->getAttributes ()->setInt("PadState06",padStates[iCurrentScene][6].padState?1:0);
			msg->getAttributes ()->setInt("PadState07",padStates[iCurrentScene][7].padState?1:0);
			msg->getAttributes ()->setInt("PadState08",padStates[iCurrentScene][8].padState?1:0);
			msg->getAttributes ()->setInt("PadState09",padStates[iCurrentScene][9].padState?1:0);
			msg->getAttributes ()->setInt("PadState10",padStates[iCurrentScene][10].padState?1:0);
			msg->getAttributes ()->setInt("PadState11",padStates[iCurrentScene][11].padState?1:0);
			msg->getAttributes ()->setInt("PadState12",padStates[iCurrentScene][12].padState?1:0);
			msg->getAttributes ()->setInt("PadState13",padStates[iCurrentScene][13].padState?1:0);
			msg->getAttributes ()->setInt("PadState14",padStates[iCurrentScene][14].padState?1:0);
			msg->getAttributes ()->setInt("PadState15",padStates[iCurrentScene][15].padState?1:0);

			msg->getAttributes ()->setInt("PadType00",padStates[iCurrentScene][0].padType);
			msg->getAttributes ()->setInt("PadType01",padStates[iCurrentScene][1].padType);
			msg->getAttributes ()->setInt("PadType02",padStates[iCurrentScene][2].padType);
			msg->getAttributes ()->setInt("PadType03",padStates[iCurrentScene][3].padType);
			msg->getAttributes ()->setInt("PadType04",padStates[iCurrentScene][4].padType);
			msg->getAttributes ()->setInt("PadType05",padStates[iCurrentScene][5].padType);
			msg->getAttributes ()->setInt("PadType06",padStates[iCurrentScene][6].padType);
			msg->getAttributes ()->setInt("PadType07",padStates[iCurrentScene][7].padType);
			msg->getAttributes ()->setInt("PadType08",padStates[iCurrentScene][8].padType);
			msg->getAttributes ()->setInt("PadType09",padStates[iCurrentScene][9].padType);
			msg->getAttributes ()->setInt("PadType10",padStates[iCurrentScene][10].padType);
			msg->getAttributes ()->setInt("PadType11",padStates[iCurrentScene][11].padType);
			msg->getAttributes ()->setInt("PadType12",padStates[iCurrentScene][12].padType);
			msg->getAttributes ()->setInt("PadType13",padStates[iCurrentScene][13].padType);
			msg->getAttributes ()->setInt("PadType14",padStates[iCurrentScene][14].padType);
			msg->getAttributes ()->setInt("PadType15",padStates[iCurrentScene][15].padType);

			msg->getAttributes ()->setInt("padTag00",padStates[iCurrentScene][0].padTag);
			msg->getAttributes ()->setInt("padTag01",padStates[iCurrentScene][1].padTag);
			msg->getAttributes ()->setInt("padTag02",padStates[iCurrentScene][2].padTag);
			msg->getAttributes ()->setInt("padTag03",padStates[iCurrentScene][3].padTag);
			msg->getAttributes ()->setInt("padTag04",padStates[iCurrentScene][4].padTag);
			msg->getAttributes ()->setInt("padTag05",padStates[iCurrentScene][5].padTag);
			msg->getAttributes ()->setInt("padTag06",padStates[iCurrentScene][6].padTag);
			msg->getAttributes ()->setInt("padTag07",padStates[iCurrentScene][7].padTag);
			msg->getAttributes ()->setInt("padTag08",padStates[iCurrentScene][8].padTag);
			msg->getAttributes ()->setInt("padTag09",padStates[iCurrentScene][9].padTag);
			msg->getAttributes ()->setInt("padTag10",padStates[iCurrentScene][10].padTag);
			msg->getAttributes ()->setInt("padTag11",padStates[iCurrentScene][11].padTag);
			msg->getAttributes ()->setInt("padTag12",padStates[iCurrentScene][12].padTag);
			msg->getAttributes ()->setInt("padTag13",padStates[iCurrentScene][13].padTag);
			msg->getAttributes ()->setInt("padTag14",padStates[iCurrentScene][14].padTag);
			msg->getAttributes ()->setInt("padTag15",padStates[iCurrentScene][15].padTag); */
        String tmp;
        String ttmp1[ENumberOfPads];
        String ttmp2[ENumberOfPads];
        String ttmp3[ENumberOfPads];
        for (int i = 0; i < ENumberOfPads; i++) {
            ttmp1[i] = tmp.printf("PadState%02d",i);
            msg->getAttributes ()->setInt(ttmp1[i].text8(),padStates[iCurrentScene][i].padState?1:0);
            ttmp2[i] = tmp.printf("PadType%02d",i);
            msg->getAttributes ()->setInt(ttmp2[i].text8(),padStates[iCurrentScene][i].padType);
            ttmp3[i] = tmp.printf("PadTag%02d",i);
            msg->getAttributes ()->setInt(ttmp3[i].text8(),padStates[iCurrentScene][i].padTag);
        }
        sendMessage (msg);
        msg->release ();
    }

}

void AVinyl::processEvent(const Event &event) {
    switch (event.type) {
    case Event::kNoteOnEvent:
        for (int i = 0; i < ENumberOfPads; i++) {
            if (padStates[iCurrentScene][i].padMidi == event.noteOn.pitch) {
                dirtyParams |= PadWork(i, 1.0);
            }
            if (padStates[iCurrentScene][i].padType == PadEntry::kAssigMIDI) {
                padStates[iCurrentScene][i].padType = PadEntry::kSamplePad;
                padStates[iCurrentScene][i].padMidi = event.noteOn.pitch;
                dirtyParams = true;
            }
        }

        break;
    case Event::kNoteOffEvent:
        for (int i = 0; i < ENumberOfPads; i++) {
            if (padStates[iCurrentScene][i].padMidi == event.noteOn.pitch)
                dirtyParams |= PadWork(i, 0);
        }
        break;
    default:
        break;
    }
}

void AVinyl::reset(bool state) {

    if (state) {
        // ---Reset filters------
        SCursor = 0;
        FCursor = EFilterFrame - 1;
        FFTCursor = 0;
        FSignalL = 0;
        FSignalR = 0;
        for (int i = 0; i < EFilterFrame; i++) {
            SignalL[i] = 0;
            SignalR[i] = 0;
        }
        absAVGSpeed = 0;
        Direction = 1;
        DeltaL = 0.;
        DeltaR = 0.;
        OldSignalL = 0;
        OldSignalR = 0;
        filtredL = 0.;
        filtredR = 0.;
        TimeCodeAmplytude = 0;
        HoldCounter = 0;
        FreezeCounter = 0;

    } else {
        // reset the VuMeter value
        fVuOldL = 0.f;
        fVuOldR = 0.f;
    }
    softVolume = 0;
    softPreRoll = 0;
    softPostRoll = 0;
    softDistorsion = 0;
    softHold = 0;
    softFreeze = 0;
    softVintage = 0;
}

///////////////////////////////////////////////////////////////////////////////
void AVinyl::SetCurrentEntry(int _newentry)
{
    if ((_newentry < SamplesArray.size()) && (_newentry >= 0)) {
        //MessageBox(0,STR("!!!"),STR("pos"),MB_OK);
        iCurrentEntry = _newentry;
        SamplesArray.at(_newentry)->ResetCursor();
        fPosition = 0;
        //lBeatCount = SamplesArray.at(_newentry)->GetACIDbeats();
        //beatCue = 0L;
    }
}

///////////////////////CalcFunctions
Sample32 AVinyl::CalcRealPitch(Sample32 _normalPitch, Sample32 _normalKoeff)
{
    if (( _normalKoeff == 0 )||(_normalPitch == 0.5 )) {
        return 1.0f;
    }
    else {
        Sample32 multipleKoeff;
        if (_normalKoeff<=0.5) {
            multipleKoeff = 0.2;
        }else multipleKoeff = 1.;
        if (_normalPitch > 0.5)
            return  (_normalPitch - 0.5) * multipleKoeff + 1.0;
        else
            return  1.0 - (0.5 - _normalPitch) * multipleKoeff;
    }
}

Sample32 AVinyl::CalcRealVolume(Sample32 _normalGain, Sample32 _normalVolume, Sample32 _normalKoeff)
{
    if ( _normalKoeff == 0 ) {
        return _normalVolume * _normalGain;
    }
    else {
        if (_normalKoeff<=0.5) {
            return _normalVolume<0.5?_normalVolume*2.0 * _normalGain:_normalGain;
        }else
            return _normalVolume<0.2?sqrt(_normalVolume*5.0) * _normalGain:_normalGain;

    }
}
////////////////////////PadManipulations
bool AVinyl::PadWork(int _padId, Sample32 _paramValue)
{
    /////////////
    bool result = false;
    switch(padStates[iCurrentScene][_padId].padType){
    case PadEntry::kSamplePad:
        if ((padStates[iCurrentScene][_padId].padTag>=0)&&(SamplesArray.size () > padStates[iCurrentScene][_padId].padTag)) {
            if (_paramValue>0.5) {
                for (int j = 0; j < EMaximumScenes; j++)
                    for (int i = 0; i < ENumberOfPads; i++) {
                        if (padStates[j][i].padType == PadEntry::kSamplePad) {
                            padStates[j][i].padState = false;
                        }
                    }
                padStates[iCurrentScene][_padId].padState = true;
                //iCurrentEntry = padStates[iCurrentScene][_padId].padTag;
                SetCurrentEntry (padStates[iCurrentScene][_padId].padTag);
                result = true;
            }
        }
        break;
    case PadEntry::kSwitchPad:
    {
        //int addEffect = 1 << padStates[iCurrentScene][_padId].padTag;
        if (_paramValue>0.5) {
            padStates[iCurrentScene][_padId].padState = !padStates[iCurrentScene][_padId].padState;
            //effectorSet ^= addEffect;
            effectorSet ^= padStates[iCurrentScene][_padId].padTag;
            result = true;
        }
    }
    break;
    case PadEntry::kKickPad:
    {
        //int addEffect = 1 << padStates[iCurrentScene][_padId].padTag;
        if (_paramValue>0.5) {
            padStates[iCurrentScene][_padId].padState = true;
            //effectorSet |= addEffect;
            effectorSet |= padStates[iCurrentScene][_padId].padTag;
        }else{
            padStates[iCurrentScene][_padId].padState = false;
            //effectorSet &= ~addEffect;
            effectorSet &= ~padStates[iCurrentScene][_padId].padTag;
        }
        result = true;
    }
    break;
    default:
        break;
    }
    return result;

}

bool AVinyl::PadSet(int _currentSample)
{
    /////////////
    bool result = false;
    for (int j = 0; j < EMaximumScenes; j++)
        for (int i = 0; i < ENumberOfPads; i++) {
            if (padStates[j][i].padType == PadEntry::kSamplePad) {
                if (padStates[j][i].padTag == _currentSample) {
                    padStates[j][i].padState = true;
                }else{
                    padStates[j][i].padState = false;
                }
                result = true;
            }
        }
    return result;
}

void AVinyl::SetPadState(PadEntry * _Pad)
{
    /////////////
    switch(_Pad->padType){
    case PadEntry::kSamplePad:
        if ((_Pad->padTag == (int)iCurrentEntry)&&(SamplesArray.size ()>0)) _Pad->padState = true; else _Pad->padState = false;
        break;
    case PadEntry::kSwitchPad:
        //case PadEntry::kKickPad:
        if (_Pad->padTag & effectorSet) _Pad->padState = true; else _Pad->padState = false;
        break;
    default:
        _Pad->padState = false;
    }
}

bool AVinyl::PadRemove(int _currentSample)
{
    /////////////
    bool result = false;
    for (int j = 0; j < EMaximumScenes; j++)
        for (int i = 0; i < ENumberOfPads; i++) {
            if (padStates[j][i].padType == PadEntry::kSamplePad) {
                if (padStates[j][i].padTag == _currentSample) {
                    padStates[j][i].padTag = -1;
                }else
                    if (padStates[j][i].padTag > _currentSample) {
                        padStates[j][i].padTag = padStates[j][i].padTag-1;
                    }
                result = true;
            }
        }
    return result;
}

float AVinyl::GetNormalizeTag(int _tag)
{
    float padValue;
    if (padStates[iCurrentScene][_tag].padTag <0 ) return 0;
    switch (padStates[iCurrentScene][_tag].padType) {
    case PadEntry::kSamplePad:
        if (padStates[iCurrentScene][_tag].padTag>=SamplesArray.size()) return 0;
        padValue = (padStates[iCurrentScene][_tag].padTag+1.0)/(SamplesArray.size());
        break;
    case PadEntry::kSwitchPad:
        if (padStates[iCurrentScene][_tag].padTag>=5) return 0;
        padValue = (padStates[iCurrentScene][_tag].padTag+1.0)/5.0;
        break;
    case PadEntry::kKickPad:
        if (padStates[iCurrentScene][_tag].padTag>=5) return 0;
        padValue = (padStates[iCurrentScene][_tag].padTag+1.0)/5.0;
        break;
    default:
        break;
    }
    return padValue;
}

//------------------------------------------------------------------------
Sample32 AVinyl::aproxParamValue(int32 currentSampleIndex,double currentValue, int32 nextSampleIndex, double nextValue)
{
    double slope = (nextValue - currentValue) / (double)(nextSampleIndex - currentSampleIndex + 1);
    double offset = currentValue - (slope * (double)(currentSampleIndex - 1));
    return (slope * (double)currentSampleIndex) + offset; // bufferTime is any position in buffer
}

}} // namespaces
