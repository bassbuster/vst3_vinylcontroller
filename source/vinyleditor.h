#pragma once

#include "public.sdk/source/vst/vstguieditor.h"
#include "pluginterfaces/vst/ivstplugview.h"
#include "helpers/sampleentry.h"
#include "controls/cvinylbuttons.h"
#include "controls/cwaveview.h"
#include "vinylconfigconst.h"


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
private:

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

    static bool callBeforePopup(VSTGUI::IControlListener*, VSTGUI::CControl*);
	void setPadMessage(int _pad,int _type,int _tag);
	void setPadMessage(int _pad,int _type);
	int padForSetting;
	//bool callBeforePopup(int _tag);

};

}} // namespaces

