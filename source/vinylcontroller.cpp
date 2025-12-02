//------------------------------------------------------------------------
// Project     : VST SDK
// Version     : 3.1.0
//
// Category    : Examples
// Filename    : public.sdk/samples/vst/again/source/againcontroller.cpp
// Created by  : Steinberg, 04/2005
// Description : AGain Controller Example for VST 3.0
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

#include "vinylcontroller.h"
#include "vinylparamids.h"
#include "vinyleditor.h"

#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/base/ustring.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"

#include "base/source/fstring.h"

#include <stdio.h>
#include <math.h>

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

	virtual void toString (ParamValue normValue, String128 string) const;
	virtual bool fromString (const TChar* string, ParamValue& normValue) const;
};

//------------------------------------------------------------------------
// GainParameter Implementation
//------------------------------------------------------------------------
GainParameter::GainParameter (const char* title, int32 flags, int32 id)
{
	Steinberg::UString (info.title, USTRINGSIZE (info.title)).assign (USTRING (title));
	Steinberg::UString (info.units, USTRINGSIZE (info.units)).assign (USTRING ("dB"));
	
	info.flags = flags;
	info.id = id;
	info.stepCount = 0;
	info.defaultNormalizedValue = 1.0f;
	info.unitId = kRootUnitId;
	
	setNormalized (1.f);
}

//------------------------------------------------------------------------
void GainParameter::toString (ParamValue normValue, String128 string) const
{
	char text[32];
	if (normValue > 0.0001)
	{
		sprintf (text, "%.2f", 20 * log10 ((float)normValue));
	}
	else
	{
		strcpy (text, "-oo");
	}

	Steinberg::UString (string, 128).fromAscii (text);
}

//------------------------------------------------------------------------
bool GainParameter::fromString (const TChar* string, ParamValue& normValue) const
{
	String wrapper ((TChar*)string); // don't know buffer size here!
	double tmp = 0.0;
	if (wrapper.scanFloat (tmp))
	{
		// allow only values between -oo and 0dB
		if (tmp > 0.0)
		{
			tmp = -tmp;
		}

		normValue = exp (log (10.f) * (float)tmp / 20.f);
		return true;
	}
	return false;
}

//------------------------------------------------------------------------
// GainParameter Declaration
// example of custom parameter (overwriting to and fromString)
//------------------------------------------------------------------------
//class NumberParameter : public Parameter
//{
//public:
//	NumberParameter (int32 flags,int32 minValue,int32 maxValue, int32 id);
//
//	virtual void toString (ParamValue normValue, String128 string) const;
//	virtual bool fromString (const TChar* string, ParamValue& normValue) const;
//};



