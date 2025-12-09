#include "vinylcontroller.h"
#include "vinylparamids.h"
#include "vinyleditor.h"

#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"

#include "base/source/fstring.h"

#include <stdio.h>
#include <math.h>

namespace {

template <typename T, typename ... Args>
auto make_shared(Args&&... arg) {
    return Steinberg::IPtr<T>(new T(std::forward<Args>(arg)...),
                              true /* vstsdk takes ownership instead of sharing,
                                      probably to support old way of work with raw pointers,
                                      so we need here one extra refcounter inc*/);
}

}

namespace Steinberg {
namespace Vst {

//------------------------------------------------------------------------
// GainParameter Declaration
// example of custom parameter (overwriting to and fromString)
//------------------------------------------------------------------------
class GainParameter : public Parameter
{
public:
	GainParameter (const char* title, int32 flags, int32 id);

    void toString (ParamValue normValue, String128 string) const override;
    bool fromString (const TChar* string, ParamValue& normValue) const override;
};

//------------------------------------------------------------------------
// GainParameter Implementation
//------------------------------------------------------------------------
GainParameter::GainParameter (const char* title, int32 flags, int32 id)
{
    Steinberg::UString(info.title, USTRINGSIZE(info.title)).assign(USTRING(title));
    Steinberg::UString(info.units, USTRINGSIZE(info.units)).assign(USTRING("dB"));
	
	info.flags = flags;
	info.id = id;
	info.stepCount = 0;
    info.defaultNormalizedValue = 1.;
	info.unitId = kRootUnitId;
	
    setNormalized(1.);
}

//------------------------------------------------------------------------
void GainParameter::toString (ParamValue normValue, String128 string) const
{
	char text[32];
    if (normValue > 0.0001) {
        sprintf(text, "%.2f", 20. * log10(normValue));
    } else {
        strcpy(text, "-oo");
	}

    Steinberg::UString(string, 128).fromAscii(text);
}

//------------------------------------------------------------------------
bool GainParameter::fromString (const TChar* string, ParamValue& normValue) const
{
    String wrapper((TChar*)string); // don't know buffer size here!
	double tmp = 0.0;
    if (wrapper.scanFloat(tmp)) {
		// allow only values between -oo and 0dB
        if (tmp > 0.0) {
			tmp = -tmp;
		}

        normValue = exp(log(10.) * tmp / 20.);
		return true;
	}
	return false;
}

//------------------------------------------------------------------------
// AGainController Implementation
//------------------------------------------------------------------------
tresult PLUGIN_API AVinylController::initialize(FUnknown* context)
{
	midiGain = gGain;
	midiScene = gScene;
	midiMix = gMix;
	midiPitch = gPitch;
	midiVolume = gVolume;
	midiTune = gTune;

    tresult result = EditControllerEx1::initialize(context);
    if (result != kResultOk) {
		return result;
	}

	//--- Create Units-------------
	UnitInfo unitInfo;
    //Unit* unit;

	// create root only if you want to use the programListId
	unitInfo.id = kRootUnitId;	// always for Root Unit
	unitInfo.parentUnitId = kNoParentUnitId;	// always for Root Unit
    Steinberg::UString(unitInfo.name, USTRINGSIZE(unitInfo.name)).assign(USTRING("Root"));
	unitInfo.programListId = kNoProgramListId;

    auto unit = make_shared<Unit>(unitInfo);
    addUnit(unit);

	//---Create Parameters------------
	
	//---Gain parameter--
    auto gainParam = make_shared<GainParameter>("Gain", ParameterInfo::kCanAutomate, kGainId);
    parameters.addParameter(gainParam);
    gainParam->setUnitID(kRootUnitId);

    auto volumeParam = make_shared<GainParameter>("Volume", ParameterInfo::kCanAutomate, kVolumeId);
    parameters.addParameter(volumeParam);
    volumeParam->setUnitID(kRootUnitId);

	//---VuMeter parameter---
    parameters.addParameter(STR16("VuLeft"), 0, 0, 0, ParameterInfo::kIsReadOnly, kVuLeftId);
    parameters.addParameter(STR16("VuRight"), 0, 0, 0, ParameterInfo::kIsReadOnly, kVuRightId);
    parameters.addParameter(STR16("Position"), 0, 0, 0, ParameterInfo::kIsReadOnly, kPositionId);
    parameters.addParameter(STR16("Punch In"), 0, 1, 0, ParameterInfo::kIsReadOnly, kPunchInId);
    parameters.addParameter(STR16("Punch Out"), 0, 1, 0, ParameterInfo::kIsReadOnly, kPunchOutId);
    parameters.addParameter(STR16("Distorsion"), 0, 1, 0, ParameterInfo::kIsReadOnly, kDistorsionId);
    parameters.addParameter(STR16("Pre Roll"), 0, 1, 0, ParameterInfo::kIsReadOnly, kPreRollId);
    parameters.addParameter(STR16("Post Roll"), 0, 1, 0, ParameterInfo::kIsReadOnly, kPostRollId);
    parameters.addParameter(STR16("Hold"), 0, 1, 0, ParameterInfo::kIsReadOnly, kHoldId);
    parameters.addParameter(STR16("Freeze"), 0, 1, 0, ParameterInfo::kIsReadOnly, kFreezeId);
    parameters.addParameter(STR16("Vintage"), 0, 1, 0, ParameterInfo::kIsReadOnly, kVintageId);
    parameters.addParameter(STR16("LockTone"), 0, 1, 0, ParameterInfo::kIsReadOnly, kLockToneId);

    auto pitchParam = make_shared<RangeParameter>(STR16 ("Pitch"), kPitchId,STR16 ("%"),0,1,0.5,511, ParameterInfo::kCanAutomate|ParameterInfo::kIsWrapAround, kRootUnitId);
    parameters.addParameter(pitchParam);

	//RangeParameter* padsParam = new RangeParameter (STR16 ("ActivePad"), kCurrentPadId,STR16 ("Number"),1,NumberOfPads,1,NumberOfPads-1, ParameterInfo::kCanAutomate, kRootUnitId);
	//parameters.addParameter (padsParam);

    auto sampleParam = make_shared<RangeParameter>(STR16 ("CurrentSample"), kCurrentEntryId,STR16 ("Number"),1,EMaximumSamples,1,EMaximumSamples-1, ParameterInfo::kCanAutomate|ParameterInfo::kIsWrapAround, kRootUnitId);
    parameters.addParameter(sampleParam);

    auto sceneParam = make_shared<RangeParameter>(STR16 ("CurrentScene"), kCurrentSceneId,STR16 ("Number"),1,EMaximumScenes,1,EMaximumScenes-1, ParameterInfo::kCanAutomate|ParameterInfo::kIsWrapAround, kRootUnitId);
    parameters.addParameter(sceneParam);

    auto switchParam = make_shared<RangeParameter>(STR16 ("PitchSwitch"), kPitchSwitchId,STR16 ("Switch"),0,2,0,2, ParameterInfo::kCanAutomate|ParameterInfo::kIsWrapAround, kRootUnitId);
    parameters.addParameter(switchParam);

    auto curveParam = make_shared<RangeParameter>(STR16 ("VolumeCurve"), kVolCurveId,STR16 ("Curve"),0,2,0,2, ParameterInfo::kCanAutomate|ParameterInfo::kIsWrapAround, kRootUnitId);
    parameters.addParameter(curveParam);

	//---Bypass parameter---
	parameters.addParameter (STR16 ("Bypass"), 0, 1, 0, ParameterInfo::kCanAutomate|ParameterInfo::kIsBypass, kBypassId);
	//---Timecode parameter---
	parameters.addParameter (STR16 ("TimecodeLearn"), 0, 1, 0, ParameterInfo::kCanAutomate|ParameterInfo::kIsWrapAround, kTimecodeLearnId);
	//---Sample params---
	parameters.addParameter (STR16 ("Loop"), 0, 1, 0, 0, kLoopId);
	parameters.addParameter (STR16 ("Sync"), 0, 1, 0, 0, kSyncId);
	parameters.addParameter (STR16 ("Reverse"), 0, 1, 0, 0, kReverseId);

    auto ampParam = make_shared<RangeParameter>(STR16 ("Amp"), kAmpId,STR16 ("<>"),0,1,0.5,511, ParameterInfo::kIsWrapAround, kRootUnitId);
	parameters.addParameter (ampParam);
    auto tuneParam = make_shared<RangeParameter>(STR16 ("Tune"), kTuneId,STR16 ("<>"),0,1,0.5,511, ParameterInfo::kIsWrapAround, kRootUnitId);
	parameters.addParameter (tuneParam);

    return result;
}

//------------------------------------------------------------------------
tresult PLUGIN_API AVinylController::terminate  ()
{
    viewsArray.clear();
	
	return EditControllerEx1::terminate ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API AVinylController::setComponentState (IBStream* state)
{
	// we receive the current state of the component (processor part)
	// we read only the gain and bypass value...
    if (state) {

        int8 byteOrder;
        if (state->read(&byteOrder, sizeof (int8)) == kResultTrue) {

            float savedGain = 0.f;
            if (state->read(&savedGain, sizeof(float)) != kResultOk) {
                return kResultFalse;
            }

            float savedVolume = 0.f;
            if (state->read(&savedVolume, sizeof(float)) != kResultOk) {
                return kResultFalse;
            }

            float savedPitch = 0.f;
            if (state->read(&savedPitch, sizeof(float)) != kResultOk) {
                return kResultFalse;
            }

            DWORD savedEntry = 0;
            if (state->read(&savedEntry, sizeof(float)) != kResultOk) {
                return kResultFalse;
            }

            DWORD savedScene = 0;
            if (state->read(&savedScene, sizeof(float)) != kResultOk) {
                return kResultFalse;
            }

            float savedSwitch = 0.f;
            if (state->read(&savedSwitch, sizeof(float)) != kResultOk) {
                return kResultFalse;
            }

            float savedCurve = 0.f;
            if (state->read(&savedCurve, sizeof(float)) != kResultOk) {
                return kResultFalse;
            }

            if (byteOrder != BYTEORDER) {
                SWAP_32(savedGain)
                SWAP_32(savedVolume)
                SWAP_32(savedPitch)
                SWAP_32(savedEntry)
                SWAP_32(savedScene)
                SWAP_32(savedSwitch)
                SWAP_32(savedCurve)
                SWAP_32(savedGain)
            }

            setParamNormalized(kGainId, savedGain);
            setParamNormalized(kVolumeId, savedVolume);
            setParamNormalized(kPitchId, savedPitch);
            setParamNormalized(kCurrentEntryId, savedEntry/float(EMaximumSamples - 1));
            setParamNormalized(kCurrentSceneId, savedScene/float(EMaximumScenes - 1));
            setParamNormalized(kPitchSwitchId, savedSwitch);
            setParamNormalized(kVolCurveId, savedCurve);

            int8 bypassState;
            if (state->read(&bypassState, sizeof(bypassState)) == kResultTrue) {
                setParamNormalized(kBypassId, bypassState ? 1. : 0.);
            }
        } else {
            return kResultFalse;
        }
	}
	return kResultOk;
}

//------------------------------------------------------------------------
IPlugView* PLUGIN_API AVinylController::createView (const char* name)
{
	// someone wants my editor
    if (name && strcmp (name, "editor") == 0) {
        return new AVinylEditorView (this);
	}
	return 0;
}
//------------------------------------------------------------------------
tresult PLUGIN_API AVinylController::setKnobmode (KnobMode  mode)
{
	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API AVinylController::setState (IBStream* state)
{

	int8 byteOrder;
	if (state->read (&byteOrder, sizeof (int8)) == kResultTrue)
    {

        DWORD savedMidiGain = 0;
        if (state->read(&savedMidiGain, sizeof(float)) != kResultOk) {
            return kResultFalse;
        }

        DWORD savedMidiScene = 0;
        if (state->read(&savedMidiScene, sizeof(float)) != kResultOk) {
            return kResultFalse;
        }

        DWORD savedMidiMix = 0;
        if (state->read(&savedMidiMix, sizeof(float)) != kResultOk) {
            return kResultFalse;
        }

        DWORD savedMidiPitch = 0;
        if (state->read(&savedMidiPitch, sizeof(float)) != kResultOk) {
            return kResultFalse;
        }

        DWORD savedMidiVolume = 0;
        if (state->read(&savedMidiVolume, sizeof(float)) != kResultOk) {
            return kResultFalse;
        }

        DWORD savedMidiTune = 0;
        if (state->read(&savedMidiTune, sizeof(float)) != kResultOk) {
            return kResultFalse;
        }

        if (byteOrder != BYTEORDER){
			SWAP_32(savedMidiGain)
			SWAP_32(savedMidiScene)
			SWAP_32(savedMidiMix)
			SWAP_32(savedMidiPitch)
			SWAP_32(savedMidiVolume)
			SWAP_32(savedMidiTune)
		}
		midiGain = savedMidiGain;
		midiScene = savedMidiScene;
		midiMix = savedMidiMix;
		midiPitch = savedMidiPitch;
		midiVolume = savedMidiVolume;
		midiTune = savedMidiTune;
		return kResultOk;
	}
	
	return kResultFalse;
}

//------------------------------------------------------------------------
tresult PLUGIN_API AVinylController::getState (IBStream* state)
{
	// here we can save UI settings for example

	// as we save a Unicode string, we must know the byteorder when setState is called
	int8 byteOrder = BYTEORDER;

    if (state->write (&byteOrder, sizeof (int8)) == kResultTrue) {
		DWORD toSaveGain = midiGain;
		DWORD toSaveScene = midiScene;
		DWORD toSaveMix = midiMix;
		DWORD toSavePitch = midiPitch;
		DWORD toSaveVolume = midiVolume;
		DWORD toSaveTune = midiTune;
        state->write(&toSaveGain, sizeof (DWORD));
        state->write(&toSaveScene, sizeof (DWORD));
        state->write(&toSaveMix, sizeof (DWORD));
        state->write(&toSavePitch, sizeof (DWORD));
        state->write(&toSaveVolume, sizeof (DWORD));
        state->write(&toSaveTune, sizeof (DWORD));
		return kResultOk;
	}
	return kResultFalse;
}

//------------------------------------------------------------------------
tresult AVinylController::receiveText (const char* text)
{
	// received from Component
	if (text)
	{
		fprintf (stderr, "[VinylController] received: ");
        fprintf (stderr, "%s", text);
		fprintf (stderr, "\n");
	}
	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API AVinylController::setParamNormalized (ParamID tag, ParamValue value)
{
	// called from host to update our parameters state
	tresult result = EditControllerEx1::setParamNormalized (tag, value);
	
    for (auto& view: viewsArray) {
        if (view) {
            static_cast<AVinylEditorView*>(view.get())->update(tag, value);
		}
	}

	return result;
}

//------------------------------------------------------------------------
tresult PLUGIN_API AVinylController::getParamStringByValue (ParamID tag, ParamValue valueNormalized, String128 string)
{
	return EditControllerEx1::getParamStringByValue (tag, valueNormalized, string);
}

//------------------------------------------------------------------------
tresult PLUGIN_API AVinylController::getParamValueByString (ParamID tag, TChar* string, ParamValue& valueNormalized)
{
	return EditControllerEx1::getParamValueByString (tag, string, valueNormalized);
}

//------------------------------------------------------------------------
void AVinylController::editorAttached (EditorView* editor)
{
    AVinylEditorView* view = dynamic_cast<AVinylEditorView*>(editor);
    if (view) {
        viewsArray.push_back(view);
        IMessage* msg = allocateMessage();
        if (msg) {
            msg->setMessageID("initView");
            sendMessage(msg);
            msg->release();
        }
	}
}

//------------------------------------------------------------------------
void AVinylController::editorRemoved (EditorView* editor)
{
    AVinylEditorView* view = dynamic_cast<AVinylEditorView*>(editor);
    if (view) {
        viewsArray.erase(std::remove(viewsArray.begin(), viewsArray.end(), view), viewsArray.end());
	}
}

//------------------------------------------------------------------------
tresult PLUGIN_API AVinylController::queryInterface (const char* iid, void** obj)
{
	QUERY_INTERFACE (iid, obj, IMidiMapping::iid, IMidiMapping)
	return EditControllerEx1::queryInterface (iid, obj);
}

//------------------------------------------------------------------------
tresult PLUGIN_API AVinylController::getMidiControllerAssignment(int32 busIndex, int16 midiChannel,
																 CtrlNumber midiControllerNumber, ParamID& tag)
{

    if (midiControllerNumber == midiGain) {
		tag = kGainId;
		return kResultTrue;
	}
    if (midiControllerNumber == midiScene) {
		tag = kCurrentSceneId;
		return kResultTrue;
	}
    if (midiControllerNumber == midiMix) {
		tag = kVolumeId;
		return kResultTrue;
	}
    if (midiControllerNumber == midiPitch) {
		tag = kPitchId;
		return kResultTrue;
	}
    if (midiControllerNumber == midiVolume) {
		tag = kAmpId;
		return kResultTrue;
	}
    if (midiControllerNumber == midiTune) {
		tag = kTuneId;
		return kResultTrue;
	}
	return kResultFalse;
}

//-----------------------------------------------------------------------------
tresult PLUGIN_API AVinylController::notify (IMessage* message)
{

    if (strcmp (message->getMessageID(), "delEntry") == 0) {
		int64 delEntryInd;
		message->getAttributes()->getInt ("EntryIndex", delEntryInd);

        for(auto& view: viewsArray) {
            static_cast<AVinylEditorView*>(view.get())->delEntry(delEntryInd);
		}

		return kResultTrue;
	}
    if (strcmp (message->getMessageID(), "addEntry") == 0) {
		int64 newEntryInt;
		SampleEntry * newEntry;
		message->getAttributes()->getInt ("Entry", newEntryInt);
		newEntry = (SampleEntry *)newEntryInt;
        for(auto& view: viewsArray) {
            static_cast<AVinylEditorView*>(view.get())->initEntry(newEntry);
        }

		return kResultTrue;
	}
    if (strcmp (message->getMessageID(), "updateSpeed") == 0) {
		double newSpeed;
		message->getAttributes()->getFloat ("Speed", newSpeed);       
        for(auto& view: viewsArray) {
            static_cast<AVinylEditorView*>(view.get())->setSpeedMonitor(newSpeed);
        }

		return kResultTrue;
	}
    if (strcmp (message->getMessageID(), "updatePosition") == 0) {
		double newPosition;
		message->getAttributes()->getFloat ("Position", newPosition);
        for(auto& view: viewsArray) {
            static_cast<AVinylEditorView*>(view.get())->setPositionMonitor(newPosition);
        }

		return kResultTrue;
	}
    if (strcmp (message->getMessageID(), "updatePads") == 0) {

		for (int i = 0; i < ENumberOfPads; i++) {

			int64 newState;
			String tmp;
			tmp = tmp.printf("PadState%02d",i);
			if (message->getAttributes()->getInt (tmp.text8(), newState) == kResultTrue){

                for(auto& view: viewsArray) {
                    static_cast<AVinylEditorView*>(view.get())->setPadState(i, (newState == 1));
                }
            }
			tmp = tmp.printf("PadType%02d",i);
			if (message->getAttributes()->getInt (tmp.text8(), newState) == kResultTrue){

                for(auto& view: viewsArray) {
                    static_cast<AVinylEditorView*>(view.get())->setPadType(i, newState);
                }

            }
			tmp = tmp.printf("PadTag%02d",i);
            if (message->getAttributes()->getInt (tmp.text8(), newState) == kResultTrue) {
                for(auto& view: viewsArray) {
                    static_cast<AVinylEditorView*>(view.get())->setPadTag(i, newState);
                }
            }
		}
		return kResultTrue;
	}
	return EditControllerEx1::notify (message);
}

}} // namespaces
