//------------------------------------------------------------------------
// Project     : VST SDK
// Version     : 3.1.0
//
// Category    : Examples
// Filename    : public.sdk/samples/vst/again/source/againeditor.h
// Created by  : Steinberg, 04/2005
// Description : AGain Editor Example using VSTGUI 3.5
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

#include "public.sdk/source/vst/vstguieditor.h"
#include "pluginterfaces/vst/ivstplugview.h"
#include "helpers/sampleentry.h"
#include "controls/cvinylbuttons.h"
#include "controls/cwaveview.h"
#include "vinylconfigconst.h"


#define CASE_BEGIN_EDIT(a,b)                         	\
	case a:                                         	\
		{                                             	\
			controller->beginEdit (b);                	\
		}	break;

#define CASE_END_EDIT(a,b)								\
	case a:                                           \
		{                                               \
			controller->endEdit (b);                    \
		}	break;

#define CASE_UPDATE(a,b)                               	\
		case a:                                             \
			if (b)                                          \
			{                                               \
                b->setValueNormalized ((float)value); \
			}                                               \
			break;

#define CASE_UPDATE_MINUS(a,b)                              \
		case a:                                             \
			if (b)                                          \
			{                                               \
                b->setValueNormalized (1.0 - (float)value); \
			}                                               \
			break;


#define CASE_VALUE_CHANGED(a,b)                                           	\
		case a:                                                             \
		{                                                                   \
			controller->setParamNormalized (b, pControl->getValueNormalized ()); 		\
			controller->performEdit (b, pControl->getValueNormalized ());        		\
		}	break;

#define CASE_PAD_CHANGED(a,b)                                                     \
		case a:                                                              \
		{                                                                         \
			IMessage* msg = controller->allocateMessage ();                       \
			if (msg)                                                              \
			{                                                                     \
				String sampleName (nameEdit->getText());                          \
				msg->setMessageID ("touchPad");                                   \
				msg->getAttributes ()->setInt("PadNumber", b);                    \
				msg->getAttributes ()->setFloat("PadValue", pControl->getValueNormalized());                    \
				controller->sendMessage (msg);                                    \
				msg->release ();                                                  \
			}                                                                     \
		}	break;


namespace Steinberg {
namespace Vst {

//------------------------------------------------------------------------
// AGainEditorView Declaration
//------------------------------------------------------------------------
class AVinylEditorView:	public VSTGUIEditor,
                        public VSTGUI::IControlListener,
						public IParameterFinder
{
public:
//------------------------------------------------------------------------
    AVinylEditorView(void* controller);

	//---from VSTGUIEditor---------------
    bool PLUGIN_API open(void* parent, const VSTGUI::PlatformType& platformType) override;
    void PLUGIN_API close() override;
    VSTGUI::CMessageResult notify(CBaseObject* sender, const char* message) override;
    void beginEdit(long /*index*/) {}
    void endEdit(long /*index*/) {}

	//---from CControlListener---------
    void valueChanged(VSTGUI::CControl *pControl) override;
    void controlBeginEdit(VSTGUI::CControl* pControl) override;
    void controlEndEdit(VSTGUI::CControl* pControl) override;

	//---from EditorView---------------
    tresult PLUGIN_API onSize(ViewRect* newSize) override;
    tresult PLUGIN_API canResize() override { return kResultTrue; }
    tresult PLUGIN_API checkSizeConstraint(ViewRect* rect) override;

	//---from IParameterFinder---------------
    tresult PLUGIN_API findParameter(int32 xPos, int32 yPos, ParamID& resultTag) override;

    DELEGATE_REFCOUNT (EditorView)
    tresult PLUGIN_API queryInterface(const char* iid, void** obj)  override;

    //---Custom Function------------------
    void update (ParamID tag, ParamValue value);
    //void messageTextChanged ();
    //void newEntry(SampleEntry * newEntry);
    void initEntry(SampleEntry * newEntry);
    void delEntry(int64 delEntryIndex);
    void setPadState(int _pad, bool _state);
    void setPadType(int _pad, int _type);
    void setPadTag(int _pad, int _tag);
    VSTGUI::CBitmap * generateWaveform(SampleEntry * newEntry);

	//double fSpeedMonitor;
    void setSpeedMonitor(double _speed);
    void setPositionMonitor(double _position);
//------------------------------------------------------------------------
protected:

    VSTGUI::CKnob* gainKnob;
    VSTGUI::CKnob* scenKnob;
    VSTGUI::CHorizontalSlider* ampSlide;
    VSTGUI::CHorizontalSlider* tuneSlide;
    VSTGUI::CTextEdit* nameEdit;
	//CTextEdit* fileEdit;
    VSTGUI::CHorizontalSlider* pitchSlider;
    VSTGUI::CVerticalSlider* volumeSlider;
    VSTGUI::CVuMeter* vuLeftMeter;
    VSTGUI::CVuMeter* vuRightMeter;

    VSTGUI::CVuMeter* dispDistorsion;
    VSTGUI::CVuMeter* dispPreRoll;
    VSTGUI::CVuMeter* dispPostRoll;
    VSTGUI::CVuMeter* dispHold;
    VSTGUI::CVuMeter* dispFreeze;
    VSTGUI::CVuMeter* dispVintage;
    VSTGUI::CVuMeter* dispLockTone;
	//CVuMeter* dispPunchIn;
	//CVuMeter* dispPunchOut;

    VSTGUI::CVinylKickButton * padKick[ENumberOfPads];
    VSTGUI::C5PositionView * padType[ENumberOfPads];
	//CVinylKickButton * sampleCombo;
    VSTGUI::CVinylCheckBox * loopBox;
    VSTGUI::CVinylCheckBox * syncBox;
    VSTGUI::CVinylCheckBox * rvrsBox;
    VSTGUI::CVinylCheckBox * tcLearnBox;
    VSTGUI::C3PositionSwitchBox * curvSwitch;
    VSTGUI::C3PositionSwitchBox * pitchSwitch;

    VSTGUI::CWaveView * wavView;

    VSTGUI::CTextLabel * pitchValue;
    VSTGUI::CTextLabel * sceneValue;
    VSTGUI::CTextLabel * speedValue;
    VSTGUI::CTextLabel * sampleNumber;
    VSTGUI::CVinylPopupMenu * popupPad;
    VSTGUI::COptionMenu * samplePopup;
    VSTGUI::COptionMenu * sampleBase;
    VSTGUI::COptionMenu * padBase;
    VSTGUI::COptionMenu * effectBase1;
    VSTGUI::COptionMenu * effectBase2;

	float lastVuLeftMeterValue;
	float lastVuRightMeterValue;
	double lastSpeedValue;
	double lastPositionValue;

	bool lastDistort;
	bool lastPreRoll;
	bool lastPostRoll;
	bool lastHold;
	bool lastFreeze;
	bool lastVintage;
	bool lastLockTone;

	int64 currentEntry;
	int PadTag[ENumberOfPads];

	//int64 currentScene;
    std::vector<VSTGUI::CBitmap*> sampleBitmaps;
private:
    static bool callBeforePopup(VSTGUI::IControlListener*, VSTGUI::CControl*);
	void setPadMessage(int _pad,int _type,int _tag);
	void setPadMessage(int _pad,int _type);
	int padForSetting;
	//bool callBeforePopup(int _tag);

};

}} // namespaces

