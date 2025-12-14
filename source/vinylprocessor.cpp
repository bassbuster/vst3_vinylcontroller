#pragma hdrstop

#include "vinylprocessor.h"
#include "vinylparamids.h"
#include "vinylcids.h"	// for class ids

#include "pluginterfaces/base/ibstream.h"
#include "base/source/fstring.h"
#include "base/source/fstreamer.h"

#include <stdio.h>
#include <cmath>
#include "helpers/parameterwriter.h"
#include "helpers/fft.h"

namespace {

inline double sqr(double x) {
    return x * x;
}

constexpr double Pi = 3.14159265358979323846264338327950288;

inline float aproxParamValue(int32_t currentSampleIndex, double currentValue, int32_t nextSampleIndex, double nextValue)
{
    double slope = (nextValue - currentValue) / double(nextSampleIndex - currentSampleIndex + 1);
    double offset = currentValue - (slope * double(currentSampleIndex - 1));
    return (slope * double(currentSampleIndex)) + offset; // bufferTime is any position in buffer
}

inline float calcRealPitch(float normalPitch, float normalKoeff)
{
    if ((normalKoeff != 0.) && (normalPitch != 0.5)) {
        float multipleKoeff;
        if (normalKoeff <= 0.5) {
            multipleKoeff = 0.2;
        } else {
            multipleKoeff = 1.;
        }
        if (normalPitch > 0.5) {
            return  (normalPitch - 0.5) * multipleKoeff + 1.;
        }
        return  1. - (0.5 - normalPitch) * multipleKoeff;
    }
    return 1.0f;
}

inline float calcRealVolume(float normalGain, float normalVolume, float normalKoeff)
{
    if (normalKoeff != 0.) {
        if (normalKoeff <= 0.5) {
            return normalVolume < 0.5 ? normalVolume * 2. * normalGain : normalGain;
        }
        return normalVolume < 0.2 ? sqrt(normalVolume * 5.) * normalGain : normalGain;
    }
    return normalVolume * normalGain;
}

}

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

    VintageSample = std::make_unique<SampleEntry<Sample32>>("vintage", "c:\\Work\\vst3sdk\\build\\VST3\\Debug\\vinylcontroller.vst3\\Contents\\Resources\\vintage.wav");
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
                         fRealVolume = calcRealVolume(fGain, fVolume, fCurve); },
                     [this] (int32 sampleOffset, int32 currentOffset, Sample64 currentValue) {
                         fGain = aproxParamValue(sampleOffset, fGain, currentOffset, currentValue);
                         fRealVolume = calcRealVolume(fGain, fVolume, fCurve);
                     });
    params.addReader(kVolumeId, [this] () { return fVolume; },
                     [this](Sample64 value) {
                         fVolume = value;
                         fRealVolume = calcRealVolume(fGain, fVolume, fCurve); },
                     [this] (int32 sampleOffset, int32 currentOffset, Sample64 currentValue) {
                         fVolume = aproxParamValue(sampleOffset, fVolume, currentOffset, currentValue);
                         fRealVolume = calcRealVolume(fGain, fVolume, fCurve);
                     });
    params.addReader(kVolCurveId, [this] () { return fCurve; },
                     [this](Sample64 value) {
                         fCurve = value;
                         fRealVolume = calcRealVolume(fGain, fVolume, fCurve); },
                     [this] (int32 sampleOffset, int32 currentOffset, Sample64 currentValue) {
                         fCurve = aproxParamValue(sampleOffset, fCurve, currentOffset, currentValue);
                         fRealVolume = calcRealVolume(fGain, fVolume, fCurve);
                     });
    params.addReader(kPitchId, [this] () { return fPitch; },
                     [this](Sample64 value) { fPitch = value; fRealPitch = calcRealPitch (fPitch, fSwitch); },
                     [this] (int32 sampleOffset, int32 currentOffset, Sample64 currentValue) {
                         fPitch = aproxParamValue(sampleOffset, fPitch, currentOffset, currentValue);
                         fRealPitch = calcRealPitch (fPitch, fSwitch);
                     });
    params.addReader(kPitchSwitchId, [this] () { return fSwitch; },
                     [this](Sample64 value) {
                         fSwitch = value;
                         fRealPitch = calcRealPitch (fPitch, fSwitch); },
                     [this] (int32 sampleOffset, int32 currentOffset, Sample64 currentValue) {
                         fSwitch = aproxParamValue(sampleOffset, fSwitch, currentOffset, currentValue);
                         fRealPitch = calcRealPitch (fPitch, fSwitch);
                     });

    params.addReader(kCurrentEntryId, [this] () { return double(iCurrentEntry / (EMaximumSamples - 1)); },
                     [this](Sample64 value) {
                         SetCurrentEntry(floor(value * double(EMaximumSamples - 1) + 0.5));
                         dirtyParams |= PadSet(iCurrentEntry);
                     });
    params.addReader(kCurrentSceneId, [this] () { return double(iCurrentScene / (EMaximumScenes - 1)); },
                     [this](Sample64 value) {
                         iCurrentScene = floor(value * float(EMaximumScenes - 1) + 0.5f);
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
            padStates[j][i].padType = PadEntry::SamplePad;
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

inline Sample64 noteLengthInSamples(Sample64 note, Sample64 tempo, Sample64 sampleRate) {
    if ((tempo > 0) && (note > 0)) {
        return sampleRate / tempo * 60.0 * note;
    }
    return 0;

}

// ------------------------------------------------------------------------
tresult PLUGIN_API AVinyl::process(ProcessData& data) {


    bool samplesParamsUpdate = false;
    if (data.processContext) {
        dSampleRate = data.processContext->sampleRate;
        dTempo = data.processContext->tempo;
        dNoteLength = noteLengthInSamples(ERollNote,dTempo,dSampleRate);
    }

    IParameterChanges* paramChanges = data.inputParameterChanges;
    IParameterChanges* outParamChanges = data.outputParameterChanges;
    IEventList* eventList = data.inputEvents;

    params.setQueue(paramChanges);

    ParameterWriter vuLeftWriter(kVuLeftId, outParamChanges);
    ParameterWriter vuRightWriter(kVuRightId, outParamChanges);
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

    Event event;
    Event *eventP = nullptr;
    int32 eventIndex = 0;

    Finalizer readTheRest([&](){

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
    bool bOldTimecodeLearn = bTCLearn;
    Sample64 fOldSpeed = fRealSpeed;

    if (numChannels >= 2) {


        int32 sampleFrames = data.numSamples;
        int32 sampleOffset = 0;
        Sample32* ptrInLeft = in[0];
        Sample32* ptrOutLeft = out[0];
        Sample32* ptrInRight = in[1];
        Sample32* ptrOutRight = out[1];

        while (--sampleFrames >= 0) {

            params.checkOffset(sampleOffset);

            ////////////Get Offsetted Events ////////////////////////////////////
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
            /////////////////////////////////////////////////////////////////////

            if (!bBypass) {


                Sample32 inL = *ptrInLeft++;
                Sample32 inR = *ptrInRight++;

                SignalL[FCursor] = (filtredL += inL);
                SignalR[FCursor] = (filtredL += inR);

                SignalL[SCursor] = SignalL[SCursor] - (FSignalL += inL);
                SignalR[SCursor] = SignalR[SCursor] - (FSignalR += inR);

                DeltaL += (SignalL[SCursor] - OldSignalL);
                DeltaR += (SignalR[SCursor] - OldSignalR);

                CalcDirectionTimeCodeAmplitude();

                OldSignalL = SignalL[SCursor];
                OldSignalR = SignalR[SCursor];

                if (++FCursor >= EFilterFrame) {
                    FCursor = 0;
                }

                if (++SCursor >= EFilterFrame) {
                    SCursor = 0;
                }


//                Sample64 SmoothCoef = 0.5 - 0.5 * cos((2. * Pi * Sample64(FFTCursor + EFFTFrame - ESpeedFrame) / Sample64(EFFTFrame)));
                Sample64 SmoothCoef = sin(Pi * Sample64(FFTCursor + EFFTFrame - ESpeedFrame) / Sample64(EFFTFrame));
                FFTPre[FFTCursor + EFFTFrame - ESpeedFrame] = (OldSignalL - OldSignalR);
                FFT[FFTCursor + EFFTFrame - ESpeedFrame] = (SmoothCoef * FFTPre[FFTCursor + EFFTFrame - ESpeedFrame]);

                FFTCursor++;
                if (FFTCursor >= ESpeedFrame) {
                    FFTCursor = 0;

                    if (TimeCodeAmplytude >= ETimeCodeMinAmplytude) {
                        {
                            auto debugInput = new SampleEntry<Sample32>("dbg", FFT, FFT, EFFTFrame);
                            debugInputMessage(debugInput);
                        }
                        fastsinetransform(FFT, EFFTFrame);
                        {
                            auto debugFft = new SampleEntry<Sample32>("dbg", FFT, FFT, EFFTFrame);
                            debugFftMessage(debugFft);
                        }

                        CalcAbsSpeed();
                        if (TimecodeLearnCounter > 0) {
                            TimecodeLearnCounter--;
                            avgTimeCodeCoeff += (Direction * absAVGSpeed);
                            fRealSpeed = 1.;
                        } else {
                            fRealSpeed = absAVGSpeed / avgTimeCodeCoeff;
                            bTCLearn = false;
                        }
                        fVolCoeff += sqrt(fabs(fRealSpeed));
                        fRealSpeed = (Sample64)Direction * fRealSpeed;
                    }

                    for (int i = 0; i < EFFTFrame-ESpeedFrame; i++) {
                        Sample64 SmoothCoef = 0.5 - 0.5 * cos((2.0 * Pi * Sample64(i) / EFFTFrame));
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
                        absAVGSpeed = 0.;
                        fRealSpeed = 0.;
                        fVolCoeff = 0.;
                    }

                }


                if ((fVolCoeff >= 0.00001) && (SamplesArray.size() > 0)) {

                    ////////////////////EFFECTOR BEGIN//////////////////////////////

                    softVolume += ((effectorSet & ePunchIn) ? (fGain * fVolCoeff) :
                                       ((effectorSet & ePunchOut) ? 0. : (fRealVolume * fVolCoeff)));
                    softPreRoll += (effectorSet & ePreRoll ? 1. : 0.);
                    softPostRoll += (effectorSet & ePostRoll ? 1. : 0.);
                    softDistorsion += (effectorSet & eDistorsion ? 1. : 0.);
                    softVintage += (effectorSet & eVintage ? 1. : 0.);

                    if (effectorSet & eHold) {
                        if (!keyHold) {
                            HoldCue = SamplesArray.at(iCurrentEntry)->cue();
                            keyHold = true;
                        }
                        softHold += 1.;
                    } else {
                        keyHold = false;
                        softHold += 0.;
                    }

                    if (effectorSet & eFreeze) {
                        if (!keyFreeze) {
                            FreezeCue = SamplesArray.at(iCurrentEntry)->cue();
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
                            SamplesArray.at(iCurrentEntry)->beginLockStrobe();
                        }
                        fVolCoeff = lockVolume;
                    } else {
                        keyLockTone = false;
                    }

                    //////////////////////////PLAYER////////////////////////////////////////////////
                    Sample64 _speed = 0.;//fRealSpeed * fRealPitch;
                    Sample64 _tempo = 0.;

                    if (keyLockTone) {
                        _speed = (Sample64)Direction * lockSpeed;
                        _tempo = SamplesArray.at(iCurrentEntry)->Sync
                                     ? fabs(dTempo * fRealSpeed * fRealPitch)
                                     : SamplesArray.at(iCurrentEntry)->tempo() * fabs(fRealSpeed * fRealPitch) * lockTune;
                        SamplesArray.at(iCurrentEntry)->playStereoSampleTempo(ptrOutLeft,
                                                                              ptrOutRight,
                                                                              _speed,
                                                                              _tempo,
                                                                              dSampleRate,
                                                                              true);
                    } else {
                        _speed = fRealSpeed * fRealPitch;
                        _tempo = dTempo;
                        SamplesArray.at(iCurrentEntry)->playStereoSample(ptrOutLeft,
                                                                         ptrOutRight,
                                                                         _speed,
                                                                         _tempo,
                                                                         dSampleRate,
                                                                         true);
                    }

                    if (keyFreeze) {
                        FreezeCounter++;
                        bool pushSync = SamplesArray.at(iCurrentEntry)->Sync;
                        SamplesArray.at(iCurrentEntry)->Sync = false;
                        auto PushCue = SamplesArray.at(iCurrentEntry)->cue();
                        SamplesArray.at(iCurrentEntry)->cue(FreezeCueCur);
                        SamplesArray.at(iCurrentEntry)->playStereoSample(ptrOutLeft,
                                                                         ptrOutRight,
                                                                         _speed,
                                                                         _tempo,
                                                                         dSampleRate,
                                                                         true);
                        FreezeCueCur =  SamplesArray.at(iCurrentEntry)->cue();
                        if ((softFreeze>0.00001)&&(softFreeze<0.99)) {
                            Sample32 fLeft = 0;
                            Sample32 fRight = 0;
                            SamplesArray.at(iCurrentEntry)->cue(AfterFreezeCue);
                            SamplesArray.at(iCurrentEntry)->playStereoSample(&fLeft,
                                                                             &fRight,
                                                                             _speed,
                                                                             _tempo,
                                                                             dSampleRate,
                                                                             true);
                            *ptrOutLeft = *ptrOutLeft * softFreeze + fLeft * (1.0 - softFreeze);
                            *ptrOutRight = *ptrOutRight * softFreeze + fRight * (1.0 - softFreeze);
                            AfterFreezeCue = SamplesArray.at(iCurrentEntry)->cue();
                        }
                        if (_speed != 0.0){
                            if (FreezeCounter >= dNoteLength) {
                                FreezeCounter = 0;
                                AfterFreezeCue = FreezeCueCur;
                                FreezeCueCur = FreezeCue;
                                softFreeze = 0;
                            }
                        }
                        SamplesArray.at(iCurrentEntry)->Sync = pushSync;
                        SamplesArray.at(iCurrentEntry)->cue(PushCue);
                    }

                    if (keyHold) {
                        HoldCounter++;
                        if ((softHold>0.00001)&&(softHold<0.99)) {
                            Sample32 fLeft = 0;
                            Sample32 fRight = 0;
                            auto PushCue = SamplesArray.at(iCurrentEntry)->cue();
                            SamplesArray.at(iCurrentEntry)->cue(AfterHoldCue);
                            SamplesArray.at(iCurrentEntry)->playStereoSample(&fLeft,
                                                                             &fRight,
                                                                             _speed,
                                                                             SamplesArray.at(iCurrentEntry)->tempo(),
                                                                             dSampleRate,
                                                                             true);
                            *ptrOutLeft = *ptrOutLeft * softHold + fLeft * (1. - softHold);
                            *ptrOutRight = *ptrOutRight * softHold + fRight * (1. - softHold);
                            AfterHoldCue = SamplesArray.at(iCurrentEntry)->cue();
                            SamplesArray.at(iCurrentEntry)->cue(PushCue);
                        }
                        if (HoldCounter >= dNoteLength) {
                            AfterHoldCue = SamplesArray.at(iCurrentEntry)->cue();
                            SamplesArray.at(iCurrentEntry)->cue(HoldCue);
                            HoldCounter = 0;
                            softHold = 0;
                        }
                    }
                    ////////////////////////////////////////////////////////////////////////////

                    if ((softPreRoll > 0.0001) || (softPostRoll > 0.0001)) {

                        Sample64 offset = SamplesArray.at(iCurrentEntry)->getNoteLength(ERollNote, dTempo);
                        Sample64 pre_offset = offset;
                        Sample64 pos_offset = -offset;

                        Sample32 fLeft = 0;
                        Sample32 fRight = 0;
                        for (int i = 0; i < ERollCount; i++) {
                            Sample32 rollVolume = (1. -Sample32(i) / Sample32(ERollCount)) * .6;
                            if (softPreRoll > 0.0001) {
                                SamplesArray.at(iCurrentEntry)->playStereoSample(&fLeft, &fRight, pos_offset, false);
                                *ptrOutLeft = *ptrOutLeft * (1.0 - softPreRoll * 0.1) + fLeft * rollVolume * softPreRoll;
                                *ptrOutRight = *ptrOutRight * (1.0 - softPreRoll * 0.1) + fRight * rollVolume * softPreRoll;
                                pos_offset = pos_offset + offset;
                            }
                            if (softPostRoll > 0.0001) {
                                SamplesArray.at(iCurrentEntry)->playStereoSample(&fLeft, &fRight, pre_offset, false);
                                *ptrOutLeft = *ptrOutLeft * (1.0 - softPostRoll * 0.1) + fLeft * rollVolume * softPostRoll;
                                *ptrOutRight = *ptrOutRight * (1.0 - softPostRoll * 0.1) + fRight * rollVolume * softPostRoll;
                                pre_offset = pre_offset - offset;
                            }
                        }
                    }

                    if (softDistorsion > 0.0001) {
                        if (*ptrOutLeft > 0) {
                            *ptrOutLeft = *ptrOutLeft * (1.0 - softDistorsion * 0.6) + 0.4 * sqrt(*ptrOutLeft) * softDistorsion;
                        } else {
                            *ptrOutLeft = *ptrOutLeft * (1.0 - softDistorsion * 0.6) - 0.3 * sqrt(-*ptrOutLeft) * softDistorsion;
                        }
                        if (*ptrOutRight>0) {
                            *ptrOutRight = *ptrOutRight * (1.0 - softDistorsion * 0.6) + 0.4 * sqrt(*ptrOutRight) * softDistorsion;
                        } else {
                            *ptrOutRight = *ptrOutRight * (1.0 - softDistorsion * 0.6) - 0.3 * sqrt(-*ptrOutRight) * softDistorsion;
                        }
                    }

                    if (softVintage > 0.0001) {
                        Sample32 VintageLeft = 0;
                        Sample32 VintageRight = 0;
                        if (VintageSample) {
                            VintageSample->playStereoSample(&VintageLeft,&VintageRight, _speed, dTempo, dSampleRate, true);
                        }
                        *ptrOutLeft = *ptrOutLeft * (1. - softVintage * .3) + (-sqr(*ptrOutRight * .5) + VintageLeft) * softVintage;
                        *ptrOutRight = *ptrOutRight * (1. - softVintage * .3)  + (-sqr(*ptrOutLeft * .5) + VintageRight) * softVintage;
                    }

                    /////////////////////////END OF EFFECTOR//////////////////


                    *ptrOutLeft = *ptrOutLeft * softVolume;
                    *ptrOutRight = *ptrOutRight * softVolume;

                    fPosition = (float)SamplesArray.at(iCurrentEntry)->cue().integerPart() / SamplesArray.at(iCurrentEntry)->bufferLength();
                    if (*ptrOutLeft > fVuLeft) {
                        fVuLeft = *ptrOutLeft;
                    }
                    if (*ptrOutRight > fVuRight) {
                        fVuRight = *ptrOutRight;
                    }

                } else {
                    *ptrOutLeft = 0;
                    *ptrOutRight = 0;
                }

            } else {
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

        if ((samplesParamsUpdate || dirtyParams) && (SamplesArray.size() > iCurrentEntry)){

            loopWriter.store(data.numSamples - 1, SamplesArray.at(iCurrentEntry)->Loop ? 1. : 0.);
            syncWriter.store(data.numSamples - 1, SamplesArray.at(iCurrentEntry)->Sync ? 1. : 0.);
            levelWriter.store(data.numSamples - 1, SamplesArray.at(iCurrentEntry)->Level / 2.);
            reverseWriter.store(data.numSamples - 1, SamplesArray.at(iCurrentEntry)->Reverse ? 1. : 0.);
            tuneWriter.store(data.numSamples - 1, SamplesArray.at(iCurrentEntry)->Tune > 1. ? SamplesArray.at(iCurrentEntry)->Tune / 2. : SamplesArray.at(iCurrentEntry)->Tune - 0.5);
        }

        if (dirtyParams) {
            sceneWriter.store(data.numSamples - 1, iCurrentScene / double(EMaximumScenes - 1.)); //????
            if (SamplesArray.size() > iCurrentEntry) {
                entryWriter.store(data.numSamples - 1, iCurrentEntry / double(EMaximumSamples - 1.));
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

            dirtyParams = false;
        }

        if (bTCLearn != bOldTimecodeLearn) {
            tcLearnWriter.store(data.numSamples - 1, bTCLearn ? 1. : 0.);
        }
        if (fabs(fOldPosition-fPosition) > 0.001) {
            updatePositionMessage(fPosition);
        }
        if (fabs(fOldSpeed-fRealSpeed) > 0.001) {
            updateSpeedMessage(fRealSpeed);
        }
    }
    fVuOldL = fVuLeft;
    fVuOldR = fVuRight;

    return kResultOk;
}

tresult PLUGIN_API AVinyl::setProcessing (TBool state){
    currentProcessStatus = state;
    return kResultTrue;
}

tresult PLUGIN_API AVinyl::canProcessSampleSize (int32 symbolicSampleSize){
    if (kSample64 == symbolicSampleSize) {
        return kResultFalse;
    }
    return kResultTrue;
}

uint32 PLUGIN_API AVinyl::getLatencySamples (){
    return ESpeedFrame;
}

uint32 PLUGIN_API AVinyl::getTailSamples (){
    return ETimecodeLearnCount;
}

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

void AVinyl::CalcAbsSpeed() {

    int maxX = 0;
    float maxY = 0.f;

    for (int i = 0; i < EFFTFrame; i++) {
        if (maxY < fabs(FFT[i])) {
            maxY = fabs(FFT[i]);
            maxX = i;
        }
    }

    double tmp = double(maxX);
    for (int i = maxX + 1; i < maxX + 3; i++) {
        if (i < EFFTFrame) {
            double koef = 100.;
            if (FFT[i] != 0) {
                koef = (maxY / FFT[i]) * (maxY / FFT[i]);
            }
            tmp = (koef * tmp + double(i)) / (koef + 1.);
            continue;
        }
        break;
    }

    for (int i = maxX - 1; i > maxX - 3; i--) {
        if (i >= 0) {
            double koef = 100.;
            if (FFT[i] != 0) {
                koef = (maxY / FFT[i]) * (maxY / FFT[i]);
            }
            tmp = (koef * tmp + double(i)) / (koef + 1.);
            continue;
        }
        break;
    }

    if (fabs(tmp - absAVGSpeed) > 0.7) {
        absAVGSpeed = tmp;
    } else {
        absAVGSpeed += tmp;
    }
}

tresult AVinyl::receiveText(const char* text)
{
    // received from Controller
    fprintf(stderr, "[Vinyl] received: ");
    fprintf(stderr, "%s", text);
    fprintf(stderr, "\n");

    return kResultOk;
}

tresult PLUGIN_API AVinyl::setState(IBStream* state)
{
    // called when we load a preset, the model has to be reloaded

    int8 byteOrder;
    if (state->read(&byteOrder, sizeof (int8)) == kResultTrue) {

        IBStreamer reader(state, byteOrder);

        reader.readFloat(fGain);
        reader.readFloat(fVolume);
        reader.readFloat(fPitch);

        reader.readInt32u(iCurrentEntry);
        reader.readInt32u(iCurrentScene);

        reader.readFloat(fSwitch);
        reader.readFloat(fCurve);

        uint32_t savedBypass;
        reader.readInt32u(savedBypass);
        reader.readInt32(effectorSet);

        float savedTimeCodeCoeff;
        reader.readFloat(savedTimeCodeCoeff);

        uint32_t savedPadCount;
        uint32_t savedSceneCount;
        uint32_t savedEntryCount;
        reader.readInt32u(savedPadCount);
        reader.readInt32u(savedSceneCount);
        reader.readInt32u(savedEntryCount);

        avgTimeCodeCoeff = savedTimeCodeCoeff;

        bBypass = savedBypass > 0;

        keyLockTone = (effectorSet & eLockTone) > 0;

        fRealPitch = calcRealPitch (fPitch,fSwitch);
        fRealVolume = calcRealVolume(fGain, fVolume, fCurve);

        for (unsigned j = 0; j < savedSceneCount; j++) {
            for (unsigned i = 0; i < savedPadCount; i++) {
                uint32_t savedType;
                uint32_t savedTag;
                uint32_t savedMidi;
                reader.readInt32u(savedType);
                reader.readInt32u(savedTag);
                reader.readInt32u(savedMidi);

                if ((j < EMaximumScenes) && (i < ENumberOfPads)) {
                    padStates[j][i].padType = PadEntry::TypePad(savedType);
                    padStates[j][i].padTag =savedTag;
                    padStates[j][i].padMidi = savedMidi;
                }
            }
        }

        for (int i = 0; i < (int)savedEntryCount; i++) {
            uint32_t savedLoop;
            uint32_t savedReverse;
            uint32_t savedSync;
            float savedTune;
            float savedLevel;
            uint32_t savedNameLen;
            uint32_t savedFileNameLen;

            reader.readInt32u(savedLoop);
            reader.readInt32u(savedReverse);
            reader.readInt32u(savedSync);
            reader.readFloat(savedTune);
            reader.readFloat(savedLevel);
            reader.readInt32u(savedNameLen);
            reader.readInt32u(savedFileNameLen);

            std::vector<tchar> bufname(savedNameLen + 1);
            std::vector<tchar> buffile(savedFileNameLen + 1);

            reader.readInt16uArray((uint16_t *)bufname.data(), int32_t(bufname.size()));
            reader.readInt16uArray((uint16_t *)buffile.data(), int32_t(buffile.size()));

            String sName (bufname.data());
            String sFile (buffile.data());

            SamplesArray.push_back(std::make_unique<SampleEntry<Sample32>>(sName,sFile));
            SamplesArray.back()->index(SamplesArray.size());
            SamplesArray.back()->Loop = savedLoop > 0;
            SamplesArray.back()->Reverse = savedReverse > 0;
            SamplesArray.back()->Sync = savedSync > 0;
            SamplesArray.back()->Tune = savedTune;
            SamplesArray.back()->Level = savedLevel;

        }

        for (unsigned j = 0; j < savedSceneCount; j++) {
            for (unsigned i = 0; i < savedPadCount; i++) {
                if ((j < EMaximumScenes)&&(i < ENumberOfPads)) {
                    SetPadState(&padStates[j][i]);
                }
            }
        }

        float savedLockedSpeed;
        float savedLockedVolume;
        reader.readFloat(savedLockedSpeed);
        reader.readFloat(savedLockedVolume);

        lockSpeed = savedLockedSpeed;
        lockVolume = savedLockedVolume;

        dirtyParams = true;
        return kResultOk;
    }
    return kResultFalse;
}

tresult PLUGIN_API AVinyl::getState(IBStream* state)
{
    // here we need to save the model
    int8 byteOrder = BYTEORDER;
    if (state->write (&byteOrder, sizeof (int8)) == kResultTrue) {

        float toSaveGain = fGain;
        float toSaveVolume = fVolume;
        float toSavePitch = fPitch;
        uint32_t toSaveEntry = iCurrentEntry;
        uint32_t toSaveScene = iCurrentScene;
        float toSaveSwitch = fSwitch;
        float toSaveCurve = fCurve;
        float toSaveTimecodeCoeff = avgTimeCodeCoeff;
        uint32_t toSaveBypass = bBypass ? 1 : 0;
        uint32_t toSaveEffector = effectorSet;
        uint32_t toSavePadCount = ENumberOfPads;
        uint32_t toSaveSceneCount = EMaximumScenes;
        uint32_t toSaveEntryCount = uint32_t(SamplesArray.size());
        float toSavelockSpeed = lockSpeed;
        float toSavelockVolume = lockVolume;

        state->write(&toSaveGain, sizeof(float));
        state->write(&toSaveVolume, sizeof(float));
        state->write(&toSavePitch, sizeof(float));
        state->write(&toSaveEntry, sizeof(uint32_t));
        state->write(&toSaveScene, sizeof(uint32_t));
        state->write(&toSaveSwitch, sizeof(float));
        state->write(&toSaveCurve, sizeof(float));
        state->write(&toSaveBypass, sizeof(uint32_t));
        state->write(&toSaveEffector, sizeof(uint32_t));
        state->write(&toSaveTimecodeCoeff, sizeof(float));
        state->write(&toSavePadCount, sizeof(uint32_t));
        state->write(&toSaveSceneCount, sizeof(uint32_t));
        state->write(&toSaveEntryCount, sizeof(uint32_t));

        for (int j = 0; j < EMaximumScenes; j++) {
            for (int i = 0; i < ENumberOfPads; i++) {
                uint32_t toSaveType = padStates[j][i].padType;
                uint32_t toSaveTag = padStates[j][i].padTag;
                uint32_t toSaveMidi = padStates[j][i].padMidi;
                state->write(&toSaveType, sizeof(uint32_t));
                state->write(&toSaveTag, sizeof(uint32_t));
                state->write(&toSaveMidi, sizeof(uint32_t));
            }
        }

        for (int i = 0; i < SamplesArray.size(); i++) {
            uint32_t toSavedLoop = SamplesArray.at(i)->Loop ? 1 : 0;
            uint32_t toSavedReverse = SamplesArray.at(i)->Reverse ? 1 : 0;
            uint32_t toSavedSync = SamplesArray.at(i)->Sync ? 1 : 0;
            float toSavedTune = SamplesArray.at(i)->Tune;
            float toSavedLevel = SamplesArray.at(i)->Level;
            String tmpName (SamplesArray.at(i)->name());
            String tmpFile (SamplesArray.at(i)->fileName());
            uint32_t toSaveNameLen = tmpName.length();
            uint32_t toSaveFileNameLen = tmpFile.length();

            state->write(&toSavedLoop, sizeof(uint32_t));
            state->write(&toSavedReverse, sizeof(uint32_t));
            state->write(&toSavedSync, sizeof(uint32_t));
            state->write(&toSavedTune, sizeof(float));
            state->write(&toSavedLevel, sizeof(float));
            state->write(&toSaveNameLen, sizeof(uint32_t));
            state->write(&toSaveFileNameLen, sizeof(uint32_t));
            state->write((void *)tmpName.text16(), (toSaveNameLen + 1) * sizeof(TChar));
            state->write((void *)tmpFile.text16(), (toSaveFileNameLen + 1) * sizeof(TChar));
        }

        state->write(&toSavelockSpeed, sizeof(float));
        state->write(&toSavelockVolume, sizeof(float));

        return kResultOk;
    }
    return kResultFalse;
}

tresult PLUGIN_API AVinyl::setupProcessing(ProcessSetup& newSetup) {
    // called before the process call, always in a disable state (not active)

    // here we keep a trace of the processing mode (offline,...) for example.
    currentProcessMode = newSetup.processMode;

    return AudioEffect::setupProcessing(newSetup);
}

tresult PLUGIN_API AVinyl::setBusArrangements(SpeakerArrangement* inputs,
                                              int32 numIns,
                                              SpeakerArrangement* outputs,
                                              int32 numOuts) {
    if ((numIns == 1 || numIns == 2) && numOuts == 1) {
        if (SpeakerArr::getChannelCount(inputs[0]) == 2
            && SpeakerArr::getChannelCount(outputs[0]) == 2) {

            AudioBus* bus = FCast<AudioBus>(audioInputs.at(0));
            if (bus) {
                if (bus->getArrangement() != inputs[0]) {
                    removeAudioBusses();
                    addAudioInput(STR16("Stereo In"), inputs[0]);
                    addAudioOutput(STR16("Stereo Out"), outputs[0]);

                    // recreate the Stereo SideChain input bus
                    if ((numIns == 2) && (SpeakerArr::getChannelCount(inputs[1]) == 2)) {
                        addAudioInput(STR16("Stereo Aux In"), inputs[1], kAux, 0);
                    }
                }
            }
            return kResultTrue;
        }
    }
    return kResultFalse;
}

tresult PLUGIN_API AVinyl::notify (IMessage* message)
{
    if (strcmp(message->getMessageID(), "setPad") == 0) {
        int64 PadNumber;
        if (message->getAttributes()->getInt("PadNumber", PadNumber) == kResultOk) {
            padStates[iCurrentScene][PadNumber-1].padState = false;
            int64 PadType;
            if (message->getAttributes()->getInt("PadType", PadType) == kResultOk) {
                padStates[iCurrentScene][PadNumber-1].padType = PadEntry::TypePad(PadType);
                int64 PadTag;
                if (message->getAttributes()->getInt("PadTag", PadTag) == kResultOk) {
                    padStates[iCurrentScene][PadNumber-1].padTag = PadTag;
                    SetPadState(&padStates[iCurrentScene][PadNumber-1]);
                }
            }
            dirtyParams = true;
        }
        return kResultTrue;
    }

    if (strcmp(message->getMessageID(), "touchPad") == 0) {
        int64 PadNumber;
        if (message->getAttributes()->getInt("PadNumber", PadNumber) == kResultOk) {
            double PadValue;
            if (message->getAttributes()->getFloat("PadValue", PadValue) == kResultOk) {
                dirtyParams |= PadWork(PadNumber,PadValue);
            }

        }
        return kResultTrue;
    }

    if (strcmp(message->getMessageID(), "loadNewEntry") == 0) {
        TChar stringBuff[256] = {0};
        if (message->getAttributes()->getString("File", stringBuff, sizeof(stringBuff) / sizeof(TChar)) == kResultOk) {
            String newFile(stringBuff);
            memset(stringBuff, 0, 256 * sizeof(tchar));
            if (message->getAttributes()->getString("Sample", stringBuff, sizeof(stringBuff) / sizeof(TChar)) == kResultOk) {
                String newName(stringBuff);
                SamplesArray.push_back(std::make_unique<SampleEntry<Sample32>>(newName, newFile));
                SamplesArray.back()->index(SamplesArray.size());
                if (iCurrentEntry == (SamplesArray.back()->index() - 1)) {
                    PadSet(iCurrentEntry);
                }
                addSampleMessage(SamplesArray.back().get());
                dirtyParams = true;
            }
        }

        return kResultTrue;
    }

    if (strcmp(message->getMessageID(), "replaceEntry") == 0) {
        TChar stringBuff[256] = {0};
        if (message->getAttributes()->getString("File", stringBuff, sizeof (stringBuff) / sizeof (TChar)) == kResultOk) {
            String newFile(stringBuff);
            if (message->getAttributes()->getString("Sample", stringBuff, sizeof (stringBuff) / sizeof (TChar)) == kResultOk) {
                String newName(stringBuff);
                SamplesArray[iCurrentEntry] = std::make_unique<SampleEntry<Sample32>>(newName, newFile);
                SamplesArray[iCurrentEntry]->index(iCurrentEntry + 1);
            }
        }
        return kResultTrue;
    }

    if (strcmp(message->getMessageID(), "renameEntry") == 0) {
        TChar stringBuff[256] = {0};
        if (message->getAttributes()->getString("SampleName", stringBuff, sizeof(stringBuff) / sizeof(TChar)) == kResultOk) {
            int64 sampleIndex;
            if (message->getAttributes()->getInt("SampleNumber", sampleIndex) == kResultOk) {
                String newName(stringBuff);
                SamplesArray.at(iCurrentEntry)->name(newName);
            }
        }
        return kResultTrue;
    }

    if (strcmp(message->getMessageID(), "deleteEntry") == 0) {
        int64 sampleIndex;
        if (message->getAttributes()->getInt ("SampleNumber", sampleIndex) == kResultOk) {
            delSampleMessage(SamplesArray.at(sampleIndex).get());
            SamplesArray.erase(SamplesArray.begin() + sampleIndex);
            for (size_t i = 0; i < SamplesArray.size(); i++) {
                SamplesArray.at(i)->index(i + 1);
            }
            PadRemove(sampleIndex);
            dirtyParams = true;
        }
        return kResultTrue;
    }

    if (strcmp(message->getMessageID(), "initView") == 0) {
        initSamplesMessage();
        dirtyParams = true;
        return kResultTrue;
    }
    return AudioEffect::notify (message);
}

void AVinyl::addSampleMessage(SampleEntry<Sample32>* newSample)
{
    if (newSample) {
        int64_t sampleInt = int64_t(newSample);
        IMessage* msg = allocateMessage ();
        if (msg) {
            msg->setMessageID("addEntry");
            msg->getAttributes()->setInt("Entry", sampleInt);
            sendMessage(msg);
            msg->release();
        }
    }
}

void AVinyl::delSampleMessage(SampleEntry<Sample32> *delSample)
{
    if (delSample) {
        IMessage* msg = allocateMessage ();
        if (msg) {
            msg->setMessageID("delEntry");
            msg->getAttributes()->setInt("EntryIndex", delSample->index() - 1);
            sendMessage(msg);
            msg->release ();
        }
    }
}

void AVinyl::debugFftMessage(SampleEntry<Sample32>* fft)
{
    if (fft) {
        int64_t fftInt = int64_t(fft);
        IMessage* msg = allocateMessage();
        if (msg) {
            msg->setMessageID("debugFft");
            msg->getAttributes()->setInt("Entry", fftInt);
            sendMessage(msg);
            msg->release();
        }
    }
}

void AVinyl::debugInputMessage(SampleEntry<Sample32>* input)
{
    if (input) {
        int64_t inputInt = int64_t(input);
        IMessage* msg = allocateMessage();
        if (msg) {
            msg->setMessageID("debugInput");
            msg->getAttributes()->setInt("Entry", inputInt);
            sendMessage(msg);
            msg->release();
        }
    }
}

void AVinyl::initSamplesMessage(void)
{
    for (int32 i = 0; i < SamplesArray.size (); i++) {
        int64 sampleInt = int64(SamplesArray.at(i).get());
        IMessage* msg = allocateMessage();
        if (msg) {
            msg->setMessageID("addEntry");
            msg->getAttributes()->setInt("Entry", sampleInt);
            sendMessage(msg);
            msg->release ();
        }
    }
}

void AVinyl::updateSpeedMessage(Sample64 speed)
{
    IMessage* msg = allocateMessage ();
    if (msg) {
        msg->setMessageID("updateSpeed");
        msg->getAttributes()->setFloat("Speed", speed);
        sendMessage(msg);
        msg->release();
    }
}

void AVinyl::updatePositionMessage(Sample64 position)
{
    IMessage* msg = allocateMessage ();
    if (msg) {
        msg->setMessageID("updatePosition");
        msg->getAttributes()->setFloat("Position", position);
        sendMessage(msg);
        msg->release();
    }
}

void AVinyl::updatePadsMessage(void)
{
    IMessage* msg = allocateMessage ();
    if (msg) {
        msg->setMessageID("updatePads");
        String tmp;
        String ttmp1[ENumberOfPads];
        String ttmp2[ENumberOfPads];
        String ttmp3[ENumberOfPads];
        for (int i = 0; i < ENumberOfPads; i++) {
            ttmp1[i] = tmp.printf("PadState%02d", i);
            msg->getAttributes()->setInt(ttmp1[i].text8(), padStates[iCurrentScene][i].padState ? 1 : 0);
            ttmp2[i] = tmp.printf("PadType%02d", i);
            msg->getAttributes()->setInt(ttmp2[i].text8(), padStates[iCurrentScene][i].padType);
            ttmp3[i] = tmp.printf("PadTag%02d", i);
            msg->getAttributes()->setInt(ttmp3[i].text8(), padStates[iCurrentScene][i].padTag);
        }
        sendMessage(msg);
        msg->release();
    }
}

void AVinyl::processEvent(const Event &event)
{
    switch (event.type) {
    case Event::kNoteOnEvent:
        for (int i = 0; i < ENumberOfPads; i++) {
            if (padStates[iCurrentScene][i].padMidi == event.noteOn.pitch) {
                dirtyParams |= PadWork(i, 1.0);
            }
            if (padStates[iCurrentScene][i].padType == PadEntry::AssigMIDI) {
                padStates[iCurrentScene][i].padType = PadEntry::SamplePad;
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

void AVinyl::reset(bool state)
{
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

void AVinyl::SetCurrentEntry(int newentry)
{
    if ((newentry < SamplesArray.size()) && (newentry >= 0)) {
        iCurrentEntry = newentry;
        SamplesArray.at(newentry)->resetCursor();
        fPosition = 0;
    }
}

bool AVinyl::PadWork(int padId, Sample32 paramValue)
{
    bool result = false;
    switch(padStates[iCurrentScene][padId].padType) {
    case PadEntry::SamplePad:
        if ((padStates[iCurrentScene][padId].padTag >= 0)
            && (SamplesArray.size() > padStates[iCurrentScene][padId].padTag)) {
            if (paramValue > 0.5) {
                for (int j = 0; j < EMaximumScenes; j++) {
                    for (int i = 0; i < ENumberOfPads; i++) {
                        if (padStates[j][i].padType == PadEntry::SamplePad) {
                            padStates[j][i].padState = false;
                        }
                    }
                }
                padStates[iCurrentScene][padId].padState = true;
                SetCurrentEntry (padStates[iCurrentScene][padId].padTag);
                result = true;
            }
        }
        break;
    case PadEntry::SwitchPad:
    {
        if (paramValue > 0.5) {
            padStates[iCurrentScene][padId].padState = !padStates[iCurrentScene][padId].padState;
            effectorSet ^= padStates[iCurrentScene][padId].padTag;
            result = true;
        }
    }
    break;
    case PadEntry::KickPad:
    {
        if (paramValue > 0.5) {
            padStates[iCurrentScene][padId].padState = true;
            effectorSet |= padStates[iCurrentScene][padId].padTag;
        }else{
            padStates[iCurrentScene][padId].padState = false;
            effectorSet &= ~padStates[iCurrentScene][padId].padTag;
        }
        result = true;
    }
    break;
    default:
        break;
    }
    return result;
}

bool AVinyl::PadSet(int currentSample)
{
    bool result = false;
    for (int j = 0; j < EMaximumScenes; j++) {
        for (int i = 0; i < ENumberOfPads; i++) {
            if (padStates[j][i].padType == PadEntry::SamplePad) {
                if (padStates[j][i].padTag == currentSample) {
                    padStates[j][i].padState = true;
                } else {
                    padStates[j][i].padState = false;
                }
                result = true;
            }
        }
    }
    return result;
}

void AVinyl::SetPadState(PadEntry * pad)
{
    switch(pad->padType){
    case PadEntry::SamplePad:
        pad->padState = (pad->padTag == int(iCurrentEntry));
        break;
    case PadEntry::SwitchPad:
        pad->padState = (pad->padTag & effectorSet);
        break;
    default:
        pad->padState = false;
    }
}

bool AVinyl::PadRemove(int currentSample)
{
    bool result = false;
    for (int j = 0; j < EMaximumScenes; j++) {
        for (int i = 0; i < ENumberOfPads; i++) {
            if (padStates[j][i].padType == PadEntry::SamplePad) {
                if (padStates[j][i].padTag == currentSample) {
                    padStates[j][i].padTag = -1;
                } else if (padStates[j][i].padTag > currentSample) {
                    padStates[j][i].padTag = padStates[j][i].padTag - 1;
                }
                result = true;
            }
        }
    }
    return result;
}

float AVinyl::GetNormalizeTag(int tag)
{
    if (padStates[iCurrentScene][tag].padTag < 0) {
        return 0.;
    }

    switch (padStates[iCurrentScene][tag].padType) {
    case PadEntry::SamplePad:
        if (padStates[iCurrentScene][tag].padTag >= SamplesArray.size()) {
            return 0.;
        }
        return (padStates[iCurrentScene][tag].padTag + 1.) / SamplesArray.size();
    case PadEntry::SwitchPad:
        if (padStates[iCurrentScene][tag].padTag >= 5) {
            return 0.;
        }
        return (padStates[iCurrentScene][tag].padTag + 1.) / 5.;
    case PadEntry::KickPad:
        if (padStates[iCurrentScene][tag].padTag >= 5) {
            return 0.;
        }
        return (padStates[iCurrentScene][tag].padTag + 1.) / 5.;
    default:
        break;
    }
    return 0.;
}

}} // namespaces
