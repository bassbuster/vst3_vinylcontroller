#pragma once

#include "public.sdk/source/vst/vstguieditor.h"
#include "pluginterfaces/vst/ivstplugview.h"
#include "helpers/sampleentry.h"
#include "controls/cvinylbuttons.h"
#include "controls/cwaveview.h"
#include "controls/cdebugfftview.h"
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
    using SharedPointer = VSTGUI::SharedPointer<T>;
public:

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
    tresult PLUGIN_API canResize() override;
    tresult PLUGIN_API checkSizeConstraint(ViewRect* rect) override;

	//---from IParameterFinder---------------
    tresult PLUGIN_API findParameter(int32 xPos, int32 yPos, ParamID& resultTag) override;

    DELEGATE_REFCOUNT (EditorView)
    tresult PLUGIN_API queryInterface(const char* iid, void** obj)  override;

    //---Custom Functions------------------
    void update (ParamID tag, ParamValue value);
    void initEntry(SampleEntry<Sample64> &newEntry);
    void delEntry(size_t delEntryIndex);
    void setPadState(int pad, bool state);
    void setPadType(int pad, int type);
    void setPadTag(int pad, int tag);
    SharedPointer<VSTGUI::CBitmap> generateWaveform(SampleEntry<Sample64> &sample, bool normolize = false);

    void setSpeedMonitor(double speed);
    void setPositionMonitor(double position);


    void debugFft(SampleEntry<Sample64> &sample);
    void debugInput(SampleEntry<Sample64> &sample);

private:

    static bool callBeforePopup(VSTGUI::IControlListener*, VSTGUI::CControl*);
    void setPadMessage(int pad,int type,int tag);
    void setPadMessage(int pad,int type);
    void touchPadMessage(int pad, double value);

    SharedPointer<VSTGUI::CKnob> gainKnob_;
    SharedPointer<VSTGUI::CKnob> scenKnob_;
    SharedPointer<VSTGUI::CHorizontalSlider> ampSlide_;
    SharedPointer<VSTGUI::CHorizontalSlider> tuneSlide_;
    SharedPointer<VSTGUI::CTextEdit> nameEdit_;
    SharedPointer<VSTGUI::CHorizontalSlider> pitchSlider_;
    SharedPointer<VSTGUI::CVerticalSlider> volumeSlider_;
    SharedPointer<VSTGUI::CVuMeter> vuLeftMeter_;
    SharedPointer<VSTGUI::CVuMeter> vuRightMeter_;

    SharedPointer<VSTGUI::CVuMeter> dispDistorsion_;
    SharedPointer<VSTGUI::CVuMeter> dispPreRoll_;
    SharedPointer<VSTGUI::CVuMeter> dispPostRoll_;
    SharedPointer<VSTGUI::CVuMeter> dispHold_;
    SharedPointer<VSTGUI::CVuMeter> dispFreeze_;
    SharedPointer<VSTGUI::CVuMeter> dispVintage_;
    SharedPointer<VSTGUI::CVuMeter> dispLockTone_;

    std::array<SharedPointer<VSTGUI::CVinylKickButton>, ENumberOfPads> padKick_;
    std::array<SharedPointer<VSTGUI::C5PositionView>, ENumberOfPads> padType_;

    SharedPointer<VSTGUI::CVinylCheckBox> loopBox_;
    SharedPointer<VSTGUI::CVinylCheckBox> syncBox_;
    SharedPointer<VSTGUI::CVinylCheckBox> rvrsBox_;
    SharedPointer<VSTGUI::CVinylCheckBox> tcLearnBox_;
    SharedPointer<VSTGUI::C3PositionSwitchBox> curvSwitch_;
    SharedPointer<VSTGUI::C3PositionSwitchBox> pitchSwitch_;

    SharedPointer<VSTGUI::CWaveView> wavView_;
    SharedPointer<VSTGUI::CDebugFftView> debugFftView_;
    SharedPointer<VSTGUI::CDebugFftView> debugInputView_;

    SharedPointer<VSTGUI::CTextLabel> pitchValue_;
    SharedPointer<VSTGUI::CTextLabel> sceneValue_;
    SharedPointer<VSTGUI::CTextLabel> speedValue_;
    SharedPointer<VSTGUI::CTextLabel> sampleNumber_;
    SharedPointer<VSTGUI::CVinylPopupMenu> popupPad_;
    SharedPointer<VSTGUI::COptionMenu> samplePopup_;
    SharedPointer<VSTGUI::COptionMenu> sampleBase_;
    SharedPointer<VSTGUI::COptionMenu> padBase_;
    SharedPointer<VSTGUI::COptionMenu> effectBase1_;
    SharedPointer<VSTGUI::COptionMenu> effectBase2_;

    float lastVuLeftMeterValue_;
    float lastVuRightMeterValue_;
    double lastSpeedValue_;
    double lastPositionValue_;

    bool lastDistort_;
    bool lastPreRoll_;
    bool lastPostRoll_;
    bool lastHold_;
    bool lastFreeze_;
    bool lastVintage_;
    bool lastLockTone_;

    int64_t currentEntry_;
    int padTag_[ENumberOfPads];
    int padForSetting_;

    std::vector<SharedPointer<VSTGUI::CBitmap>> sampleBitmaps_;

};

}} // namespaces

