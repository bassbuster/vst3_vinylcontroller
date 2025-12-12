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
    template<typename T>
    // using SharedPointer = std::unique_ptr<T, void(*)(T*)>;
    using SharedPointer = VSTGUI::SharedPointer<T>;
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
    void initEntry(SampleEntry<Sample32> * newEntry);
    void delEntry(size_t delEntryIndex);
    void setPadState(int _pad, bool _state);
    void setPadType(int _pad, int _type);
    void setPadTag(int _pad, int _tag);
    SharedPointer<VSTGUI::CBitmap> generateWaveform(SampleEntry<Sample32> * newEntry);

    void setSpeedMonitor(double _speed);
    void setPositionMonitor(double _position);
//------------------------------------------------------------------------
private:

    SharedPointer<VSTGUI::CKnob> gainKnob;
    SharedPointer<VSTGUI::CKnob> scenKnob;
    SharedPointer<VSTGUI::CHorizontalSlider> ampSlide;
    SharedPointer<VSTGUI::CHorizontalSlider> tuneSlide;
    SharedPointer<VSTGUI::CTextEdit> nameEdit;
    SharedPointer<VSTGUI::CHorizontalSlider> pitchSlider;
    SharedPointer<VSTGUI::CVerticalSlider> volumeSlider;
    SharedPointer<VSTGUI::CVuMeter> vuLeftMeter;
    SharedPointer<VSTGUI::CVuMeter> vuRightMeter;

    SharedPointer<VSTGUI::CVuMeter> dispDistorsion;
    SharedPointer<VSTGUI::CVuMeter> dispPreRoll;
    SharedPointer<VSTGUI::CVuMeter> dispPostRoll;
    SharedPointer<VSTGUI::CVuMeter> dispHold;
    SharedPointer<VSTGUI::CVuMeter> dispFreeze;
    SharedPointer<VSTGUI::CVuMeter> dispVintage;
    SharedPointer<VSTGUI::CVuMeter> dispLockTone;

    std::array<SharedPointer<VSTGUI::CVinylKickButton>, ENumberOfPads> padKick;//[ENumberOfPads];
    std::array<SharedPointer<VSTGUI::C5PositionView>, ENumberOfPads> padType;//[ENumberOfPads];

    SharedPointer<VSTGUI::CVinylCheckBox> loopBox;
    SharedPointer<VSTGUI::CVinylCheckBox> syncBox;
    SharedPointer<VSTGUI::CVinylCheckBox> rvrsBox;
    SharedPointer<VSTGUI::CVinylCheckBox> tcLearnBox;
    SharedPointer<VSTGUI::C3PositionSwitchBox> curvSwitch;
    SharedPointer<VSTGUI::C3PositionSwitchBox> pitchSwitch;

    SharedPointer<VSTGUI::CWaveView> wavView;

    SharedPointer<VSTGUI::CTextLabel> pitchValue;
    SharedPointer<VSTGUI::CTextLabel> sceneValue;
    SharedPointer<VSTGUI::CTextLabel> speedValue;
    SharedPointer<VSTGUI::CTextLabel> sampleNumber;
    SharedPointer<VSTGUI::CVinylPopupMenu> popupPad;
    SharedPointer<VSTGUI::COptionMenu> samplePopup;
    SharedPointer<VSTGUI::COptionMenu> sampleBase;
    SharedPointer<VSTGUI::COptionMenu> padBase;
    SharedPointer<VSTGUI::COptionMenu> effectBase1;
    SharedPointer<VSTGUI::COptionMenu> effectBase2;

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

	int64_t currentEntry;
	int PadTag[ENumberOfPads];

    std::vector<SharedPointer<VSTGUI::CBitmap>> sampleBitmaps;

    static bool callBeforePopup(VSTGUI::IControlListener*, VSTGUI::CControl*);
    void setPadMessage(int pad,int type,int tag);
    void setPadMessage(int pad,int type);
	int padForSetting;

};

}} // namespaces

