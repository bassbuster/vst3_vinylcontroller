#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"

namespace Steinberg {
namespace Vst {

//------------------------------------------------------------------------
class AVinylEditorView;

//------------------------------------------------------------------------
// AGainController
//------------------------------------------------------------------------
class AVinylController: public EditControllerEx1, public IMidiMapping
{
    template<typename T>
    using SharedPointer = IPtr<T>;
public:
//------------------------------------------------------------------------
// create function required for plug-in factory,
// it will be called to create new instances of this controller
//------------------------------------------------------------------------
	static FUnknown* createInstance (void* context)
	{
		return (IEditController*)new AVinylController;
	}

	//---from IPluginBase--------
    tresult PLUGIN_API initialize(FUnknown* context) override;
    tresult PLUGIN_API terminate() override;

	//---from EditController-----
    tresult PLUGIN_API setComponentState (IBStream* state) override;
    IPlugView* PLUGIN_API createView (const char* name) override;
    tresult PLUGIN_API setKnobmode (KnobMode  mode);
    tresult PLUGIN_API setState (IBStream* state) override;
    tresult PLUGIN_API getState (IBStream* state) override;
    tresult PLUGIN_API setParamNormalized (ParamID tag, ParamValue value) override;
    tresult PLUGIN_API getParamStringByValue (ParamID tag, ParamValue valueNormalized, String128 string) override;
    tresult PLUGIN_API getParamValueByString (ParamID tag, TChar* string, ParamValue& valueNormalized) override;
    void editorDestroyed (EditorView* editor) override {} // nothing to do here
    void editorAttached (EditorView* editor) override;
    void editorRemoved (EditorView* editor) override;

	//---from ComponentBase-----
    tresult receiveText (const char* text) override;
    tresult PLUGIN_API notify (IMessage* message) override;

	//---from IMidiMapping-----------------
    tresult PLUGIN_API getMidiControllerAssignment (int32 busIndex, int16 channel, CtrlNumber midiControllerNumber, ParamID& tag) override;

	DELEGATE_REFCOUNT (EditController)
    tresult PLUGIN_API queryInterface (const char* iid, void** obj) override;

private:
    std::vector<SharedPointer<EditorView>> viewsArray_;
    int midiGain_;
    int midiScene_;
    int midiMix_;
    int midiPitch_;
    int midiVolume_;
    int midiTune_;

};

}} // namespaces
 

