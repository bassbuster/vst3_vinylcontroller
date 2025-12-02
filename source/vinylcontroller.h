//------------------------------------------------------------------------
// Project     : VST SDK
// Version     : 3.1.0
//
// Category    : Examples
// Filename    : public.sdk/samples/vst/again/source/againcontroller.h
// Created by  : Steinberg, 04/2005
// Description : AGain Editor Example for VST 3.0
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

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "helpers/sampleentry.h"
#include "vinylconfigconst.h"

#define  READ_SAVED_FLOAT(savedParam)  							\
	float savedParam = 0.f; 												\
		if (state->read (&savedParam, sizeof (float)) != kResultOk) 		\
		{                                                                   \
			return kResultFalse;                                            \
		}


#define  READ_SAVED_DWORD(savedParam)  							\
	DWORD savedParam = 0; 													\
		if (state->read (&savedParam, sizeof (float)) != kResultOk) 		\
		{                                                                   \
			return kResultFalse;                                            \
		}


namespace Steinberg {
namespace Vst {

//------------------------------------------------------------------------
class AVinylEditorView;

//------------------------------------------------------------------------
// AGainController
//------------------------------------------------------------------------
class AVinylController: public EditControllerEx1, public IMidiMapping
{
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

	//---Internal functions-------
	void addDependentView (AVinylEditorView* view);
	void removeDependentView (AVinylEditorView* view);

	//void setDefaultMessageText (String128 text);
	//TChar* getDefaultMessageText ();
//------------------------------------------------------------------------

private:
    std::vector<AVinylEditorView*> viewsArray;
	int midiGain;
	int midiScene;
	int midiMix;
	int midiPitch;
	int midiVolume;
	int midiTune;

//	String128 defaultMessageText;
};

}} // namespaces
 