//------------------------------------------------------------------------
// AGainController Implementation
//------------------------------------------------------------------------
tresult PLUGIN_API AVinylController::initialize (FUnknown* context)
{
	midiGain = gGain;
	midiScene = gScene;
	midiMix = gMix;
	midiPitch = gPitch;
	midiVolume = gVolume;
	midiTune = gTune;

	tresult result = EditControllerEx1::initialize (context);
	if (result != kResultOk)
	{
		return result;
	}

	//--- Create Units-------------
	UnitInfo unitInfo;
	Unit* unit;

	// create root only if you want to use the programListId
	unitInfo.id = kRootUnitId;	// always for Root Unit
	unitInfo.parentUnitId = kNoParentUnitId;	// always for Root Unit
	Steinberg::UString (unitInfo.name, USTRINGSIZE (unitInfo.name)).assign (USTRING ("Root"));
	unitInfo.programListId = kNoProgramListId;
	
	unit = new Unit (unitInfo);
	addUnit (unit);

	// create a unit1 for the gain
	//unitInfo.id = 1;
	//unitInfo.parentUnitId = kRootUnitId;	// attached to the root unit
	
	//Steinberg::UString (unitInfo.name, USTRINGSIZE (unitInfo.name)).assign (USTRING ("Unit1"));
	
	//unitInfo.programListId = kNoProgramListId;
	
	//unit = new Unit (unitInfo);
	//addUnit (unit);

	//---Create Parameters------------
	
	//---Gain parameter--
	GainParameter* gainParam = new GainParameter ("Gain",ParameterInfo::kCanAutomate, kGainId);
	parameters.addParameter (gainParam);
	gainParam->setUnitID (kRootUnitId);

	GainParameter* volumeParam = new GainParameter ("Volume",ParameterInfo::kCanAutomate, kVolumeId);
	parameters.addParameter (volumeParam);
	volumeParam->setUnitID (kRootUnitId);

	//---VuMeter parameter---
	parameters.addParameter (STR16 ("VuLeft"), 0, 0, 0, ParameterInfo::kIsReadOnly, kVuLeftId);
	parameters.addParameter (STR16 ("VuRight"), 0, 0, 0, ParameterInfo::kIsReadOnly, kVuRightId);
	parameters.addParameter (STR16 ("Position"), 0, 0, 0, ParameterInfo::kIsReadOnly, kPositionId);
	parameters.addParameter (STR16 ("Punch In"), 0, 1, 0, ParameterInfo::kIsReadOnly, kPunchInId);
	parameters.addParameter (STR16 ("Punch Out"), 0, 1, 0, ParameterInfo::kIsReadOnly, kPunchOutId);
	parameters.addParameter (STR16 ("Distorsion"), 0, 1, 0, ParameterInfo::kIsReadOnly, kDistorsionId);
	parameters.addParameter (STR16 ("Pre Roll"), 0, 1, 0, ParameterInfo::kIsReadOnly, kPreRollId);
	parameters.addParameter (STR16 ("Post Roll"), 0, 1, 0, ParameterInfo::kIsReadOnly, kPostRollId);
	parameters.addParameter (STR16 ("Hold"), 0, 1, 0, ParameterInfo::kIsReadOnly, kHoldId);
	parameters.addParameter (STR16 ("Freeze"), 0, 1, 0, ParameterInfo::kIsReadOnly, kFreezeId);
	parameters.addParameter (STR16 ("Vintage"), 0, 1, 0, ParameterInfo::kIsReadOnly, kVintageId);
	parameters.addParameter (STR16 ("LockTone"), 0, 1, 0, ParameterInfo::kIsReadOnly, kLockToneId);

	RangeParameter* pitchParam = new RangeParameter (STR16 ("Pitch"), kPitchId,STR16 ("%"),0,1,0.5,511, ParameterInfo::kCanAutomate|ParameterInfo::kIsWrapAround, kRootUnitId);
	parameters.addParameter (pitchParam);

	//RangeParameter* padsParam = new RangeParameter (STR16 ("ActivePad"), kCurrentPadId,STR16 ("Number"),1,NumberOfPads,1,NumberOfPads-1, ParameterInfo::kCanAutomate, kRootUnitId);
	//parameters.addParameter (padsParam);

	RangeParameter* sampleParam = new RangeParameter (STR16 ("CurrentSample"), kCurrentEntryId,STR16 ("Number"),1,EMaximumSamples,1,EMaximumSamples-1, ParameterInfo::kCanAutomate|ParameterInfo::kIsWrapAround, kRootUnitId);
	parameters.addParameter (sampleParam);

	RangeParameter* sceneParam = new RangeParameter (STR16 ("CurrentScene"), kCurrentSceneId,STR16 ("Number"),1,EMaximumScenes,1,EMaximumScenes-1, ParameterInfo::kCanAutomate|ParameterInfo::kIsWrapAround, kRootUnitId);
	parameters.addParameter (sceneParam);

	RangeParameter* switchParam = new RangeParameter (STR16 ("PitchSwitch"), kPitchSwitchId,STR16 ("Switch"),0,2,0,2, ParameterInfo::kCanAutomate|ParameterInfo::kIsWrapAround, kRootUnitId);
	parameters.addParameter (switchParam);

	RangeParameter* curveParam = new RangeParameter (STR16 ("VolumeCurve"), kVolCurveId,STR16 ("Curve"),0,2,0,2, ParameterInfo::kCanAutomate|ParameterInfo::kIsWrapAround, kRootUnitId);
	parameters.addParameter (curveParam);

	//---Bypass parameter---
	parameters.addParameter (STR16 ("Bypass"), 0, 1, 0, ParameterInfo::kCanAutomate|ParameterInfo::kIsBypass, kBypassId);
	//---Timecode parameter---
	parameters.addParameter (STR16 ("TimecodeLearn"), 0, 1, 0, ParameterInfo::kCanAutomate|ParameterInfo::kIsWrapAround, kTimecodeLearnId);
	//---Sample params---
	parameters.addParameter (STR16 ("Loop"), 0, 1, 0, 0, kLoopId);
	parameters.addParameter (STR16 ("Sync"), 0, 1, 0, 0, kSyncId);
	parameters.addParameter (STR16 ("Reverse"), 0, 1, 0, 0, kReverseId);

	RangeParameter* ampParam = new RangeParameter (STR16 ("Amp"), kAmpId,STR16 ("<>"),0,1,0.5,511, ParameterInfo::kIsWrapAround, kRootUnitId);
	parameters.addParameter (ampParam);
	RangeParameter* tuneParam = new RangeParameter (STR16 ("Tune"), kTuneId,STR16 ("<>"),0,1,0.5,511, ParameterInfo::kIsWrapAround, kRootUnitId);
	parameters.addParameter (tuneParam);
	//parameters.addParameter (STR16 ("Amp"), 0, 1, 0.5, 0, kAmpId);
	//parameters.addParameter (STR16 ("Tune"), 0, 1, 0.5, 0, kTuneId);


	/*for (int i = 0; i < ENumberOfPads; i++) {
		String tmp;
		tmp = tmp.printf("Pad%02d",i+1);
		parameters.addParameter (tmp.text16(), 0, 1, 0, ParameterInfo::kIsReadOnly, kPadId+i);
	}
	for (int i = 0; i < ENumberOfPads; i++) {
		String tmp;
		tmp = tmp.printf("PadType%02d",i+1);
		parameters.addParameter (tmp.text16(), 0, 2, 0, ParameterInfo::kIsReadOnly, kPadTypeId+i);
	}
	for (int i = 0; i < ENumberOfPads; i++) {
		String tmp;
		tmp = tmp.printf("PadVal%02d",i+1);
		parameters.addParameter (tmp.text16(), 0, 2, 0, ParameterInfo::kIsReadOnly, kPadValueId+i);
	}*/

	//---Custom state init------------

	//String str ("Hello World!");
	//str.copyTo (defaultMessageText, 0, 127);

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
            READ_SAVED_FLOAT(savedGain)
            READ_SAVED_FLOAT(savedVolume)
            READ_SAVED_FLOAT(savedPitch)
            READ_SAVED_DWORD(savedEntry)
            READ_SAVED_DWORD(savedScene)
            READ_SAVED_FLOAT(savedSwitch)
            READ_SAVED_FLOAT(savedCurve)

            //READ_SAVED_FLOAT(savedGain,kGainId)
            //READ_SAVED_FLOAT(savedVolume,kVolumeId)
            //READ_SAVED_FLOAT(savedPitch,kPitchId)
            //READ_SAVED_DWORD(savedEntry,kCurrentEntryId)
            //READ_SAVED_DWORD(savedScene,kCurrentSceneId)
            //READ_SAVED_FLOAT(savedSwitch,kPitchSwitchId)
            //READ_SAVED_FLOAT(savedCurve,kVolCurveId)

            if (byteOrder != BYTEORDER) {
        //#if BYTEORDER == kBigEndian
                SWAP_32 (savedGain)
                SWAP_32 (savedVolume)
                SWAP_32 (savedPitch)
                SWAP_32 (savedEntry)
                SWAP_32 (savedScene)
                SWAP_32 (savedSwitch)
                SWAP_32 (savedCurve)
                SWAP_32 (savedGain)
        //#endif
            }

            setParamNormalized (kGainId, savedGain);
            setParamNormalized (kVolumeId, savedVolume);
            setParamNormalized (kPitchId, savedPitch);
            setParamNormalized (kCurrentEntryId, (float)savedEntry/(float)(EMaximumSamples-1));
            setParamNormalized (kCurrentSceneId, (float)savedScene/(float)(EMaximumScenes-1));
            setParamNormalized (kPitchSwitchId, savedSwitch);
            setParamNormalized (kVolCurveId, savedCurve);



            // jump the GainReduction
            //state->seek (sizeof (float), IBStream::kIBSeekCur);

            // read the bypass
            int8 bypassState;
            if (state->read (&bypassState, sizeof (bypassState)) == kResultTrue)
            {
            //    if (byteOrder != BYTEORDER) {
            //#if BYTEORDER == kBigEndian
            //        SWAP_32 (bypassState)
            //#endif
            //    }
                setParamNormalized (kBypassId, bypassState ? 1. : 0.);
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
	if (name && strcmp (name, "editor") == 0)
	{
		AVinylEditorView* view = new AVinylEditorView (this);
		return view;
	}
	return 0;
}
//------------------------------------------------------------------------
tresult PLUGIN_API AVinylController::setKnobmode (KnobMode  mode)
{
	return kResultOk;
	/*switch (mode) {
	case kCircularMode: return kResultOk;
	case kRelativCircularMode: return kResultOk;
	default:
		return kResultFalse;
	}*/
}

//------------------------------------------------------------------------
tresult PLUGIN_API AVinylController::setState (IBStream* state)
{
	tresult result = kResultFalse;

	int8 byteOrder;
	if (state->read (&byteOrder, sizeof (int8)) == kResultTrue)
	{

		READ_SAVED_DWORD(savedMidiGain)
		READ_SAVED_DWORD(savedMidiScene)
		READ_SAVED_DWORD(savedMidiMix)
		READ_SAVED_DWORD(savedMidiPitch)
		READ_SAVED_DWORD(savedMidiVolume)
		READ_SAVED_DWORD(savedMidiTune)
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
		/////////////////////////////////MIDIReMAP
		////////////////////////////////
		//MessageBox(0,STR("READSTATE"),STR("READSTATE"),MB_OK);

		//restartComponent(kMidiCCAssignmentChanged);
		return kResultOk;
	}
	//if ((result = state->read (defaultMessageText, 128 * sizeof (TChar))) != kResultTrue)
	//{
	//	return result;
	//}

	// if the byteorder doesn't match, byte swap the text array ...
	//if (byteOrder != BYTEORDER)
	//{
	//	for (int32 i = 0; i < 128; i++)
	//	{
	//		SWAP_16 (defaultMessageText[i])
	//	}
	//}

	// update our editors
	//for (int32 i = 0; i < viewsArray.total (); i++)
	//{
	//	if (viewsArray.at (i))
	//	{
	//		viewsArray.at (i)->messageTextChanged ();
	//	}
	//}
	
	return kResultFalse;
}

//------------------------------------------------------------------------
tresult PLUGIN_API AVinylController::getState (IBStream* state)
{
	// here we can save UI settings for example

	// as we save a Unicode string, we must know the byteorder when setState is called
	int8 byteOrder = BYTEORDER;

	if (state->write (&byteOrder, sizeof (int8)) == kResultTrue)
	{
		DWORD toSaveGain = midiGain;
		DWORD toSaveScene = midiScene;
		DWORD toSaveMix = midiMix;
		DWORD toSavePitch = midiPitch;
		DWORD toSaveVolume = midiVolume;
		DWORD toSaveTune = midiTune;
		state->write (&toSaveGain, sizeof (DWORD));
		state->write (&toSaveScene, sizeof (DWORD));
		state->write (&toSaveMix, sizeof (DWORD));
		state->write (&toSavePitch, sizeof (DWORD));
		state->write (&toSaveVolume, sizeof (DWORD));
		state->write (&toSaveTune, sizeof (DWORD));
	//	return state->write (defaultMessageText, 128 * sizeof (TChar));
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
            view->update (tag, value);
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
void AVinylController::addDependentView (AVinylEditorView* view)
{
    viewsArray.push_back(view);
	IMessage* msg = allocateMessage ();
	if (msg)
	{
		msg->setMessageID ("initView");
		sendMessage (msg);
		msg->release ();
	}

}

//------------------------------------------------------------------------
void AVinylController::removeDependentView (AVinylEditorView* view)
{
    viewsArray.erase(std::remove(viewsArray.begin(), viewsArray.end(), view), viewsArray.end());
    // for (int32 i = 0; i < viewsArray.total (); i++)
    // {
    // 	if (viewsArray.at (i) == view)
    // 	{
    // 		viewsArray.removeAt (i);
    // 		break;
    // 	}
    // }
}

//------------------------------------------------------------------------
void AVinylController::editorAttached (EditorView* editor)
{
    AVinylEditorView* view = dynamic_cast<AVinylEditorView*>(editor);
    if (view) {
        addDependentView(view);
	}
}

//------------------------------------------------------------------------
void AVinylController::editorRemoved (EditorView* editor)
{
    AVinylEditorView* view = dynamic_cast<AVinylEditorView*>(editor);
    if (view) {
        removeDependentView(view);
	}
}

//------------------------------------------------------------------------
//void AVinylController::setDefaultMessageText (String128 text)
//{
   //	String tmp (text);
   //	tmp.copyTo (defaultMessageText, 0, 127);
//}

//------------------------------------------------------------------------
//TChar* AVinylController::getDefaultMessageText ()
//{
//	return defaultMessageText;
//}

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


	// we support for the Gain parameter all Midi Channel
	if (midiControllerNumber == midiGain)
	{
//MessageBox(0,STR("MIDIMAP"),STR("MIDIMAP"),MB_OK);
		tag = kGainId;
		return kResultTrue;
	}
	if (midiControllerNumber == midiScene)
	{
		tag = kCurrentSceneId;
		return kResultTrue;
	}
	if (midiControllerNumber == midiMix)
	{
		tag = kVolumeId;
		return kResultTrue;
	}
	if (midiControllerNumber == midiPitch)
	{
		tag = kPitchId;
		return kResultTrue;
	}
	if (midiControllerNumber == midiVolume)
	{
		tag = kAmpId;
		return kResultTrue;
	}
	if (midiControllerNumber == midiTune)
	{
		tag = kTuneId;
		return kResultTrue;
	}
	return kResultFalse;
}

//-----------------------------------------------------------------------------
tresult PLUGIN_API AVinylController::notify (IMessage* message)
{

//MessageBox(0,STR("123"),STR("123"),MB_OK);
	if (strcmp (message->getMessageID(), "delEntry") == 0)
	{
		int64 delEntryInd;
		message->getAttributes()->getInt ("EntryIndex", delEntryInd);
//String ttmm;
//ttmm = ttmm.printf("%d",delEntryInd);

//MessageBox(0,ttmm.text16(),STR("123"),MB_OK);

        for(auto& view: viewsArray) {
            view->delEntry(delEntryInd);
		}

		return kResultTrue;
	}
	if (strcmp (message->getMessageID(), "addEntry") == 0)
	{
		int64 newEntryInt;
		SampleEntry * newEntry;
		message->getAttributes()->getInt ("Entry", newEntryInt);
		newEntry = (SampleEntry *)newEntryInt;
        for(auto& view: viewsArray) {
            view->initEntry(newEntry);
        }

		return kResultTrue;
	}
	if (strcmp (message->getMessageID(), "updateSpeed") == 0)
	{
		double newSpeed;
		message->getAttributes()->getFloat ("Speed", newSpeed);       
        for(auto& view: viewsArray) {
            view->setSpeedMonitor(newSpeed);
        }

		return kResultTrue;
	}
	if (strcmp (message->getMessageID(), "updatePosition") == 0)
	{
		double newPosition;
		message->getAttributes()->getFloat ("Position", newPosition);
        for(auto& view: viewsArray) {
            view->setPositionMonitor(newPosition);
        }

		return kResultTrue;
	}
	if (strcmp (message->getMessageID(), "updatePads") == 0)
	{

		for (int i = 0; i < ENumberOfPads; i++) {

			int64 newState;
			String tmp;
			tmp = tmp.printf("PadState%02d",i);
			if (message->getAttributes()->getInt (tmp.text8(), newState) == kResultTrue){

                for(auto& view: viewsArray) {
                    view->setPadState(i, (newState == 1));
                }

            }
			tmp = tmp.printf("PadType%02d",i);
			if (message->getAttributes()->getInt (tmp.text8(), newState) == kResultTrue){

				//String ttmp;
				//ttmp = ttmp.printf("%s = %d",tmp.text8(),newState);
				//MessageBox(0,ttmp.text16(),STR("123"),MB_OK);
                for(auto& view: viewsArray) {
                    view->setPadType(i, newState);
                }

            }
			tmp = tmp.printf("PadTag%02d",i);
            if (message->getAttributes()->getInt (tmp.text8(), newState) == kResultTrue) {
                for(auto& view: viewsArray) {
                    view->setPadTag(i, newState);
                }
            }
		}
		return kResultTrue;
	}
	return EditControllerEx1::notify (message);
}

}} // namespaces
