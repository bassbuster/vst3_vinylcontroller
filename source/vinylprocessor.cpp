#pragma hdrstop

#include "vinylprocessor.h"
#include "vinylparamids.h"
#include "vinylcids.h"	// for class ids
#include "helpers/parameterwriter.h"
#include "helpers/resourcepath.h"
#include "helpers/fft.h"

#include "pluginterfaces/base/ibstream.h"
#include "base/source/fstring.h"
#include "base/source/fstreamer.h"

#include <stdio.h>
#include <cmath>


namespace {

inline double sqr(double x) {
    return x * x;
}

constexpr double Pi = 3.14159265358979323846264338327950288;

template<typename T>
inline T aproxParamValue(int32_t currentSampleIndex, T currentValue, int32_t nextSampleIndex, T nextValue)
{
    T slope = (nextValue - currentValue) / T(nextSampleIndex - currentSampleIndex + 1);
    T offset = currentValue - (slope * T(currentSampleIndex - 1));
    return (slope * T(currentSampleIndex)) + offset; // bufferTime is any position in buffer
}

template<typename T>
inline T calcRealPitch(T normalPitch, T normalKoeff)
{
    if ((normalKoeff != 0.) && (normalPitch != 0.5)) {
        T multipleKoeff;
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
    return 1.;
}

template<typename T>
inline T calcRealVolume(T normalGain, T normalVolume, T normalKoeff)
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
    fGain(0.8),
    fRealVolume(0.8),
    fVuOldL(0.),
    fVuOldR(0.),
    fVolume(1.0),
    fPitch(.5),
    fRealPitch(1.0),
    currentEntry_(0),
    currentScene_(0),
    fSwitch(0),
    fCurve(0),
    currentProcessMode(-1), // -1 means not initialized
    bBypass(false),
    avgTimeCodeCoeff_(ETimeCodeCoeff),
    bTCLearn(false),
    dSampleRate(EDefaultSampleRate),
    dTempo(EDefaultTempo),
    timecodeLearnCounter_(0),
    HoldCounter(0),
    FreezeCounter(0),
    dNoteLength(0),
    directionBits_(0),
    direction_(1.),
    effectorSet_(eNoEffects),
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

    vintageSample_ = std::make_unique<SampleEntry<Sample64>>("vintage", (getResourcePath() + "\\vintage.wav").c_str());
    //vintageSample_ = std::make_unique<SampleEntry<Sample64>>("vintage", "c:\\Work\\vst3sdk\\build\\VST3\\Debug\\vinylcontroller.vst3\\Contents\\Resources\\vintage.wav");
    //vintageSample_ = std::make_unique<SampleEntry<Sample64>>("vintage", vintageLeft, vintageRight, sizeof(vintageLeft) / sizeof(Sample64));
    if (vintageSample_) {
        vintageSample_->Loop = true;
        vintageSample_->Sync = false;
        vintageSample_->Reverse = false;
    }


    //{
    //    auto VintageSample = std::make_unique<SampleEntry<Sample64>>("vintage", "c:\\Work\\vst3sdk\\build\\VST3\\Debug\\vinylcontroller.vst3\\Contents\\Resources\\vintage.wav");

    //    std::ofstream vintage("c:\\Work\\vst3sdk\\build\\VST3\\Debug\\vinylcontroller.vst3\\Contents\\Resources\\vintage.h");
    //    vintage << "#pragma once" << std::endl;
    //    vintage << "" << std::endl;

    //    vintage << "double vintageLeft[] = {";
    //    for(size_t i = 0; i < VintageSample->bufferLength(); i++) {
    //        vintage << VintageSample->getLeft(i) << (i == 0 ? "" : ",");
    //    }
    //    vintage << "};" << std::endl;

    //    vintage << "double vintageRight[] = {";
    //    for(size_t i = 0; i < VintageSample->bufferLength(); i++) {
    //        vintage << VintageSample->getRight(i) << (i == 0 ? "" : ",");
    //    }
    //    vintage << "};" << std::endl;
    //}


    params_.addReader(kBypassId, [this] () { return bBypass ? 1. : 0.; },
                     [this](Sample64 value) {
                         bBypass = value > 0.5;
                     });
    params_.addReader(kGainId,   [this] () { return fGain; },
                     [this](Sample64 value) {
                         fGain = value;
                         fRealVolume = calcRealVolume(fGain, fVolume, fCurve); },
                     [this] (int32 sampleOffset, int32 currentOffset, Sample64 currentValue) {
                         fGain = aproxParamValue<Sample32>(sampleOffset, fGain, currentOffset, currentValue);
                         fRealVolume = calcRealVolume(fGain, fVolume, fCurve);
                     });
    params_.addReader(kVolumeId, [this] () { return fVolume; },
                     [this](Sample64 value) {
                         fVolume = value;
                         fRealVolume = calcRealVolume(fGain, fVolume, fCurve); },
                     [this] (int32 sampleOffset, int32 currentOffset, Sample64 currentValue) {
                         fVolume = aproxParamValue<Sample32>(sampleOffset, fVolume, currentOffset, currentValue);
                         fRealVolume = calcRealVolume(fGain, fVolume, fCurve);
                     });
    params_.addReader(kVolCurveId, [this] () { return fCurve; },
                     [this](Sample64 value) {
                         fCurve = value;
                         fRealVolume = calcRealVolume(fGain, fVolume, fCurve); },
                     [this] (int32 sampleOffset, int32 currentOffset, Sample64 currentValue) {
                         fCurve = aproxParamValue<Sample32>(sampleOffset, fCurve, currentOffset, currentValue);
                         fRealVolume = calcRealVolume(fGain, fVolume, fCurve);
                     });
    params_.addReader(kPitchId, [this] () { return fPitch; },
                     [this](Sample64 value) { fPitch = value; fRealPitch = calcRealPitch (fPitch, fSwitch); },
                     [this] (int32 sampleOffset, int32 currentOffset, Sample64 currentValue) {
                         fPitch = aproxParamValue<Sample32>(sampleOffset, fPitch, currentOffset, currentValue);
                         fRealPitch = calcRealPitch (fPitch, fSwitch);
                     });
    params_.addReader(kPitchSwitchId, [this] () { return fSwitch; },
                     [this](Sample64 value) {
                         fSwitch = value;
                         fRealPitch = calcRealPitch (fPitch, fSwitch); },
                     [this] (int32 sampleOffset, int32 currentOffset, Sample64 currentValue) {
                         fSwitch = aproxParamValue<Sample32>(sampleOffset, fSwitch, currentOffset, currentValue);
                         fRealPitch = calcRealPitch (fPitch, fSwitch);
                     });

    params_.addReader(kCurrentEntryId, [this] () { return double(currentEntry_) / (EMaximumSamples - 1.); },
                     [this](Sample64 value) {
                         currentEntry(floor(value * double(EMaximumSamples - 1) + 0.5));
                         dirtyParams_ |= padSet(currentEntry_);
                     });
    params_.addReader(kCurrentSceneId, [this] () { return double(currentScene_) / (EMaximumScenes - 1.); },
                     [this](Sample64 value) {
                         currentScene_ = floor(value * double(EMaximumScenes - 1.) + 0.5);
                         dirtyParams_ = true;
                     });

    params_.addReader(kLoopId, [this] () { return (samplesArray_.size() > currentEntry_) ? (samplesArray_.at(currentEntry_)->Loop ? 1. : 0.) : 0.; },
                     [this](Sample64 value) {
                         if (samplesArray_.size() > currentEntry_) {
                             samplesArray_.at(currentEntry_)->Loop = value > 0.5;
                         }
                     });

    params_.addReader(kSyncId, [this] () { return (samplesArray_.size() > currentEntry_) ? (samplesArray_.at(currentEntry_)->Sync ? 1. : 0.) : 0.; },
                     [this](Sample64 value) {
                         if (samplesArray_.size() > currentEntry_) {
                             samplesArray_.at(currentEntry_)->Sync = value > 0.5;
                         }
                     });

    params_.addReader(kReverseId, [this] () { return (samplesArray_.size() > currentEntry_) ? (samplesArray_.at(currentEntry_)->Reverse ? 1. : 0.) : 0.; },
                     [this](Sample64 value) {
                         if (samplesArray_.size() > currentEntry_) {
                             samplesArray_.at(currentEntry_)->Reverse = value > 0.5;
                         }
                     });

    params_.addReader(kAmpId, [this] () { return (samplesArray_.size() > currentEntry_) ? (samplesArray_.at(currentEntry_)->Level / 2.) : 0.5; },
                     [this](Sample64 value) {
                         if (samplesArray_.size() > currentEntry_) {
                             samplesArray_.at(currentEntry_)->Level = value * 2.0;
                         }
                     });

    params_.addReader(kTuneId, [this] () { return (samplesArray_.size() > currentEntry_) ? (samplesArray_.at(currentEntry_)->Tune > 1.0 ? samplesArray_.at(currentEntry_)->Tune / 2.0 : samplesArray_.at(currentEntry_)->Tune - 0.5): 0.5; },
                     [this](Sample64 value) {
                         if (samplesArray_.size() > currentEntry_) {
                             samplesArray_.at(currentEntry_)->Tune = value > 0.5 ? value * 2.0 : value + 0.5;
                         }
                     });

    params_.addReader(kTimecodeLearnId, [this] () { return bTCLearn ? 1. : 0.; },
                     [this](Sample64 value) {
                         if (value>0.5) {
                             timecodeLearnCounter_ = ETimecodeLearnCount;
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
            padStates[j][i].padTag = j * ENumberOfPads + i;
            padStates[j][i].padState = false;
            padStates[j][i].padMidi = gPad[i];
        }
    }

    reset(true);
    dirtyParams_ = false;
    return kResultOk;
}

// ------------------------------------------------------------------------
tresult PLUGIN_API AVinyl::terminate() {
    // nothing to do here yet...except calling our parent terminate
    samplesArray_.clear();
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
tresult PLUGIN_API AVinyl::process(ProcessData& data)
{
    try {

        bool samplesParamsUpdate = false;
        if (data.processContext) {
            dSampleRate = data.processContext->sampleRate;
            dTempo = data.processContext->tempo;
            dNoteLength = noteLengthInSamples(ERollNote, dTempo, dSampleRate);
        }

        IParameterChanges* paramChanges = data.inputParameterChanges;
        IParameterChanges* outParamChanges = data.outputParameterChanges;
        IEventList* eventList = data.inputEvents;

        params_.setQueue(paramChanges);

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
        Event* eventP = nullptr;
        int32 eventIndex = 0;

        Finalizer readTheRest([&]() {

            params_.flush();

            while (eventList) {
                if (eventList->getEvent(eventIndex++, event) != kResultOk) {
                    eventList = nullptr;
                }
                else {
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
        uint8_t** in = reinterpret_cast<uint8_t**>(data.inputs[0].channelBuffers64);
        uint8_t** out = reinterpret_cast<uint8_t**>(data.outputs[0].channelBuffers64);

        //{
        Sample64 fVuLeft = 0.;
        Sample64 fVuRight = 0.;
        Sample64 fOldPosition = fPosition;
        bool bOldTimecodeLearn = bTCLearn;
        Sample64 fOldSpeed = fRealSpeed;

        if (numChannels >= 2) {


            int32 sampleFrames = data.numSamples;
            int32 sampleOffset = 0;

            uint8_t* ptrInLeft = in[0];
            uint8_t* ptrOutLeft = out[0];
            uint8_t* ptrInRight = in[1];
            uint8_t* ptrOutRight = out[1];

            while (--sampleFrames >= 0) {

                params_.checkOffset(sampleOffset);

                ////////////Get Offsetted Events ////////////////////////////////////
                if (eventList) {
                    if (eventP == nullptr) {
                        if (eventList->getEvent(eventIndex++, event) != kResultOk) {
                            eventList = nullptr;
                            eventP = nullptr;
                        }
                        else {
                            eventP = &event;
                        }
                    }
                    if ((eventP != nullptr) && (event.sampleOffset == sampleOffset)) {
                        processEvent(event);
                        eventP = nullptr;
                    }
                }
                /////////////////////////////////////////////////////////////////////

                Sample64 inL{0.};
                Sample64 inR{0.};
                Sample64 outL{0.};
                Sample64 outR{0.};

                if (!bBypass) {

                    if (data.symbolicSampleSize == kSample64) {
                        inL = *reinterpret_cast<Sample64*>(ptrInLeft);
                        inR = *reinterpret_cast<Sample64*>(ptrInRight);
                        ptrInLeft += sizeof(Sample64);
                        ptrInRight += sizeof(Sample64);
                    } else {
                        inL = *reinterpret_cast<Sample32*>(ptrInLeft);
                        inR = *reinterpret_cast<Sample32*>(ptrInRight);
                        ptrInLeft += sizeof(Sample32);
                        ptrInRight += sizeof(Sample32);
                    }

                    filterBufferLeft_.writeHead(filtredHiLeft_.append(inL));
                    filterBufferRight_.writeHead(filtredHiRight_.append(inR));

                    Sample64 newSignalLeft = filterBufferLeft_.readTail() - filtredLoLeft_.append(inL);
                    Sample64 newSignalRight = filterBufferRight_.readTail() - filtredLoRight_.append(inR);

                    deltaLeft_.append(newSignalLeft - oldSignalLeft_);
                    deltaRight_.append(newSignalRight - oldSignalRight_);


                    calcDirectionTimeCodeAmplitude();

                    oldSignalLeft_ = newSignalLeft;
                    oldSignalRight_ = newSignalRight;

                    Sample64 smoothWindow = sin(Pi * Sample64(speedFrameIndex_ + EFFTFrame - ESpeedFrame) / Sample64(EFFTFrame));
                    originalBuffer_[speedFrameIndex_ + EFFTFrame - ESpeedFrame] = oldSignalLeft_ - oldSignalRight_;
                    fftBuffer_[speedFrameIndex_ + EFFTFrame - ESpeedFrame] = (smoothWindow * originalBuffer_[speedFrameIndex_ + EFFTFrame - ESpeedFrame]);
                    //filtred_[FFTCursor + EFFTFrame - ESpeedFrame] = inL - inR;// (OldSignalL - OldSignalR);
                    //fft_[FFTCursor + EFFTFrame - ESpeedFrame].real = (SmoothCoef * filtred_[FFTCursor + EFFTFrame - ESpeedFrame]);
                    //fft_[FFTCursor + EFFTFrame - ESpeedFrame].imaginary = 0;

                    speedFrameIndex_++;
                    if (speedFrameIndex_ >= ESpeedFrame) {
                        speedFrameIndex_ = 0;

                        if (timeCodeAmplytude_ >= ETimeCodeMinAmplytude) {
#ifdef DEVELOPMENT
                            {
                                debugInputMessage(fftBuffer_, EFFTFrame);
                            }
#endif // DEBUG
                            fastsine(fftBuffer_, EFFTFrame);
                            //fft(fft_, EFFTFrame);
                            /// Filter out low noise
                            for (size_t i = 0; i < 10; i++) {
                                Sample64 SmoothCoef =  i * .1 + .01;
                                fftBuffer_[i] = SmoothCoef * fftBuffer_[i];
                                //fft_[i].real = SmoothCoef * fft_[i].real;
                                //fft_[i].imaginary = SmoothCoef * fft_[i].imaginary;
                            }
#ifdef DEVELOPMENT
                            {
                                //Sample64 real_[EFFTFrame];
                                //for (size_t i = 0; i < EFFTFrame; i++) {
                                //    real_[i] = fft_[i].real;
                                //}
                                debugFftMessage(fftBuffer_, EFFTFrame);
                            }
                            //{
                            //    Sample64 real_[EFFTFrame];
                            //    for (size_t i = 0; i < EFFTFrame; i++) {
                            //        real_[i] = fft_[i].imaginary;
                            //    }
                            //    debugInputMessage(real_, EFFTFrame);
                            //}
#endif // DEBUG

                            calcAbsSpeed();
                            if (timecodeLearnCounter_ > 0) {
                                timecodeLearnCounter_--;
                                avgTimeCodeCoeff_.append(direction_ * absAvgSpeed_);
                                fRealSpeed = 1.;
                            }
                            else {
                                fRealSpeed = absAvgSpeed_ / avgTimeCodeCoeff_;
                                bTCLearn = false;
                            }
                            fVolCoeff.append(sqrt(fabs(fRealSpeed)));
                            fRealSpeed = direction_ * fRealSpeed;
                        }

                        for (size_t i = 0; i < EFFTFrame - ESpeedFrame; i++) {
                            //Sample64 SmoothCoef = 0.5 - 0.5 * cos((2.0 * Pi * Sample64(i) / EFFTFrame));
                            Sample64 SmoothCoef = sin(Pi * Sample64(i) / Sample64(EFFTFrame));
                            originalBuffer_[i] = originalBuffer_[ESpeedFrame + i];
                            fftBuffer_[i] = (SmoothCoef * originalBuffer_[ESpeedFrame + i]);
                            //filtred_[i] = filtred_[ESpeedFrame + i];
                            //fft_[i].real = (SmoothCoef * filtred_[ESpeedFrame + i]);
                            //fft_[i].imaginary = 0.;
                        }
                    }

                    if (timeCodeAmplytude_ < ETimeCodeMinAmplytude) {
                        if (fVolCoeff >= 0.00001) {
                            absAvgSpeed_ = absAvgSpeed_ / 1.07;
                            fRealSpeed = absAvgSpeed_ / avgTimeCodeCoeff_;
                            fVolCoeff.append(sqrt(fabs(fRealSpeed)));
                            fRealSpeed = direction_ * fRealSpeed;
                        } else {
                            absAvgSpeed_ = 0.;
                            fRealSpeed = 0.;
                            fVolCoeff = 0.;
                        }
                    }

                    if ((fVolCoeff >= 0.00001) && (samplesArray_.size() > currentEntry_)) {

                        ////////////////////EFFECTOR BEGIN//////////////////////////////

                        softVolume.append((effectorSet_ & ePunchIn) ? (fGain * fVolCoeff) :
                            ((effectorSet_ & ePunchOut) ? 0. : (fRealVolume * fVolCoeff)));
                        softPreRoll.append(effectorSet_ & ePreRoll ? 1. : 0.);
                        softPostRoll.append(effectorSet_ & ePostRoll ? 1. : 0.);
                        softDistorsion.append(effectorSet_ & eDistorsion ? 1. : 0.);
                        softVintage.append(effectorSet_ & eVintage ? 1. : 0.);

                        if (effectorSet_ & eHold) {
                            if (!keyHold) {
                                HoldCue = samplesArray_.at(currentEntry_)->cue();
                                keyHold = true;
                            }
                            softHold.append(1.);
                        } else {
                            keyHold = false;
                            softHold.append(0.);
                        }

                        if (effectorSet_ & eFreeze) {
                            if (!keyFreeze) {
                                FreezeCue = samplesArray_.at(currentEntry_)->cue();
                                FreezeCueCur = FreezeCue;
                                keyFreeze = true;
                            }
                            softFreeze.append(1.);
                        } else {
                            if (keyFreeze) {
                                keyFreeze = false;
                                AfterFreezeCue = FreezeCueCur;
                            }
                            softFreeze.append(0.);
                        }

                        if (effectorSet_ & eLockTone) {
                            if (!keyLockTone) {
                                keyLockTone = true;
                                lockSpeed = fabs(fRealSpeed * fRealPitch);
                                lockVolume = fVolCoeff;
                                lockTune = samplesArray_.at(currentEntry_)->Tune;
                                samplesArray_.at(currentEntry_)->beginLockStrobe();
                            }
                            fVolCoeff = lockVolume;
                        } else {
                            keyLockTone = false;
                        }

                        //////////////////////////PLAYER////////////////////////////////////////////////
                        Sample64 speed = 0.;//fRealSpeed * fRealPitch;
                        Sample64 tempo = 0.;

                        if (keyLockTone) {
                            speed = direction_ * lockSpeed;
                            tempo = samplesArray_.at(currentEntry_)->Sync
                                ? fabs(dTempo * fRealSpeed * fRealPitch)
                                : samplesArray_.at(currentEntry_)->tempo() * fabs(fRealSpeed * fRealPitch) * lockTune;
                            samplesArray_.at(currentEntry_)->playStereoSampleTempo(&outL,
                                &outR,
                                speed,
                                tempo,
                                dSampleRate,
                                true);
                        } else {
                            speed = fRealSpeed * fRealPitch;
                            tempo = dTempo;
                            samplesArray_.at(currentEntry_)->playStereoSample(&outL,
                                &outR,
                                speed,
                                tempo,
                                dSampleRate,
                                true);
                        }

                        if (keyFreeze) {
                            FreezeCounter++;
                            bool pushSync = samplesArray_.at(currentEntry_)->Sync;
                            samplesArray_.at(currentEntry_)->Sync = false;
                            auto PushCue = samplesArray_.at(currentEntry_)->cue();
                            samplesArray_.at(currentEntry_)->cue(FreezeCueCur);
                            samplesArray_.at(currentEntry_)->playStereoSample(&outL,
                                &outR,
                                speed,
                                tempo,
                                dSampleRate,
                                true);
                            FreezeCueCur = samplesArray_.at(currentEntry_)->cue();
                            if ((softFreeze > 0.00001) && (softFreeze < 0.99)) {
                                Sample64 fLeft = 0;
                                Sample64 fRight = 0;
                                samplesArray_.at(currentEntry_)->cue(AfterFreezeCue);
                                samplesArray_.at(currentEntry_)->playStereoSample(&fLeft,
                                    &fRight,
                                    speed,
                                    tempo,
                                    dSampleRate,
                                    true);
                                outL = outL * softFreeze + fLeft * (1.0 - softFreeze);
                                outR = outR * softFreeze + fRight * (1.0 - softFreeze);
                                AfterFreezeCue = samplesArray_.at(currentEntry_)->cue();
                            }
                            if (speed != 0.0) {
                                if (FreezeCounter >= dNoteLength) {
                                    FreezeCounter = 0;
                                    AfterFreezeCue = FreezeCueCur;
                                    FreezeCueCur = FreezeCue;
                                    softFreeze = 0;
                                }
                            }
                            samplesArray_.at(currentEntry_)->Sync = pushSync;
                            samplesArray_.at(currentEntry_)->cue(PushCue);
                        }

                        if (keyHold) {
                            HoldCounter++;
                            if ((softHold > 0.00001) && (softHold < 0.99)) {
                                Sample64 fLeft = 0;
                                Sample64 fRight = 0;
                                auto PushCue = samplesArray_.at(currentEntry_)->cue();
                                samplesArray_.at(currentEntry_)->cue(AfterHoldCue);
                                samplesArray_.at(currentEntry_)->playStereoSample(&fLeft,
                                    &fRight,
                                    speed,
                                    samplesArray_.at(currentEntry_)->tempo(),
                                    dSampleRate,
                                    true);
                                outL = outL * softHold + fLeft * (1. - softHold);
                                outR = outR * softHold + fRight * (1. - softHold);
                                AfterHoldCue = samplesArray_.at(currentEntry_)->cue();
                                samplesArray_.at(currentEntry_)->cue(PushCue);
                            }
                            if (HoldCounter >= dNoteLength) {
                                AfterHoldCue = samplesArray_.at(currentEntry_)->cue();
                                samplesArray_.at(currentEntry_)->cue(HoldCue);
                                HoldCounter = 0;
                                softHold = 0;
                            }
                        }
                        ////////////////////////////////////////////////////////////////////////////

                        if ((softPreRoll > 0.0001) || (softPostRoll > 0.0001)) {

                            Sample64 offset = samplesArray_.at(currentEntry_)->noteLength(ERollNote, dTempo);
                            Sample64 pre_offset = offset;
                            Sample64 pos_offset = -offset;

                            Sample64 fLeft = 0;
                            Sample64 fRight = 0;
                            for (int i = 0; i < ERollCount; i++) {
                                Sample32 rollVolume = (1. - Sample32(i) / Sample32(ERollCount)) * .6;
                                if (softPreRoll > 0.0001) {
                                    samplesArray_.at(currentEntry_)->playStereoSample(&fLeft, &fRight, pos_offset, false);
                                    outL = outL * (1. - softPreRoll * 0.1) + fLeft * rollVolume * softPreRoll;
                                    outR = outR * (1. - softPreRoll * 0.1) + fRight * rollVolume * softPreRoll;
                                    pos_offset = pos_offset + offset;
                                }
                                if (softPostRoll > 0.0001) {
                                    samplesArray_.at(currentEntry_)->playStereoSample(&fLeft, &fRight, pre_offset, false);
                                    outL = outL * (1. - softPostRoll * 0.1) + fLeft * rollVolume * softPostRoll;
                                    outR = outR * (1. - softPostRoll * 0.1) + fRight * rollVolume * softPostRoll;
                                    pre_offset = pre_offset - offset;
                                }
                            }
                        }

                        if (softDistorsion > 0.0001) {
                            if (outL > 0) {
                                outL = outL * (1. - softDistorsion * 0.6) + 0.4 * sqrt(outL) * softDistorsion;
                            } else {
                                outL = outL * (1. - softDistorsion * 0.6) - 0.3 * sqrt(-outL) * softDistorsion;
                            }
                            if (outR > 0) {
                                outR = outR * (1. - softDistorsion * 0.6) + 0.4 * sqrt(outR) * softDistorsion;
                            } else {
                                outR = outR * (1. - softDistorsion * 0.6) - 0.3 * sqrt(-outR) * softDistorsion;
                            }
                        }

                        if (softVintage > 0.0001) {
                            Sample64 VintageLeft = 0;
                            Sample64 VintageRight = 0;
                            if (vintageSample_) {
                                vintageSample_->playStereoSample(&VintageLeft, &VintageRight, speed, dTempo, dSampleRate, true);
                            }
                            outL = outL * (1. - softVintage * .3) + (-sqr(outR * .5) + VintageLeft) * softVintage;
                            outR = outR * (1. - softVintage * .3) + (-sqr(outL * .5) + VintageRight) * softVintage;

                        }

                        /////////////////////////END OF EFFECTOR//////////////////


                        outL = outL * softVolume;
                        outR = outR * softVolume;

                        fPosition = samplesArray_.at(currentEntry_)->cue().integerPart() / double(samplesArray_.at(currentEntry_)->bufferLength());
                        if (outL > fVuLeft) {
                            fVuLeft = outL;
                        }
                        if (outR > fVuRight) {
                            fVuRight = outR;
                        }

                    }

                }

                if (data.symbolicSampleSize == kSample64) {
                    *reinterpret_cast<Sample64*>(ptrOutLeft) = outL;
                    *reinterpret_cast<Sample64*>(ptrOutRight) = outR;
                    ptrOutLeft += sizeof(Sample64);
                    ptrOutRight += sizeof(Sample64);
                } else {
                    *reinterpret_cast<Sample32*>(ptrOutLeft) = outL;
                    *reinterpret_cast<Sample32*>(ptrOutRight) = outR;
                    ptrOutLeft += sizeof(Sample32);
                    ptrOutRight += sizeof(Sample32);
                }
                sampleOffset++;
            }
        }

        if (outParamChanges) {

            if (fVuOldL != fVuLeft) {
                vuLeftWriter.store(data.numSamples - 1, fVuLeft);
            }
            if (fVuOldR != fVuRight) {
                vuRightWriter.store(data.numSamples - 1, fVuRight);
            }
            if (fOldPosition != fPosition) {
                positionWriter.store(data.numSamples - 1, fPosition);
            }

            if ((samplesParamsUpdate || dirtyParams_) && (samplesArray_.size() > currentEntry_)) {

                loopWriter.store(data.numSamples - 1, samplesArray_.at(currentEntry_)->Loop ? 1. : 0.);
                syncWriter.store(data.numSamples - 1, samplesArray_.at(currentEntry_)->Sync ? 1. : 0.);
                levelWriter.store(data.numSamples - 1, samplesArray_.at(currentEntry_)->Level / 2.);
                reverseWriter.store(data.numSamples - 1, samplesArray_.at(currentEntry_)->Reverse ? 1. : 0.);
                tuneWriter.store(data.numSamples - 1, samplesArray_.at(currentEntry_)->Tune > 1. ? samplesArray_.at(currentEntry_)->Tune / 2. : samplesArray_.at(currentEntry_)->Tune - 0.5);
            }

            if (dirtyParams_) {
                sceneWriter.store(data.numSamples - 1, currentScene_ / double(EMaximumScenes - 1.)); //????
                if (samplesArray_.size() > currentEntry_) {
                    entryWriter.store(data.numSamples - 1, currentEntry_ / double(EMaximumSamples - 1.));
                    updatePadsMessage();
                }

                holdWriter.store(data.numSamples - 1, effectorSet_ & eHold ? 1. : 0.);
                freezeWriter.store(data.numSamples - 1, effectorSet_ & eFreeze ? 1. : 0.);
                vintageWriter.store(data.numSamples - 1, effectorSet_ & eVintage ? 1. : 0.);
                distorsionWriter.store(data.numSamples - 1, effectorSet_ & eDistorsion ? 1. : 0.);
                preRollWriter.store(data.numSamples - 1, effectorSet_ & ePreRoll ? 1. : 0.);
                postRollWriter.store(data.numSamples - 1, effectorSet_ & ePostRoll ? 1. : 0.);
                lockWriter.store(data.numSamples - 1, effectorSet_ & eLockTone ? 1. : 0.);
                punchInWriter.store(data.numSamples - 1, effectorSet_ & ePunchIn ? 1. : 0.);
                punchOutWriter.store(data.numSamples - 1, effectorSet_ & ePunchOut ? 1. : 0.);

                dirtyParams_ = false;
            }

            if (bTCLearn != bOldTimecodeLearn) {
                tcLearnWriter.store(data.numSamples - 1, bTCLearn ? 1. : 0.);
            }
            if (fabs(fOldPosition - fPosition) > 0.001) {
                updatePositionMessage(fPosition);
            }
            if (fabs(fOldSpeed - fRealSpeed) > 0.001) {
                updateSpeedMessage(fRealSpeed);
            }
        }
        fVuOldL = fVuLeft;
        fVuOldR = fVuRight;

    }
    catch (std::exception& e) {
        fprintf(stderr, "[Vinyl] exception: ");
        fprintf(stderr, "%s", e.what());
        fprintf(stderr, "\n");
    }
    return kResultOk;
}

tresult PLUGIN_API AVinyl::setProcessing(TBool state)
{
    currentProcessStatus = state;
    return kResultTrue;
}

tresult PLUGIN_API AVinyl::canProcessSampleSize(int32 symbolicSampleSize)
{
    //if (kSample64 == symbolicSampleSize) {
    //    return kResultFalse;
    //}
    return kResultTrue;
}

uint32 PLUGIN_API AVinyl::getLatencySamples()
{
    return ESpeedFrame;
}

uint32 PLUGIN_API AVinyl::getTailSamples()
{
    return ETimecodeLearnCount;
}

void AVinyl::calcDirectionTimeCodeAmplitude()
{
    if ((StatusR) && (deltaRight_ < 0.0)) {
        OldStatusR = StatusR;
        StatusR = false;
        timeCodeAmplytude_ = (63.0 * timeCodeAmplytude_ + fabs(oldSignalRight_)) / 64.0;
    } else if ((!StatusR) && (deltaRight_ > 0.0)) {
        OldStatusR = StatusR;
        StatusR = true;
        timeCodeAmplytude_ = (63.0 * timeCodeAmplytude_ + fabs(oldSignalRight_)) / 64.0;
    }

    if ((StatusL) && (deltaLeft_ < 0.0)) {
        OldStatusL = StatusL;
        StatusL = false;
        timeCodeAmplytude_ = (63.0 * timeCodeAmplytude_ + fabs(oldSignalLeft_)) / 64.0;
    } else if ((!StatusL) && (deltaLeft_ > 0.0)) {
        OldStatusL = StatusL;
        StatusL = true;
        timeCodeAmplytude_ = (63.0 * timeCodeAmplytude_ + fabs(oldSignalLeft_)) / 64.0;
    }

    if (timeCodeAmplytude_ >= ETimeCodeMinAmplytude) {
        if ((StatusL != PStatusL) || (StatusR != PStatusR) ||
            (OldStatusL != POldStatusL) || (OldStatusR != POldStatusR)) {

            if ((StatusL == PStatusR) && (StatusR == POldStatusL) && (OldStatusL == POldStatusR) && (OldStatusR == PStatusL) 
                && (speedCounter_ >= 3)) {
                directionBits_ <<= 8;
                directionBits_ |= 0xff;
                speedCounter_ = 0;
            } else if ((StatusL == POldStatusR) && (StatusR == PStatusL) && (OldStatusL == PStatusR) && (OldStatusR == POldStatusL)
                && (speedCounter_ >= 3)) {
                directionBits_ <<= 8;
                directionBits_ &= 0xffffff00;
                speedCounter_ = 0;
            }
            PStatusL = StatusL;
            PStatusR = StatusR;
            POldStatusL = OldStatusL;
            POldStatusR = OldStatusR;
        }

        if (directionBits_ == 0) {
            direction_ = -1.;
        } else if (directionBits_ == 0xffffffff) {
            direction_ = 1.;
        }

        if (speedCounter_ < 441000) {
            speedCounter_++;
        }
    }
}

void AVinyl::calcAbsSpeed()
{

    size_t maxX = 0;
    Sample64 maxY = 0.f;

    for (int i = 0; i < EFFTFrame; i++) {
        if (maxY < fabs(fftBuffer_[i])) {
            maxY = fabs(fftBuffer_[i]);
        //if (maxY < fabs(fft_[i].real)) {
        //    maxY = fabs(fft_[i].real);
            maxX = i;
        }
    }

    Sample64 tmp = maxX;
    for (size_t i = maxX + 1; i < maxX + 3; i++) {
        if (i < EFFTFrame) {
            Sample64 koef = 100.;
            if (fftBuffer_[i] != 0) {
                koef = (maxY / fftBuffer_[i]) * (maxY / fftBuffer_[i]);
            //if (fft_[i].real != 0) {
            //    koef = (maxY / fft_[i].real) * (maxY / fft_[i].real);
            }
            tmp = (koef * tmp + Sample64(i)) / (koef + 1.);
            continue;
        }
        break;
    }

    for (size_t i = (maxX > 1 ? maxX - 1 : 0); i > maxX - 3; i--) {
        if (i >= 0) {
            Sample64 koef = 100.;
            if (fftBuffer_[i] != 0) {
                koef = (maxY / fftBuffer_[i]) * (maxY / fftBuffer_[i]);
            //if (fft_[i].real != 0) {
            //   koef = (maxY / fft_[i].real) * (maxY / fft_[i].real);
            }
            tmp = (koef * tmp + Sample64(i)) / (koef + 1.);
            continue;
        }
        break;
    }

    if (fabs(tmp - absAvgSpeed_) > 0.7) {
        absAvgSpeed_ = tmp;
    } else {
        absAvgSpeed_.append(tmp);
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

        reader.readInt32u(currentEntry_);
        reader.readInt32u(currentScene_);

        reader.readFloat(fSwitch);
        reader.readFloat(fCurve);

        uint32_t savedBypass;
        reader.readInt32u(savedBypass);
        reader.readInt32(effectorSet_);

        float savedTimeCodeCoeff;
        reader.readFloat(savedTimeCodeCoeff);

        uint32_t savedPadCount;
        uint32_t savedSceneCount;
        uint32_t savedEntryCount;
        reader.readInt32u(savedPadCount);
        reader.readInt32u(savedSceneCount);
        reader.readInt32u(savedEntryCount);

        avgTimeCodeCoeff_ = savedTimeCodeCoeff;

        bBypass = savedBypass > 0;

        keyLockTone = (effectorSet_ & eLockTone) > 0;

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
                    padStates[j][i].padTag = savedTag;
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

            samplesArray_.push_back(std::make_unique<SampleEntry<Sample64>>(sName, sFile));
            samplesArray_.back()->index(samplesArray_.size());
            samplesArray_.back()->Loop = savedLoop > 0;
            samplesArray_.back()->Reverse = savedReverse > 0;
            samplesArray_.back()->Sync = savedSync > 0;
            samplesArray_.back()->Tune = savedTune;
            samplesArray_.back()->Level = savedLevel;

        }

        for (unsigned j = 0; j < savedSceneCount; j++) {
            for (unsigned i = 0; i < savedPadCount; i++) {
                if ((j < EMaximumScenes) && (i < ENumberOfPads)) {
                    padStates[j][i].updateState(currentEntry_, effectorSet_);
                }
            }
        }

        float savedLockedSpeed;
        float savedLockedVolume;
        reader.readFloat(savedLockedSpeed);
        reader.readFloat(savedLockedVolume);

        lockSpeed = savedLockedSpeed;
        lockVolume = savedLockedVolume;

        dirtyParams_ = true;
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
        uint32_t toSaveEntry = currentEntry_;
        uint32_t toSaveScene = currentScene_;
        float toSaveSwitch = fSwitch;
        float toSaveCurve = fCurve;
        float toSaveTimecodeCoeff = avgTimeCodeCoeff_;
        uint32_t toSaveBypass = bBypass ? 1 : 0;
        uint32_t toSaveEffector = effectorSet_;
        uint32_t toSavePadCount = ENumberOfPads;
        uint32_t toSaveSceneCount = EMaximumScenes;
        uint32_t toSaveEntryCount = uint32_t(samplesArray_.size());
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

        for (int i = 0; i < samplesArray_.size(); i++) {
            uint32_t toSavedLoop = samplesArray_.at(i)->Loop ? 1 : 0;
            uint32_t toSavedReverse = samplesArray_.at(i)->Reverse ? 1 : 0;
            uint32_t toSavedSync = samplesArray_.at(i)->Sync ? 1 : 0;
            float toSavedTune = samplesArray_.at(i)->Tune;
            float toSavedLevel = samplesArray_.at(i)->Level;
            String tmpName (samplesArray_.at(i)->name());
            String tmpFile (samplesArray_.at(i)->fileName());
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
    if (numIns > 0 && numIns <= 2 && numOuts == 1) {
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

tresult PLUGIN_API AVinyl::notify(IMessage* message)
{
    if (strcmp(message->getMessageID(), "setPad") == 0) {
        int64 PadNumber;
        if (message->getAttributes()->getInt("PadNumber", PadNumber) == kResultOk) {
            padStates[currentScene_][PadNumber - 1].padState = false;
            int64 PadType;
            if (message->getAttributes()->getInt("PadType", PadType) == kResultOk) {
                padStates[currentScene_][PadNumber - 1].padType = PadEntry::TypePad(PadType);
                int64 PadTag;
                if (message->getAttributes()->getInt("PadTag", PadTag) == kResultOk) {
                    padStates[currentScene_][PadNumber - 1].padTag = PadTag;
                    padStates[currentScene_][PadNumber - 1].updateState(currentEntry_, effectorSet_);
                }
            }
            dirtyParams_ = true;
        }
        return kResultTrue;
    }

    if (strcmp(message->getMessageID(), "touchPad") == 0) {
        int64 PadNumber;
        if (message->getAttributes()->getInt("PadNumber", PadNumber) == kResultOk) {
            double PadValue;
            if (message->getAttributes()->getFloat("PadValue", PadValue) == kResultOk) {
                dirtyParams_ |= padWork(PadNumber, PadValue);
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
                samplesArray_.push_back(std::make_unique<SampleEntry<Sample64>>(newName, newFile));
                samplesArray_.back()->index(samplesArray_.size());
                if (currentEntry_ == (samplesArray_.back()->index() - 1)) {
                    padSet(currentEntry_);
                }
                addSampleMessage(samplesArray_.back().get());
                dirtyParams_ = true;
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
                samplesArray_[currentEntry_] = std::make_unique<SampleEntry<Sample64>>(newName, newFile);
                samplesArray_[currentEntry_]->index(currentEntry_ + 1);
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
                samplesArray_.at(currentEntry_)->name(newName);
            }
        }
        return kResultTrue;
    }

    if (strcmp(message->getMessageID(), "deleteEntry") == 0) {
        int64 sampleIndex {0};
        if (message->getAttributes()->getInt ("SampleNumber", sampleIndex) == kResultOk) {
            delSampleMessage(samplesArray_.at(sampleIndex).get());
            samplesArray_.erase(samplesArray_.begin() + sampleIndex);
            for (size_t i = sampleIndex; i < samplesArray_.size(); i++) {
                samplesArray_.at(i)->index(i + 1);
            }
            padRemove(sampleIndex);
            dirtyParams_ = true;
        }
        currentEntry(currentEntry_);
        return kResultTrue;
    } 

    if (strcmp(message->getMessageID(), "initView") == 0) {
        initSamplesMessage();
        dirtyParams_ = true;
        return kResultTrue;
    }
    return AudioEffect::notify (message);
}

void AVinyl::addSampleMessage(SampleEntry<Sample64>* newSample)
{
    if (newSample) {
        //int64_t sampleInt = int64_t(newSample);
        IMessage* msg = allocateMessage ();
        if (msg) {
            msg->setMessageID("addEntry");
            msg->getAttributes()->setBinary("EntryBufferLeft", newSample->bufferLeft(), uint32_t(newSample->bufferLength() * sizeof(Sample64)));
            msg->getAttributes()->setBinary("EntryBufferRight", newSample->bufferRight(), uint32_t(newSample->bufferLength() * sizeof(Sample64)));
            msg->getAttributes()->setInt("EntryLoop", newSample->Loop ? 1 : 0);
            msg->getAttributes()->setInt("EntrySync", newSample->Sync ? 1 : 0);
            msg->getAttributes()->setInt("EntryReverse", newSample->Reverse ? 1 : 0);
            msg->getAttributes()->setInt("EntryIndex", int64_t(newSample->index()));
            msg->getAttributes()->setInt("EntryBeats", int64_t(newSample->acidBeats()));
            msg->getAttributes()->setFloat("EntryTune", newSample->Tune);
            msg->getAttributes()->setFloat("EntryLevel", newSample->Level);
            msg->getAttributes()->setString("EntryName", String(newSample->name()));
            msg->getAttributes()->setString("EntryFile", String(newSample->fileName()));
            sendMessage(msg);
            msg->release();
        }
    }
}

void AVinyl::delSampleMessage(SampleEntry<Sample64> *delSample)
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

void AVinyl::debugFftMessage(Sample64* fft, size_t len)
{
    if (fft) {
        IMessage* msg = allocateMessage();
        if (msg) {
            msg->setMessageID("debugFft");
            msg->getAttributes()->setBinary("Entry", fft, uint32_t(len * sizeof(Sample64)));
            sendMessage(msg);
            msg->release();
        }
    }
}

void AVinyl::debugInputMessage(Sample64* input, size_t len)
{
    if (input) {
        IMessage* msg = allocateMessage();
        if (msg) {
            msg->setMessageID("debugInput");
            msg->getAttributes()->setBinary("Entry", input, uint32_t(len * sizeof(Sample64)));
            sendMessage(msg);
            msg->release();
        }
    }
}

void AVinyl::initSamplesMessage(void)
{
    for (int32 i = 0; i < samplesArray_.size (); i++) {
        IMessage* msg = allocateMessage();
        if (msg) {
            msg->setMessageID("addEntry");
            msg->getAttributes()->setBinary("EntryBufferLeft", samplesArray_.at(i)->bufferLeft(), uint32_t(samplesArray_.at(i)->bufferLength() * sizeof(Sample64)));
            msg->getAttributes()->setBinary("EntryBufferRight", samplesArray_.at(i)->bufferRight(), uint32_t(samplesArray_.at(i)->bufferLength() * sizeof(Sample64)));
            msg->getAttributes()->setInt("EntryLoop", samplesArray_.at(i)->Loop ? 1 : 0);
            msg->getAttributes()->setInt("EntrySync", samplesArray_.at(i)->Sync ? 1 : 0);
            msg->getAttributes()->setInt("EntryReverse", samplesArray_.at(i)->Reverse ? 1 : 0);
            msg->getAttributes()->setInt("EntryIndex", int64_t(samplesArray_.at(i)->index()));
            msg->getAttributes()->setInt("EntryBeats", int64_t(samplesArray_.at(i)->acidBeats()));
            msg->getAttributes()->setFloat("EntryTune", samplesArray_.at(i)->Tune);
            msg->getAttributes()->setFloat("EntryLevel", samplesArray_.at(i)->Level);
            msg->getAttributes()->setString("EntryName", String(samplesArray_.at(i)->name()));
            msg->getAttributes()->setString("EntryFile", String(samplesArray_.at(i)->fileName()));
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
            msg->getAttributes()->setInt(ttmp1[i].text8(), padStates[currentScene_][i].padState ? 1 : 0);
            ttmp2[i] = tmp.printf("PadType%02d", i);
            msg->getAttributes()->setInt(ttmp2[i].text8(), padStates[currentScene_][i].padType);
            ttmp3[i] = tmp.printf("PadTag%02d", i);
            msg->getAttributes()->setInt(ttmp3[i].text8(), padStates[currentScene_][i].padTag);
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
            if (padStates[currentScene_][i].padMidi == event.noteOn.pitch) {
                dirtyParams_ |= padWork(i, 1.);
            }
            if (padStates[currentScene_][i].padType == PadEntry::AssigMIDI) {
                padStates[currentScene_][i].padType = PadEntry::SamplePad;
                padStates[currentScene_][i].padMidi = event.noteOn.pitch;
                dirtyParams_ = true;
            }
        }
        break;
    case Event::kNoteOffEvent:
        for (int i = 0; i < ENumberOfPads; i++) {
            if (padStates[currentScene_][i].padMidi == event.noteOn.pitch)
                dirtyParams_ |= padWork(i, 0);
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
        speedFrameIndex_ = 0;
        filtredLoLeft_ = 0;
        filtredLoRight_ = 0;
        filterBufferLeft_.reset();
        filterBufferRight_.reset();
        absAvgSpeed_ = 0;
        directionBits_ = 0;
        deltaLeft_ = 0.;
        deltaRight_ = 0.;
        oldSignalLeft_ = 0;
        oldSignalRight_ = 0;
        filtredHiLeft_ = 0.;
        filtredHiRight_ = 0.;
        timeCodeAmplytude_ = 0;
        HoldCounter = 0;
        FreezeCounter = 0;

    }
    else {
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

void AVinyl::currentEntry(int64_t newentry)
{
    if (newentry >= int64_t(samplesArray_.size())) {
        newentry = int64_t(samplesArray_.size()) - 1;
    }
    if (newentry < 0) {
        newentry = 0;
    }

    currentEntry_ = newentry;
    if (newentry < int64_t(samplesArray_.size())) {
        samplesArray_.at(newentry)->resetCursor();
    }
    fPosition = 0;
}

bool AVinyl::padWork(int padId, double paramValue)
{
    bool result = false;
    switch(padStates[currentScene_][padId].padType) {
    case PadEntry::SamplePad:
        if ((padStates[currentScene_][padId].padTag >= 0)
            && (samplesArray_.size() > padStates[currentScene_][padId].padTag)) {
            if (paramValue > 0.5) {
                for (int j = 0; j < EMaximumScenes; j++) {
                    for (int i = 0; i < ENumberOfPads; i++) {
                        if (padStates[j][i].padType == PadEntry::SamplePad) {
                            padStates[j][i].padState = false;
                        }
                    }
                }
                padStates[currentScene_][padId].padState = true;
                currentEntry(padStates[currentScene_][padId].padTag);
                result = true;
            }
        }
        break;
    case PadEntry::SwitchPad:
    {
        if (paramValue > 0.5) {
            padStates[currentScene_][padId].padState = !padStates[currentScene_][padId].padState;
            effectorSet_ ^= padStates[currentScene_][padId].padTag;
            result = true;
        }
    }
    break;
    case PadEntry::KickPad:
    {
        if (paramValue > 0.5) {
            padStates[currentScene_][padId].padState = true;
            effectorSet_ |= padStates[currentScene_][padId].padTag;
        } else {
            padStates[currentScene_][padId].padState = false;
            effectorSet_ &= ~padStates[currentScene_][padId].padTag;
        }
        result = true;
    }
    break;
    default:
        break;
    }
    return result;
}

bool AVinyl::padSet(int currentSample)
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

//void AVinyl::padState(PadEntry &pad)
//{
//    switch(pad.padType){
//    case PadEntry::SamplePad:
//        pad.padState = (pad.padTag == int(iCurrentEntry));
//        break;
//    case PadEntry::SwitchPad:
//        pad.padState = (pad.padTag & effectorSet);
//        break;
//    default:
//        pad.padState = false;
//    }
//}

void AVinyl::padRemove(int currentSample)
{
    for (int j = 0; j < EMaximumScenes; j++) {
        for (int i = 0; i < ENumberOfPads; i++) {
            if (padStates[j][i].padType == PadEntry::SamplePad) {
                if (padStates[j][i].padTag == currentSample) {
                    padStates[j][i].padTag = -1;
                    padStates[j][i].padType = PadEntry::EmptyPad;
                    padStates[j][i].padState = false;
                } else if (padStates[j][i].padTag > currentSample) {
                    padStates[j][i].padTag = padStates[j][i].padTag - 1;
                }
            }
        }
    }
}

//double AVinyl::normalizeTag(int tag)
//{
//    if (padStates[iCurrentScene][tag].padTag < 0) {
//        return 0.;
//    }
//
//    switch (padStates[iCurrentScene][tag].padType) {
//    case PadEntry::SamplePad:
//        if (padStates[iCurrentScene][tag].padTag >= SamplesArray.size()) {
//            return 0.;
//        }
//        return (padStates[iCurrentScene][tag].padTag + 1.) / SamplesArray.size();
//    case PadEntry::SwitchPad:
//        if (padStates[iCurrentScene][tag].padTag >= 5) {
//            return 0.;
//        }
//        return (padStates[iCurrentScene][tag].padTag + 1.) / 5.;
//    case PadEntry::KickPad:
//        if (padStates[iCurrentScene][tag].padTag >= 5) {
//            return 0.;
//        }
//        return (padStates[iCurrentScene][tag].padTag + 1.) / 5.;
//    default:
//        break;
//    }
//    return 0.;
//}

}} // namespaces
