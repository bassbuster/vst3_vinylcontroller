#include "vinylprocessor.h"
#include "vinylparamids.h"
#include "vinylcids.h"	// for class ids
#include "helpers/parameterwriter.h"

#include "effects/lock.h"
#include "effects/hold.h"
#include "effects/freeze.h"
#include "effects/distortion.h"
#include "effects/roll.h"
#include "effects/punch.h"
#include "effects/vintage.h"

#include "pluginterfaces/base/ibstream.h"
#include "base/source/fstring.h"
#include "base/source/fstreamer.h"

#include <stdio.h>
#include <cmath>

namespace {

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

template<typename T>
inline size_t noteLengthInSamples(T note, T tempo, T sampleRate)
{
    if ((tempo > 0) && (note > 0)) {
        return sampleRate / tempo * 60. * note;
    }
    return 0;
}

}

namespace Steinberg {
namespace Vst {

AVinyl::AVinyl() :
    gain_(0.8),
    realVolume_(0.8),
    vuLeft_(0.),
    vuRight_(0.),
    position_(0.),
    volume_(1.0),
    pitch_(.5),
    realPitch_(1.0),
    currentEntry_(0),
    currentScene_(0),
    switch_(0),
    curve_(0),
    currentProcessMode_(-1), // -1 means not initialized
    bypass_(false),
    sampleRate_(EDefaultSampleRate),
    tempo_(EDefaultTempo),
    noteLength_(0),
    effectorSet_(0),
    currentProcessStatus_(false),
    dirtyParams_(false)
{
    // register its editor class (the same than used in againentry.cpp)
    setControllerClass(AVinylControllerUID);

    effector_.append(std::unique_ptr<Effect>(new Lock(sampleRate_, [this](){ return samplesArray_.at(currentEntry_).get(); })));
    effector_.append(std::unique_ptr<Effect>(new Hold(sampleRate_, noteLength_, [this](){ return samplesArray_.at(currentEntry_).get(); })));
    effector_.append(std::unique_ptr<Effect>(new Freeze(sampleRate_, noteLength_, [this](){ return samplesArray_.at(currentEntry_).get(); })));
    effector_.append(std::unique_ptr<Effect>(new PreRoll([this](){ return samplesArray_.at(currentEntry_).get(); })));
    effector_.append(std::unique_ptr<Effect>(new PostRoll([this](){ return samplesArray_.at(currentEntry_).get(); })));
    effector_.append(std::unique_ptr<Effect>(new Distortion()));
    effector_.append(std::unique_ptr<Effect>(new Vintage(sampleRate_)));
    effector_.append(std::unique_ptr<Effect>(new PunchIn([this]() { return gain_ * speedProcessor_.volume(); })));
    effector_.append(std::unique_ptr<Effect>(new PunchOut()));


    params_.addReader(kBypassId, [this] () { return bypass_ ? 1. : 0.; },
                     [this](Sample64 value) {
                         bypass_ = value > 0.5;
                     });
    params_.addReader(kGainId,   [this] () { return gain_; },
                     [this](Sample64 value) {
                         gain_ = value;
                         realVolume_ = calcRealVolume(gain_, volume_, curve_); },
                     [this] (int32 sampleOffset, int32 currentOffset, Sample64 currentValue) {
                         gain_ = aproxParamValue<Sample32>(sampleOffset, gain_, currentOffset, currentValue);
                         realVolume_ = calcRealVolume(gain_, volume_, curve_);
                     });
    params_.addReader(kVolumeId, [this] () { return volume_; },
                     [this](Sample64 value) {
                         volume_ = value;
                         realVolume_ = calcRealVolume(gain_, volume_, curve_); },
                     [this] (int32 sampleOffset, int32 currentOffset, Sample64 currentValue) {
                         volume_ = aproxParamValue<Sample32>(sampleOffset, volume_, currentOffset, currentValue);
                         realVolume_ = calcRealVolume(gain_, volume_, curve_);
                     });
    params_.addReader(kVolCurveId, [this] () { return curve_; },
                     [this](Sample64 value) {
                         curve_ = value;
                         realVolume_ = calcRealVolume(gain_, volume_, curve_); },
                     [this] (int32 sampleOffset, int32 currentOffset, Sample64 currentValue) {
                         curve_ = aproxParamValue<Sample32>(sampleOffset, curve_, currentOffset, currentValue);
                         realVolume_ = calcRealVolume(gain_, volume_, curve_);
                     });
    params_.addReader(kPitchId, [this] () { return pitch_; },
                     [this](Sample64 value) { pitch_ = value; realPitch_ = calcRealPitch (pitch_, switch_); },
                     [this] (int32 sampleOffset, int32 currentOffset, Sample64 currentValue) {
                         pitch_ = aproxParamValue<Sample32>(sampleOffset, pitch_, currentOffset, currentValue);
                         realPitch_ = calcRealPitch (pitch_, switch_);
                     });
    params_.addReader(kPitchSwitchId, [this] () { return switch_; },
                     [this](Sample64 value) {
                         switch_ = value;
                         realPitch_ = calcRealPitch (pitch_, switch_); },
                     [this] (int32 sampleOffset, int32 currentOffset, Sample64 currentValue) {
                         switch_ = aproxParamValue<Sample32>(sampleOffset, switch_, currentOffset, currentValue);
                         realPitch_ = calcRealPitch (pitch_, switch_);
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

    params_.addReader(kTimecodeLearnId, [this] () { return speedProcessor_.isLearning() ? 1. : 0.; },
                     [this](Sample64 value) {
                         if (value>0.5) {
                             speedProcessor_.startLearn();
                         }
                     });
}

AVinyl::~AVinyl() {
    // nothing to do here yet..
}

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
            padStates_[j][i].padType = PadEntry::SamplePad;
            padStates_[j][i].padTag = j * ENumberOfPads + i;
            padStates_[j][i].padState = false;
            padStates_[j][i].padMidi = gPad[i];
        }
    }

    reset(true);
    dirtyParams_ = false;
    return kResultOk;
}

tresult PLUGIN_API AVinyl::terminate()
{
    // nothing to do here yet...except calling our parent terminate
    samplesArray_.clear();
    return AudioEffect::terminate();
}

tresult PLUGIN_API AVinyl::setActive(TBool state)
{
    reset(state);
    // call our parent setActive
    return AudioEffect::setActive(state);
}

tresult PLUGIN_API AVinyl::process(ProcessData& data)
{
    try {

        bool samplesParamsUpdate = false;
        if (data.processContext) {
            sampleRate_ = data.processContext->sampleRate;
            tempo_ = data.processContext->tempo;
            noteLength_ = noteLengthInSamples(ERollNote, tempo_, sampleRate_);
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

        Sample64 fVuLeft = 0.;
        Sample64 fVuRight = 0.;
        Sample64 fOldPosition = position_;
        bool bOldTimecodeLearn = speedProcessor_.isLearning();
        Sample64 fOldSpeed = speedProcessor_.realSpeed();

        if (numChannels >= 2) {


            int32 sampleFrames = data.numSamples;
            int32 sampleOffset = 0;

            uint8_t* ptrInLeft = in[0];
            uint8_t* ptrOutLeft = out[0];
            uint8_t* ptrInRight = in[1];
            uint8_t* ptrOutRight = out[1];

            while (--sampleFrames >= 0) {

                params_.checkOffset(sampleOffset);

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

                Sample64 inL{0.};
                Sample64 inR{0.};
                Sample64 outL{0.};
                Sample64 outR{0.};

                if (!bypass_) {

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

                    speedProcessor_.process(inL, inR
#ifdef DEVELOPMENT
                                            ,[this](auto fftBuffer, size_t len) { debugInputMessage(fftBuffer, len); }
                                            ,[this](auto fftBuffer, size_t len) { debugFftMessage(fftBuffer, len); }
#endif // DEBUG
                                            );

                    if ((speedProcessor_.volume() >= 0.00001) && (samplesArray_.size() > currentEntry_)) {


                        Sample64 speed = speedProcessor_.realSpeed() * realPitch_;
                        Sample64 tempo = tempo_;
                        Sample64 volume = realVolume_ * speedProcessor_.volume();

                        effector_.activeSet(Effect::Type(effectorSet_));
                        effector_.process(outL, outR, speed, tempo, volume);

                        outL = outL * volume;
                        outR = outR * volume;

                        position_ = samplesArray_.at(currentEntry_)->cue().integerPart() / double(samplesArray_.at(currentEntry_)->bufferLength());
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

            if (vuLeft_ != fVuLeft) {
                vuLeftWriter.store(data.numSamples - 1, fVuLeft);
            }
            if (vuRight_ != fVuRight) {
                vuRightWriter.store(data.numSamples - 1, fVuRight);
            }
            if (fOldPosition != position_) {
                positionWriter.store(data.numSamples - 1, position_);
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

                holdWriter.store(data.numSamples - 1, effectorSet_ & Effect::Hold ? 1. : 0.);
                freezeWriter.store(data.numSamples - 1, effectorSet_ & Effect::Freeze ? 1. : 0.);
                vintageWriter.store(data.numSamples - 1, effectorSet_ & Effect::Vintage ? 1. : 0.);
                distorsionWriter.store(data.numSamples - 1, effectorSet_ & Effect::Distorsion ? 1. : 0.);
                preRollWriter.store(data.numSamples - 1, effectorSet_ & Effect::PreRoll ? 1. : 0.);
                postRollWriter.store(data.numSamples - 1, effectorSet_ & Effect::PostRoll ? 1. : 0.);
                lockWriter.store(data.numSamples - 1, effectorSet_ & Effect::LockTone ? 1. : 0.);
                punchInWriter.store(data.numSamples - 1, effectorSet_ & Effect::PunchIn ? 1. : 0.);
                punchOutWriter.store(data.numSamples - 1, effectorSet_ & Effect::PunchOut ? 1. : 0.);

                dirtyParams_ = false;
            }

            if (speedProcessor_.isLearning() != bOldTimecodeLearn) {
                tcLearnWriter.store(data.numSamples - 1, speedProcessor_.isLearning() ? 1. : 0.);
            }
            if (fabs(fOldPosition - position_) > 0.001) {
                updatePositionMessage(position_);
            }
            if (fabs(fOldSpeed - speedProcessor_.realSpeed()) > 0.001) {
                updateSpeedMessage(speedProcessor_.realSpeed());
            }
        }
        vuLeft_ = fVuLeft;
        vuRight_ = fVuRight;

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
    currentProcessStatus_ = state;
    return kResultTrue;
}

tresult PLUGIN_API AVinyl::canProcessSampleSize(int32 symbolicSampleSize)
{
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

        reader.readFloat(gain_);
        reader.readFloat(volume_);
        reader.readFloat(pitch_);

        reader.readInt32u(currentEntry_);
        reader.readInt32u(currentScene_);

        reader.readFloat(switch_);
        reader.readFloat(curve_);

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

        speedProcessor_.timecode(savedTimeCodeCoeff);

        bypass_ = savedBypass > 0;

        realPitch_ = calcRealPitch (pitch_, switch_);
        realVolume_ = calcRealVolume(gain_, volume_, curve_);

        for (unsigned j = 0; j < savedSceneCount; j++) {
            for (unsigned i = 0; i < savedPadCount; i++) {
                uint32_t savedType;
                uint32_t savedTag;
                uint32_t savedMidi;
                reader.readInt32u(savedType);
                reader.readInt32u(savedTag);
                reader.readInt32u(savedMidi);

                if ((j < EMaximumScenes) && (i < ENumberOfPads)) {
                    padStates_[j][i].padType = PadEntry::TypePad(savedType);
                    padStates_[j][i].padTag = savedTag;
                    padStates_[j][i].padMidi = savedMidi;
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
                    padStates_[j][i].updateState(currentEntry_, effectorSet_);
                }
            }
        }

        float reserved1;
        float reserved2;
        reader.readFloat(reserved1);
        reader.readFloat(reserved2);

        // lockSpeed_ = reserved1;
        // lockVolume_ = reserved2;

        effector_.activeSet(Effect::Type(effectorSet_));

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

        float toSaveGain = gain_;
        float toSaveVolume = volume_;
        float toSavePitch = pitch_;
        uint32_t toSaveEntry = currentEntry_;
        uint32_t toSaveScene = currentScene_;
        float toSaveSwitch = switch_;
        float toSaveCurve = curve_;
        float toSaveTimecodeCoeff = speedProcessor_.timecode();
        uint32_t toSaveBypass = bypass_ ? 1 : 0;
        uint32_t toSaveEffector = effectorSet_;
        uint32_t toSavePadCount = ENumberOfPads;
        uint32_t toSaveSceneCount = EMaximumScenes;
        uint32_t toSaveEntryCount = uint32_t(samplesArray_.size());
        float reserved1 = 0;//lockSpeed_;
        float reserved2 = 0;//lockVolume_;

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

        for (size_t j = 0; j < EMaximumScenes; j++) {
            for (size_t i = 0; i < ENumberOfPads; i++) {
                uint32_t toSaveType = padStates_[j][i].padType;
                uint32_t toSaveTag = padStates_[j][i].padTag;
                uint32_t toSaveMidi = padStates_[j][i].padMidi;
                state->write(&toSaveType, sizeof(uint32_t));
                state->write(&toSaveTag, sizeof(uint32_t));
                state->write(&toSaveMidi, sizeof(uint32_t));
            }
        }

        for (size_t i = 0; i < samplesArray_.size(); i++) {
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

        state->write(&reserved1, sizeof(float));
        state->write(&reserved2, sizeof(float));

        return kResultOk;
    }
    return kResultFalse;
}

tresult PLUGIN_API AVinyl::setupProcessing(ProcessSetup& newSetup) {
    // called before the process call, always in a disable state (not active)

    // here we keep a trace of the processing mode (offline,...) for example.
    currentProcessMode_ = newSetup.processMode;

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
            padStates_[currentScene_][PadNumber - 1].padState = false;
            int64 PadType;
            if (message->getAttributes()->getInt("PadType", PadType) == kResultOk) {
                padStates_[currentScene_][PadNumber - 1].padType = PadEntry::TypePad(PadType);
                int64 PadTag;
                if (message->getAttributes()->getInt("PadTag", PadTag) == kResultOk) {
                    padStates_[currentScene_][PadNumber - 1].padTag = PadTag;
                    padStates_[currentScene_][PadNumber - 1].updateState(currentEntry_, effectorSet_);
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
    for (size_t i = 0; i < samplesArray_.size(); i++) {
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
        for (int i = 0; i < ENumberOfPads; i++) {
            msg->getAttributes()->setInt(String().printf("PadState%02d", i).text8(), padStates_[currentScene_][i].padState ? 1 : 0);
            msg->getAttributes()->setInt(String().printf("PadType%02d", i).text8(), padStates_[currentScene_][i].padType);
            msg->getAttributes()->setInt(String().printf("PadTag%02d", i).text8(), padStates_[currentScene_][i].padTag);
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
            if (padStates_[currentScene_][i].padMidi == event.noteOn.pitch) {
                dirtyParams_ |= padWork(i, 1.);
            }
            if (padStates_[currentScene_][i].padType == PadEntry::AssigMIDI) {
                padStates_[currentScene_][i].padType = PadEntry::SamplePad;
                padStates_[currentScene_][i].padMidi = event.noteOn.pitch;
                dirtyParams_ = true;
            }
        }
        break;
    case Event::kNoteOffEvent:
        for (int i = 0; i < ENumberOfPads; i++) {
            if (padStates_[currentScene_][i].padMidi == event.noteOn.pitch)
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
        speedProcessor_ = SpeedProcessor<Sample64, ESpeedFrame, EFFTFrame, EFilterFrame>();
    } else {
        // reset the VuMeter value
        vuLeft_ = 0.;
        vuRight_ = 0.;
    }
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
    position_ = 0;
}

bool AVinyl::padWork(int padId, double paramValue)
{
    bool result = false;
    switch(padStates_[currentScene_][padId].padType) {
    case PadEntry::SamplePad:
        if ((padStates_[currentScene_][padId].padTag >= 0)
            && (int(samplesArray_.size()) > padStates_[currentScene_][padId].padTag)) {
            if (paramValue > 0.5) {
                for (size_t j = 0; j < EMaximumScenes; j++) {
                    for (size_t i = 0; i < ENumberOfPads; i++) {
                        if (padStates_[j][i].padType == PadEntry::SamplePad) {
                            padStates_[j][i].padState = false;
                        }
                    }
                }
                padStates_[currentScene_][padId].padState = true;
                currentEntry(padStates_[currentScene_][padId].padTag);
                result = true;
            }
        }
        break;
    case PadEntry::SwitchPad:
    {
        if (paramValue > 0.5) {
            padStates_[currentScene_][padId].padState = !padStates_[currentScene_][padId].padState;
            effectorSet_ ^= padStates_[currentScene_][padId].padTag;
            result = true;
        }
    }
    break;
    case PadEntry::KickPad:
    {
        if (paramValue > 0.5) {
            padStates_[currentScene_][padId].padState = true;
            effectorSet_ |= padStates_[currentScene_][padId].padTag;
        } else {
            padStates_[currentScene_][padId].padState = false;
            effectorSet_ &= ~padStates_[currentScene_][padId].padTag;
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
            if (padStates_[j][i].padType == PadEntry::SamplePad) {
                if (padStates_[j][i].padTag == currentSample) {
                    padStates_[j][i].padState = true;
                } else {
                    padStates_[j][i].padState = false;
                }
                result = true;
            }
        }
    }
    return result;
}

void AVinyl::padRemove(int currentSample)
{
    for (int j = 0; j < EMaximumScenes; j++) {
        for (int i = 0; i < ENumberOfPads; i++) {
            if (padStates_[j][i].padType == PadEntry::SamplePad) {
                if (padStates_[j][i].padTag == currentSample) {
                    padStates_[j][i].padTag = -1;
                    padStates_[j][i].padType = PadEntry::EmptyPad;
                    padStates_[j][i].padState = false;
                } else if (padStates_[j][i].padTag > currentSample) {
                    padStates_[j][i].padTag = padStates_[j][i].padTag - 1;
                }
            }
        }
    }
}

}} // namespaces
