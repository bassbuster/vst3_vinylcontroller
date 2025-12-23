#include "pluginterfaces/base/geoconstants.h"
#include "vinyleditor.h"
#include "vinylcontroller.h"
#include "vinylparamids.h"
#include "helpers/padentry.h"

#include "base/source/fstring.h"
#include "vstgui/standalone/source/genericalertbox.h"

#include <cmath>
#include <filesystem>

namespace {

inline double sqr(double x) {
    return x * x;
}

template <typename T, typename ... Args>
auto make_shared(Args&&... arg) {
    return VSTGUI::SharedPointer<T>(new T(std::forward<Args>(arg)...), false);
}

}

namespace Steinberg {
namespace Vst {

const char * effectNames[9] = {"Distorsion", "PreRoll", "PostRoll", "PunchIn", "PunchOut", "Hold", "Freeze", "Vintage", "LockTone"};
int switchedFx[6] = {eDistorsion, ePreRoll, ePostRoll, eHold, eVintage, eLockTone};
int punchFx[9] = {eDistorsion, ePreRoll, ePostRoll, ePunchIn, ePunchOut, eHold, eFreeze, eVintage, eLockTone};

//------------------------------------------------------------------------
enum
{
    // UI size
    kEditorMinWidth  = 252,
    kEditorMaxWidth  = 630,
    kEditorHeight = 246
};

/// \cond ignore

//------------------------------------------------------------------------
// AGainEditorView Implementation
//------------------------------------------------------------------------
AVinylEditorView::AVinylEditorView (void* controller)
    : VSTGUIEditor(controller)
    , lastVuLeftMeterValue_(0.f)
    , lastVuRightMeterValue_(0.f)
    , currentEntry_(-1)
    , lastSpeedValue_(0)
    , lastPositionValue_(0)
{
    setIdleRate(50); // 1000ms/50ms = 20Hz
    setRect ({0, 0, kEditorMaxWidth, kEditorHeight});
}

//------------------------------------------------------------------------
tresult PLUGIN_API AVinylEditorView::onSize (ViewRect* newSize)
{
    return VSTGUIEditor::onSize (newSize);
}

tresult AVinylEditorView::canResize() {
#ifdef DEVELOPMENT
    return kResultTrue;
#else
    return kResultFalse;
#endif
}

//------------------------------------------------------------------------
tresult PLUGIN_API AVinylEditorView::checkSizeConstraint (ViewRect* rect)
{
    if ((rect->right - rect->left) <= ((kEditorMinWidth + kEditorMaxWidth) / 2)) {
        rect->right = rect->left + kEditorMinWidth;
    } else if ((rect->right - rect->left) > ((kEditorMinWidth + kEditorMaxWidth) / 2)) {
        rect->right = rect->left + kEditorMaxWidth;
    }
    rect->bottom = rect->top + kEditorHeight;

    return VSTGUIEditor::checkSizeConstraint(rect);
}

//------------------------------------------------------------------------
tresult PLUGIN_API AVinylEditorView::findParameter (int32 xPos, int32 yPos, ParamID& resultTag)
{
    // look up the parameter (view) which is located at xPos/yPos.

    VSTGUI::CPoint where (xPos, yPos);

    // Implementation 1:
    // The parameter xPos/yPos are relative coordinates to the AGainEditorView coordinates.
    // If the window of the VST3 plugin is moved, xPos is always >= 0 and <= AGainEditorView width.
    // yPos is always >= 0 and <= AGainEditorView height.
    //
    // gainSlider->hitTest() is a short cut for:
    // CRect sliderRect = gainSlider->getMouseableArea ();
    // if (where.isInside (sliderRect))
    // {
    //      resultTag = kGainId;
    //      return kResultOk;
    // }


    // test wether xPos/yPos is inside the gainSlider.
    if (volumeSlider_->hitTest(where, 0)) {
        resultTag = kVolumeId;
        return kResultOk;
    }
    if (pitchSlider_->hitTest(where, 0)) {
        resultTag = kPitchId;
        return kResultOk;
    }
    if (gainKnob_->hitTest(where, 0)) {
        resultTag = kGainId;
        return kResultOk;
    }
    if (scenKnob_->hitTest(where, 0)) {
        resultTag = kCurrentSceneId;
        return kResultOk;
    }
    if (loopBox_->hitTest(where, 0)) {
        resultTag = kLoopId;
        return kResultOk;
    }
    if (syncBox_->hitTest(where, 0)) {
        resultTag = kSyncId;
        return kResultOk;
    }
    if (rvrsBox_->hitTest (where, 0)) {
        resultTag = kReverseId;
        return kResultOk;
    }
    if (curvSwitch_->hitTest(where, 0)) {
        resultTag = kVolCurveId;
        return kResultOk;
    }
    if (pitchSwitch_->hitTest(where, 0)) {
        resultTag = kPitchSwitchId;
        return kResultOk;
    }
    if (ampSlide_->hitTest(where, 0)) {
        resultTag = kAmpId;
        return kResultOk;
    }
    if (tuneSlide_->hitTest(where, 0)) {
        resultTag = kTuneId;
        return kResultOk;
    }


    // Implementation 2:
    // An alternative solution with VSTGui can look like this. (This requires C++ RTTI)
    //
    // if (frame)
    // {
    //  CControl* controlAtPos = dynamic_cast<CControl*>(frame->getViewAt (where, true);
    //  if (controlAtPos)
    //  {
    //      switch (controlAtPos->getTag ())
    //      {
    //          case 'Gain':
    //          case 'GaiT':
    //              resultTag = resultTag;
    //              return kResultOk;
    //      }
    //  }
    //

    return kResultFalse;
}

//------------------------------------------------------------------------
tresult PLUGIN_API AVinylEditorView::queryInterface (const char* iid, void** obj)
{
    QUERY_INTERFACE (iid, obj, IParameterFinder::iid, IParameterFinder)
    return VSTGUIEditor::queryInterface (iid, obj);
}

int bitNumber(int _bitSet)
{
    int testBit = 1;
    if (_bitSet == 0) {
        return -1;
    }
    for (int i = 0; i < 32; i++) {
        if (_bitSet & testBit) {
            return i;
        }
        testBit = testBit<<1;
    }
    return -1;
}

//------------------------------------------------------------------------
bool PLUGIN_API AVinylEditorView::open (void* parent, const VSTGUI::PlatformType& platformType)
{
    VSTGUI::CRect editorSize (0, 0, kEditorMaxWidth, kEditorHeight);
    VSTGUI::CRect size;
    frame = new VSTGUI::CFrame(editorSize, this);
    {
        auto frameback = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("background.png"));
        frame->setBackground(frameback);
    }

    ////////////////PopUp

    sampleBase_ = make_shared<VSTGUI::COptionMenu>(size, this, 'Base', nullptr, nullptr, VSTGUI::CVinylPopupMenu::kCheckStyle);
    sampleBase_->addEntry(EEmptyBaseTitle, 0, VSTGUI::CMenuItem::kTitle | VSTGUI::CMenuItem::kDisabled);

    padBase_ = make_shared<VSTGUI::COptionMenu>(size, this, 'BsPd', nullptr, nullptr, VSTGUI::CVinylPopupMenu::kCheckStyle);
    padBase_->addEntry(EEmptyBaseTitle, 0, VSTGUI::CMenuItem::kTitle | VSTGUI::CMenuItem::kDisabled);

    effectBase1_ = make_shared<VSTGUI::COptionMenu>(size, this, 'BsE1', nullptr, nullptr, VSTGUI::CVinylPopupMenu::kCheckStyle);
    for (size_t i = 0; i < sizeof(switchedFx) / sizeof(int); i++) {
        effectBase1_->addEntry(effectNames[bitNumber(switchedFx[i])], int32_t(i), 0);
    }

    effectBase2_ = make_shared<VSTGUI::COptionMenu>(size, this, 'BsE2', nullptr, nullptr, VSTGUI::CVinylPopupMenu::kCheckStyle);
    for (size_t i = 0; i < sizeof(punchFx)/sizeof(int); i++) {
        effectBase2_->addEntry(effectNames[bitNumber(punchFx[i])], int32_t(i), 0);
    }

    {
        auto menuOn = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("fx_yellow.png"));
        popupPad_ = make_shared<VSTGUI::CVinylPopupMenu>(this, 'PpUp', menuOn, VSTGUI::CRect(-4, -4, 4, 4));
    }
    popupPad_->addEntry("Assign MIDI key",0,0);
    popupPad_->addEntry("-", 1, VSTGUI::CMenuItem::kSeparator);
    popupPad_->addEntry(padBase_, "Select sample");
    popupPad_->addEntry(effectBase1_, "Switch Fxs");
    popupPad_->addEntry(effectBase2_, "Kick Fxs");
    popupPad_->addEntry("-", 5, VSTGUI::CMenuItem::kSeparator);
    popupPad_->addEntry("Clear", 6, 0);


    //---Knobs-------
    VSTGUI::CPoint offset (0,0);
    {
        auto handle = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("knob_handle.png"));
        auto background = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("gain_back.png"));

        size(0, 0, 24, 24);
        size.offset(17, 17);
        gainKnob_ = make_shared<VSTGUI::CKnob>(size, this, 'Gain', background, handle, offset);
        frame->addView(gainKnob_);

        size.offset (63, 0);
        scenKnob_ = make_shared<VSTGUI::CKnob>(size, this, 'Scen', background, handle, offset);
        scenKnob_->setMin(1);
        scenKnob_->setMax(EMaximumScenes);
        frame->addView(scenKnob_);
    }

    //---Pitch slider-------
    {
        auto handle = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("pitch_handler.png"));
        auto background = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("pitch_slider.png"));

        size(0, 0, 350, 39);
        size.offset(267, 182);
        pitchSlider_ = make_shared<VSTGUI::CHorizontalSlider>(size, this, 'Pith', offset, size.getWidth(), handle, background, offset, VSTGUI::CSliderBase::kLeft);
        frame->addView(pitchSlider_);
    }


    //---Mix slider-------
    {
        auto handle = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("volume_handler.png"));
        auto background = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("volume_slider.png"));
        size(0, 0, 38, 145);
        size.offset(9, 75);
        volumeSlider_ = make_shared<VSTGUI::CVerticalSlider>(size, this, 'Fadr', offset, size.getHeight(), handle, background, offset);
        frame->addView(volumeSlider_);
    }


    //---VuMeter--------------------
    {
        auto onBitmap = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("vu_on.png"));
        auto offBitmap = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("vu_off.png"));

        size(0, 0, 352, 11);
        size.offset(265, 142);
        vuLeftMeter_ = make_shared<VSTGUI::CVuMeter>(size, onBitmap, offBitmap, 37, VSTGUI::CVuMeter::Style::kHorizontal);
        frame->addView(vuLeftMeter_);
        size.offset(0, 12);
        vuRightMeter_ = make_shared<VSTGUI::CVuMeter>(size, onBitmap, offBitmap, 37, VSTGUI::CVuMeter::Style::kHorizontal);
        frame->addView(vuRightMeter_);
    }

    //---3posSwitch--------------------
    {
        auto oPos1 = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("curve_1.png"));
        auto oPos2 = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("curve_2.png"));
        auto oPos3 = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("curve_3.png"));

        size(0, 0, 18, 10);
        size.offset(18, 225);
        curvSwitch_ = make_shared<VSTGUI::C3PositionSwitchBox>(size, this, 'Curv', oPos1, oPos2, oPos3, offset);
        frame->addView(curvSwitch_);
    }

    {
        auto oPos1 = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("pitch_1.png"));
        auto oPos2 = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("pitch_2.png"));
        auto oPos3 = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("pitch_3.png"));
        size(0, 0, 28, 10);
        size.offset(264, 226);
        pitchSwitch_ = make_shared<VSTGUI::C3PositionSwitchBox>(size, this, 'PtCr', oPos1, oPos2, oPos3, offset);
        frame->addView(pitchSwitch_);
    }


    //---PadTypes--------------------
    {
        auto padType1 = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("fx_blue.png"));
        auto padType2 = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("fx_green.png"));
        auto padType3 = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("x_red.png"));
        auto padType4 = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("x_empty.png"));

        size(0, 0, 47, 47);
        size.offset(61, 52);
        padType_[0] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt00', padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType_[0]);
        size.offset(47, 0);
        padType_[1] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt01', padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType_[1]);
        size.offset(47, 0);
        padType_[2] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt02', padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType_[2]);
        size.offset(47, 0);
        padType_[3] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt03', padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType_[3]);
        size.offset(-141, 47);
        padType_[4] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt04', padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType_[4]);
        size.offset(47, 0);
        padType_[5] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt05', padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType_[5]);
        size.offset(47, 0);
        padType_[6] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt06', padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType_[6]);
        size.offset(47, 0);
        padType_[7] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt07', padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType_[7]);
        size.offset(-141, 47);
        padType_[8] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt08', padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType_[8]);
        size.offset(47, 0);
        padType_[9] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt09', padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType_[9]);
        size.offset(47, 0);
        padType_[10] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt00' + 10, padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType_[10]);
        size.offset(47, 0);
        padType_[11] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt00' + 11, padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType_[11]);
        size.offset(-141, 47);
        padType_[12] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt00' + 12, padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType_[12]);
        size.offset(47, 0);
        padType_[13] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt00' + 13, padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType_[13]);
        size.offset(47, 0);
        padType_[14] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt00' + 14, padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType_[14]);
        size.offset(47, 0);
        padType_[15] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt00' + 15, padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType_[15]);
    }

    //---Pads--------------------
    {
        auto kickOn = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("kick_button_act.png"));
        auto kickOff = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("kick_button.png"));

        size(0, 0, 40, 40);
        size.offset(65, 56);

        padKick_[0] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd00', kickOff, kickOn, offset);
        padKick_[0]->setPopupMenu(popupPad_);
        padKick_[0]->BeforePopup = &AVinylEditorView::callBeforePopup;
        frame->addView(padKick_[0]);
        size.offset(47, 0);
        padKick_[1] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd01', kickOff, kickOn, offset);
        padKick_[1]->setPopupMenu(popupPad_);
        padKick_[1]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView(padKick_[1]);
        size.offset(47, 0);
        padKick_[2] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd02', kickOff, kickOn, offset);
        padKick_[2]->setPopupMenu(popupPad_);
        padKick_[2]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView (padKick_[2]);
        size.offset(47, 0);
        padKick_[3] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd03', kickOff, kickOn, offset);
        padKick_[3]->setPopupMenu(popupPad_);
        padKick_[3]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView (padKick_[3]);
        size.offset(-141, 47);
        padKick_[4] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd04', kickOff, kickOn, offset);
        padKick_[4]->setPopupMenu(popupPad_);
        padKick_[4]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView (padKick_[4]);
        size.offset(47, 0);
        padKick_[5] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd05', kickOff, kickOn, offset);
        padKick_[5]->setPopupMenu(popupPad_);
        padKick_[5]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView (padKick_[5]);
        size.offset(47, 0);
        padKick_[6] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd06', kickOff, kickOn, offset);
        padKick_[6]->setPopupMenu(popupPad_);
        padKick_[6]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView(padKick_[6]);
        size.offset(47, 0);
        padKick_[7] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd07', kickOff, kickOn, offset);
        padKick_[7]->setPopupMenu(popupPad_);
        padKick_[7]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView(padKick_[7]);
        size.offset(-141, 47);
        padKick_[8] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd08', kickOff, kickOn, offset);
        padKick_[8]->setPopupMenu(popupPad_);
        padKick_[8]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView(padKick_[8]);
        size.offset(47, 0);
        padKick_[9] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd09', kickOff, kickOn, offset);
        padKick_[9]->setPopupMenu(popupPad_);
        padKick_[9]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView(padKick_[9]);
        size.offset(47, 0);
        padKick_[10] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd00' + 10, kickOff, kickOn, offset);
        padKick_[10]->setPopupMenu(popupPad_);
        padKick_[10]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView(padKick_[10]);
        size.offset(47, 0);
        padKick_[11] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd00' + 11, kickOff, kickOn, offset);
        padKick_[11]->setPopupMenu(popupPad_);
        padKick_[11]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView(padKick_[11]);
        size.offset(-141, 47);
        padKick_[12] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd00' + 12, kickOff, kickOn, offset);
        padKick_[12]->setPopupMenu(popupPad_);
        padKick_[12]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView(padKick_[12]);
        size.offset(47, 0);
        padKick_[13] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd00' + 13, kickOff, kickOn, offset);
        padKick_[13]->setPopupMenu(popupPad_);
        padKick_[13]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView(padKick_[13]);
        size.offset(47, 0);
        padKick_[14] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd00' + 14, kickOff, kickOn, offset);
        padKick_[14]->setPopupMenu(popupPad_);
        padKick_[14]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView (padKick_[14]);
        size.offset(47, 0);
        padKick_[15] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd00' + 15, kickOff, kickOn, offset);
        padKick_[15]->setPopupMenu(popupPad_);
        padKick_[15]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView(padKick_[15]);
    }
    //

    ////Wave View Window
    {
        auto glass = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("glass.png"));
        size(0, 0, 343, 83);
        size.offset(270, 54);
        wavView_ = make_shared<VSTGUI::CWaveView>(size, this, 'Wave', glass);
        frame->addView(wavView_);
    }
#ifdef DEVELOPMENT
    {
        size(0, 0, kEditorMaxWidth, 100);
        size.offset(0, kEditorHeight);

        debugFftView_ = make_shared<VSTGUI::CDebugFftView>(size, this, '_FFT');
        frame->addView(debugFftView_);

        size.offset(0, 100);

        debugInputView_ = make_shared<VSTGUI::CDebugFftView>(size, this, '_INP');
        frame->addView(debugInputView_);
    }
#endif

    ///////CheckBoxes/////////////////////////////////////////////////////////////////////
    {
        auto checkOff = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("loop_check.png"));
        {
            auto checkOn = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("loop_check_on.png"));
            size(0, 0, 55, 16);
            size.offset(267, 32);
            loopBox_ = make_shared<VSTGUI::CVinylCheckBox>(size,this, 'Loop', checkOn, checkOff, offset);
            frame->addView(loopBox_);
        }

        {
            auto checkOn = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("sync_check_on.png"));

            size.offset(58, 0);
            syncBox_ = make_shared<VSTGUI::CVinylCheckBox>(size,this, 'Sync', checkOn, checkOff, offset);
            frame->addView(syncBox_);
        }

        {
            auto checkOn = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("revers_check_on.png"));
            size.offset(58, 0);
            rvrsBox_ = make_shared<VSTGUI::CVinylCheckBox>(size,this, 'Rvrs', checkOn, checkOff, offset);
            frame->addView(rvrsBox_);
        }
    }

    ///////Sample Knobs
    {
        auto handle = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("smal_handler.png"));
        auto background = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("smal_slider.png"));

        size(0, 0, 55, 12);
        size.offset(467, 34);
        ampSlide_ = make_shared<VSTGUI::CHorizontalSlider>(size, this, 'Ampl', offset, size.getWidth(), handle, background, offset, VSTGUI::CSliderBase::kLeft);
        frame->addView(ampSlide_);

        size.offset (89, 0);
        tuneSlide_ = make_shared<VSTGUI::CHorizontalSlider>(size, this, 'Tune', offset, size.getWidth(), handle, background, offset, VSTGUI::CSliderBase::kLeft);
        frame->addView(tuneSlide_);
    }


    //////////TimeCodeLearn
    {
        auto checkOff = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("tclearn_check.png"));
        auto checkOn = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("tclearn_check_on.png"));
        size(0, 0, 39, 8);
        size.offset(272, 124);
        tcLearnBox_ = make_shared<VSTGUI::CVinylCheckBox>(size,this, 'TCLr', checkOff, checkOn, offset);
        frame->addView(tcLearnBox_);
    }


    /////////////////////////////////////////////////////////////
    {
        auto comboOn = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("yellow_combo.png"));
        auto comboOff = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("sample_combo.png"));
        size(0, 0, 354, 18);
        size.offset(263, 8);
        samplePopup_ = make_shared<VSTGUI::COptionMenu>(size, this, 'Comb', comboOff, comboOn, VSTGUI::CVinylPopupMenu::kNoTextStyle);
        samplePopup_->addEntry(sampleBase_, "Select sample");
        samplePopup_->addEntry("-", 3, VSTGUI::CMenuItem::kSeparator);
        samplePopup_->addEntry("Add new sample in base", 2, 0);
        samplePopup_->addEntry("Replace this sample", 3, VSTGUI::CMenuItem::kDisabled);
        samplePopup_->addEntry("Delete this sample from base", 4, VSTGUI::CMenuItem::kDisabled);

        frame->addView(samplePopup_);
    }


    {
        auto fxBit = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("distorsion.png"));
        size(0, 0, 10, 11);
        size.offset(271, 55);
        dispDistorsion_ = make_shared<VSTGUI::CVuMeter>(size, fxBit, nullptr, 1, VSTGUI::CVuMeter::Style::kHorizontal);
        frame->addView(dispDistorsion_);
    }
    {
        auto fxBit = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("preroll.png"));
        size.offset(12, 0);
        dispPreRoll_ = make_shared<VSTGUI::CVuMeter>(size, fxBit, nullptr, 1, VSTGUI::CVuMeter::Style::kHorizontal);
        frame->addView(dispPreRoll_);
    }
    {
        auto fxBit = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("postroll.png"));
        size.offset(12, 0);
        dispPostRoll_ = make_shared<VSTGUI::CVuMeter>(size, fxBit, nullptr, 1, VSTGUI::CVuMeter::Style::kHorizontal);
        frame->addView(dispPostRoll_);
    }
    {
        auto fxBit = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("hold.png"));
        size.offset (12, 0);
        dispHold_ = make_shared<VSTGUI::CVuMeter>(size, fxBit, nullptr, 1, VSTGUI::CVuMeter::Style::kHorizontal);
        frame->addView(dispHold_);
    }
    {
        auto fxBit = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("freeze.png"));
        size.offset (12, 0);
        dispFreeze_ = make_shared<VSTGUI::CVuMeter>(size, fxBit, nullptr, 1, VSTGUI::CVuMeter::Style::kHorizontal);
        frame->addView(dispFreeze_);
    }
    {
        auto fxBit = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("vintage.png"));
        size.offset (12, 0);
        dispVintage_ = make_shared<VSTGUI::CVuMeter>(size, fxBit, nullptr, 1, VSTGUI::CVuMeter::Style::kHorizontal);
        frame->addView(dispVintage_);
    }
    {
        auto fxBit = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("locktone.png"));
        size.offset (12, 0);
        dispLockTone_ = make_shared<VSTGUI::CVuMeter>(size, fxBit, nullptr, 1, VSTGUI::CVuMeter::Style::kHorizontal);
        frame->addView(dispLockTone_);
    }

    /////////////Text and labels ////////////////////////////////
    size(0, 0, 50, 10);
    size.offset(422, 230);
    pitchValue_ = make_shared<VSTGUI::CTextLabel>(size, "0.00%", nullptr, VSTGUI::CVinylPopupMenu::kNoFrame);
    pitchValue_->setHoriAlign(VSTGUI::kCenterText);
    pitchValue_->setBackColor(VSTGUI::CColor(0, 0, 0, 0));

    pitchValue_->setFontColor(VSTGUI::CColor(200, 200, 200, 200));
    pitchValue_->setAntialias(true);
    auto litleFont = make_shared<VSTGUI::CFontDesc>(*(pitchValue_->getFont()));
    litleFont->setSize(10);
    litleFont->setStyle(VSTGUI::kNormalFace);
    pitchValue_->setFont(litleFont);
    frame->addView(pitchValue_);

    size(0, 0, 40, 10);
    size.offset(572, 124);
    speedValue_ = make_shared<VSTGUI::CTextLabel>(size, "|| 0.000", nullptr, VSTGUI::CVinylPopupMenu::kNoFrame);
    speedValue_->setHoriAlign(VSTGUI::kLeftText);
    speedValue_->setBackColor(VSTGUI::CColor(0, 0, 0, 0));
    speedValue_->setFontColor(VSTGUI::CColor(10, 255, 10, 100));
    speedValue_->setAntialias(true);
    speedValue_->setFont(litleFont);
    frame->addView(speedValue_);

    size(0, 0, 40, 25);
    size.offset(271, 52);
    sampleNumber_ = make_shared<VSTGUI::CTextLabel>(size, " ", nullptr, VSTGUI::CVinylPopupMenu::kNoFrame);
    sampleNumber_->setHoriAlign(VSTGUI::kLeftText);
    sampleNumber_->setBackColor(VSTGUI::CColor(0, 0, 0, 0));
    sampleNumber_->setFontColor(VSTGUI::CColor(10,255,10, 100));
    sampleNumber_->setAntialias(true);
    sampleNumber_->setFont(litleFont);
    frame->addView(sampleNumber_);

    size(0, 0, 210, 22); //15
    size.offset(367, 7);  //12
    nameEdit_ = make_shared<VSTGUI::CTextEdit>(size, this, 'Name', " ", nullptr, VSTGUI::CVinylPopupMenu::kNoFrame);
    nameEdit_->setHoriAlign(VSTGUI::kLeftText);
    nameEdit_->setBackColor(VSTGUI::CColor(0,0,0,0));
    auto medFont = make_shared<VSTGUI::CFontDesc>(*(nameEdit_->getFont()));
    medFont->setSize(12);
    medFont->setStyle(VSTGUI::kBoldFace);
    nameEdit_->setFont(medFont);
    frame->addView(nameEdit_);

    size(0, 0, 40, 35);
    size.offset(198, 11);
    sceneValue_ = make_shared<VSTGUI::CTextLabel>(size, "1", nullptr, VSTGUI::CVinylPopupMenu::kNoFrame);
    sceneValue_->setHoriAlign(VSTGUI::kCenterText);
    sceneValue_->setBackColor(VSTGUI::CColor(0,0,0,0));
    sceneValue_->setFontColor(VSTGUI::CColor(0,0,0,255));
    sceneValue_->setAntialias(true);
    auto bigFont = make_shared<VSTGUI::CFontDesc>(*(sceneValue_->getFont()));
    bigFont->setSize(28);
    bigFont->setStyle(VSTGUI::kBoldFace);
    sceneValue_->setFont(bigFont);
    frame->addView(sceneValue_);


    update(kGainId, getController()->getParamNormalized(kGainId));
    update(kVolumeId, getController()->getParamNormalized(kVolumeId));
    update(kVolCurveId, getController()->getParamNormalized(kVolCurveId));
    update(kPitchId, getController()->getParamNormalized(kPitchId));
    update(kPitchSwitchId, getController()->getParamNormalized(kPitchSwitchId));
    update(kCurrentSceneId, getController()->getParamNormalized(kCurrentSceneId));
    update(kCurrentEntryId, getController()->getParamNormalized(kCurrentEntryId));

    getFrame()->open(parent, platformType, nullptr);

    return true;
}

//------------------------------------------------------------------------
void PLUGIN_API AVinylEditorView::close ()
{
    sampleBitmaps_.clear();

    gainKnob_ = nullptr;
    scenKnob_ = nullptr;
    ampSlide_ = nullptr;
    tuneSlide_ = nullptr;
    nameEdit_ = nullptr;

    pitchSlider_ = nullptr;
    volumeSlider_ = nullptr;
    for (auto& pad: padKick_) {
        pad = nullptr;
    }
    for (auto& pad: padType_) {
        pad = nullptr;
    }
    loopBox_ = nullptr;
    syncBox_ = nullptr;
    rvrsBox_ = nullptr;
    tcLearnBox_ = nullptr;
    pitchValue_ = nullptr;
    sceneValue_ = nullptr;
    speedValue_ = nullptr;
    sampleNumber_ = nullptr;
    vuLeftMeter_ = nullptr;
    vuRightMeter_ = nullptr;

    dispDistorsion_ = nullptr;
    dispPreRoll_ = nullptr;
    dispPostRoll_ = nullptr;
    dispHold_ = nullptr;
    dispFreeze_ = nullptr;
    dispVintage_ = nullptr;
    dispLockTone_ = nullptr;

    curvSwitch_ = nullptr;
    pitchSwitch_ = nullptr;
    samplePopup_ = nullptr;
    popupPad_ = nullptr;
    sampleBase_ = nullptr;
    padBase_ = nullptr;
    effectBase1_ = nullptr;
    effectBase2_ = nullptr;

    wavView_ = nullptr;
    debugFftView_ = nullptr;
    debugInputView_ = nullptr;

    // call it explicitly with false to prevent extra forget call
    // as we store all child views as shared pointers
    getFrame()->removeAll(false);
    int32_t refCount = getFrame()->getNbReference();
    if (refCount == 1) {
        getFrame()->close();
        frame = nullptr;
    } else {
        getFrame()->forget();
    }
}

//------------------------------------------------------------------------
void AVinylEditorView::valueChanged (VSTGUI::CControl *pControl)
{
    switch (pControl->getTag ())
    {
    case 'Gain':
        controller->setParamNormalized(kGainId, pControl->getValueNormalized());
        controller->performEdit(kGainId, pControl->getValueNormalized());
        break;

    case 'Fadr':
        controller->setParamNormalized(kVolumeId, pControl->getValueNormalized());
        controller->performEdit(kVolumeId, pControl->getValueNormalized());
        break;

    case 'Scen':
        controller->setParamNormalized(kCurrentSceneId, pControl->getValueNormalized());
        controller->performEdit(kCurrentSceneId, pControl->getValueNormalized());
        break;

    case 'Pith':
        controller->setParamNormalized(kPitchId, pControl->getValueNormalized());
        controller->performEdit(kPitchId, pControl->getValueNormalized());
        break;

    case 'PtCr':
        controller->setParamNormalized(kPitchSwitchId, pControl->getValueNormalized());
        controller->performEdit(kPitchSwitchId, pControl->getValueNormalized());
        break;

    case 'Curv':
        controller->setParamNormalized(kVolCurveId, pControl->getValueNormalized());
        controller->performEdit(kVolCurveId, pControl->getValueNormalized());
        break;

    case 'Loop':
        controller->setParamNormalized(kLoopId, pControl->getValueNormalized());
        controller->performEdit(kLoopId, pControl->getValueNormalized());
        break;

    case 'Sync':
        controller->setParamNormalized(kSyncId, pControl->getValueNormalized());
        controller->performEdit(kSyncId, pControl->getValueNormalized());
        break;

    case 'Rvrs':
        controller->setParamNormalized(kReverseId, pControl->getValueNormalized());
        controller->performEdit(kReverseId, pControl->getValueNormalized());
        break;

    case 'Ampl':
        controller->setParamNormalized(kAmpId, pControl->getValueNormalized());
        controller->performEdit(kAmpId, pControl->getValueNormalized());
        break;

    case 'Tune':
        controller->setParamNormalized(kTuneId, pControl->getValueNormalized());
        controller->performEdit(kTuneId, pControl->getValueNormalized());
        break;

    case 'TCLr':
        controller->setParamNormalized(kTimecodeLearnId, pControl->getValueNormalized());
        controller->performEdit(kTimecodeLearnId, pControl->getValueNormalized());
        break;
        //------------------

    case 'Pd00':
        touchPadMessage(0, pControl->getValueNormalized());
    break;

    case 'Pd01':
        touchPadMessage(1, pControl->getValueNormalized());
    break;

    case 'Pd02':
        touchPadMessage(2, pControl->getValueNormalized());
    break;

    case 'Pd03':
        touchPadMessage(3, pControl->getValueNormalized());
    break;

    case 'Pd04':
        touchPadMessage(4, pControl->getValueNormalized());
    break;

    case 'Pd05':
        touchPadMessage(5, pControl->getValueNormalized());
    break;

    case 'Pd06':
        touchPadMessage(6, pControl->getValueNormalized());
    break;

    case 'Pd07':
        touchPadMessage(7, pControl->getValueNormalized());
    break;

    case 'Pd08':
        touchPadMessage(8, pControl->getValueNormalized());
    break;

    case 'Pd09':
        touchPadMessage(9, pControl->getValueNormalized());
    break;

    case 'Pd00' + 10:
        touchPadMessage(10, pControl->getValueNormalized());
    break;

    case 'Pd00' + 11:
        touchPadMessage(11, pControl->getValueNormalized());
    break;

    case 'Pd00' + 12:
        touchPadMessage(12, pControl->getValueNormalized());
    break;

    case 'Pd00' + 13:
        touchPadMessage(13, pControl->getValueNormalized());
    break;

    case 'Pd00' + 14:
        touchPadMessage(14, pControl->getValueNormalized());
    break;

    case 'Pd00' + 15:
        touchPadMessage(15, pControl->getValueNormalized());
    break;

    case 'Name':
    {
        if (sampleBase_ && sampleBase_->getEntry(int32_t(currentEntry_))) {
            sampleBase_->getEntry(int32_t(currentEntry_))->setTitle(nameEdit_->getText());
        }
        if (padBase_ && padBase_->getEntry(int32_t(currentEntry_))) {
            padBase_->getEntry(int32_t(currentEntry_))->setTitle(nameEdit_->getText());
        }
        IMessage* msg = controller->allocateMessage();
        if (msg) {
            String sampleName(nameEdit_->getText());
            msg->setMessageID("renameEntry");
            msg->getAttributes()->setInt("SampleNumber", currentEntry_);
            msg->getAttributes()->setString("SampleName", sampleName);
            controller->sendMessage(msg);
            msg->release();
        }

    }
    break;
    //------------------
    case 'PpUp':
    {
        int menuIndex;
        VSTGUI::COptionMenu * SelectedMenu = popupPad_->getLastItemMenu(menuIndex);
        if (SelectedMenu == padBase_) {
            setPadMessage(padForSetting_ + 1, PadEntry::SamplePad, menuIndex);
        }
        if (SelectedMenu == effectBase1_) {
            setPadMessage(padForSetting_ + 1, PadEntry::SwitchPad, switchedFx[menuIndex]);
        }
        if (SelectedMenu == effectBase2_) {
            setPadMessage(padForSetting_ + 1, PadEntry::KickPad, punchFx[menuIndex]);
        }
        if (SelectedMenu == popupPad_) {
            if (menuIndex==0){
                setPadMessage(padForSetting_ + 1, PadEntry::AssigMIDI);
            }
            if (menuIndex==6){
                setPadMessage(padForSetting_ + 1, PadEntry::EmptyPad, -1);
            }
        }
    }
    break;
    //------------------
    case 'Comb':
    {
        int menuIndex;
        VSTGUI::COptionMenu * SelectedMenu = samplePopup_->getLastItemMenu(menuIndex);
        if (SelectedMenu == sampleBase_) {
            double normalValue = menuIndex / double(EMaximumSamples - 1.);
            controller->setParamNormalized(kCurrentEntryId, normalValue);
            controller->performEdit(kCurrentEntryId, normalValue);
        }
        if (SelectedMenu == samplePopup_) {
            if (menuIndex==2) {

                ////////////Open FileSelector Multy/////////////////////////
                auto selector = VSTGUI::SharedPointer(VSTGUI::CNewFileSelector::create(frame, VSTGUI::CNewFileSelector::kSelectFile), false);
                if (selector) {
                    selector->kSelectEndMessage = "LoadingFile";
                    selector->setDefaultExtension (VSTGUI::CFileExtension ("WAVE", "wav"));
                    selector->setTitle("Choose Samples for load in base");
                    selector->setAllowMultiFileSelection(true);
                    selector->run(this);
                }
            }
            if (menuIndex==3){
                auto selector = VSTGUI::SharedPointer(VSTGUI::CNewFileSelector::create(frame, VSTGUI::CNewFileSelector::kSelectFile), false);
                if (selector) {
                    selector->kSelectEndMessage = "ReplaceFile";
                    selector->setDefaultExtension(VSTGUI::CFileExtension ("WAVE", "wav"));
                    selector->setTitle("Choose Sample for replace");
                    selector->setAllowMultiFileSelection(false);
                    selector->run(this);
                }
            }
            if (menuIndex==4){
                using namespace VSTGUI::Standalone;
                auto wndw = Detail::createAlertBox({"Delete sample from base",
                                                    "Are you sure?",
                                                    "No",
                                                    "Yes"},
                                                   [&](AlertResult res) {

                                                       if(res == AlertResult::SecondButton) {
                                                           IMessage* msg = controller->allocateMessage();
                                                           if (msg) {
                                                               msg->setMessageID("deleteEntry");
                                                               msg->getAttributes()->setInt("SampleNumber", currentEntry_);
                                                               controller->sendMessage(msg);
                                                               msg->release();
                                                           }}
                                                   });

                VSTGUI::CCoord x;
                VSTGUI::CCoord y;
                getFrame()->getPosition(x, y);

                auto parentRect = getFrame()->getViewSize();
                parentRect.offset({ x, y });
                auto wndSize = wndw->getSize();
                wndw->setPosition({(parentRect.left + parentRect.right) / 2 - (wndSize.x / 2),
                                   (parentRect.top + parentRect.bottom) / 2 - (wndSize.y / 2)});
                wndw->show();
            }
        }
    }
    break;

    }
}

//------------------------------------------------------------------------
void AVinylEditorView::controlBeginEdit (VSTGUI::CControl* pControl)
{
    switch (pControl->getTag ()) {
    case 'Gain':
        controller->beginEdit(kGainId);
        break;
    case 'Pith':
        controller->beginEdit(kPitchId);
        break;
    case 'Fadr':
        controller->beginEdit(kVolumeId);
        break;
    case 'Scen':
        controller->beginEdit(kCurrentSceneId);
        break;
    case 'Comb':
        controller->beginEdit(kCurrentEntryId);
        break;
    case 'Loop':
        controller->beginEdit(kLoopId);
        break;
    case 'Sync':
        controller->beginEdit(kSyncId);
        break;
    case 'Rvrs':
        controller->beginEdit(kReverseId);
        break;
    case 'Ampl':
        controller->beginEdit(kAmpId);
        break;
    case 'Tune':
        controller->beginEdit(kTuneId);
        break;
    case 'PtCr':
        controller->beginEdit(kPitchSwitchId);
        break;
    case 'Curv':
        controller->beginEdit(kVolCurveId);
        break;
    case 'TCLr':
        controller->beginEdit(kTimecodeLearnId);
        break;
    default:
        break;
    }
}

//------------------------------------------------------------------------
void AVinylEditorView::controlEndEdit (VSTGUI::CControl* pControl)
{
    switch (pControl->getTag ())
    {
    case 'Gain':
        controller->endEdit(kGainId);
        break;
    case 'Pith':
        controller->endEdit(kPitchId);
        break;
    case 'Fadr':
        controller->endEdit(kVolumeId);
        break;
    case 'Scen':
        controller->endEdit(kCurrentSceneId);
        break;
    case 'Comb':
        controller->endEdit(kCurrentEntryId);
        break;
    case 'Loop':
        controller->endEdit(kLoopId);
        break;
    case 'Sync':
        controller->endEdit(kSyncId);
        break;
    case 'Rvrs':
        controller->endEdit(kReverseId);
        break;
    case 'Ampl':
        controller->endEdit(kAmpId);
        break;
    case 'Tune':
        controller->endEdit(kTuneId);
        break;
    case 'PtCr':
        controller->endEdit(kPitchSwitchId);
        break;
    case 'Curv':
        controller->endEdit(kVolCurveId);
        break;
    case 'TCLr':
        controller->endEdit(kTimecodeLearnId);
        break;
    default:
        break;
    }
}

//------------------------------------------------------------------------
void AVinylEditorView::update(ParamID tag, ParamValue value)
{
    switch (tag)
    {
    case kGainId:
        if (gainKnob_) {
            gainKnob_->setValueNormalized((float)value);
        }
        break;

    case kAmpId:
        if (ampSlide_) {
            ampSlide_->setValueNormalized((float)value);
        }
        break;

    case kTuneId:
        if (tuneSlide_) {
            tuneSlide_->setValueNormalized((float)value);
        }
        break;

    case kVolumeId:
        if (volumeSlider_) {
            volumeSlider_->setValueNormalized((float)value);
        }
        break;

    case kSyncId:
        if (syncBox_) {
            syncBox_->setValueNormalized((float)value);
        }
        break;

    case kReverseId:
        if (rvrsBox_) {
            rvrsBox_->setValueNormalized((float)value);
        }
        break;

    case kTimecodeLearnId:
        if (tcLearnBox_) {
            tcLearnBox_->setValueNormalized((float)value);
        }
        break;

    case kVolCurveId:
        if (curvSwitch_) {
            curvSwitch_->setValueNormalized((float)value);
        }
        break;

    case kLoopId:
        if (loopBox_) {
            loopBox_->setValueNormalized((float)value);
            if(wavView_) {
                wavView_->setLoop(loopBox_->getChecked());
            }
        }
        break;

    case kPitchId:
        if (pitchSlider_) {
            pitchSlider_->setValueNormalized((float)value);
        }
        if(pitchValue_ && pitchSwitch_ && pitchSlider_) {
            String textval;
            if (pitchSwitch_->getPosition() == 0) {
                textval = "0.00%";
            } else if (pitchSwitch_->getPosition() == 2) {
                textval.printf(STR("%0.2f%"), pitchSlider_->getValue() * 100. - 50.);
            } else {
                textval.printf(STR("%0.2f%"), pitchSlider_->getValue() * 20. - 10.);
            }
            pitchValue_->setText(VSTGUI::UTF8String(textval));
        }
        break;

    case kPitchSwitchId:
        if (pitchSwitch_) {
            pitchSwitch_->setValueNormalized((float)value);
        }
        if(pitchValue_ && pitchSwitch_ && pitchSlider_) {
            String textval;
            if (pitchSwitch_->getPosition() == 0) {
                textval = "0.00%";
            } else if (pitchSwitch_->getPosition() == 2) {
                textval.printf(STR("%0.2f%"), pitchSlider_->getValue() * 100. - 50.);
            } else {
                textval.printf(STR("%0.2f%"), pitchSlider_->getValue() * 20. - 10.);
            }
            pitchValue_->setText(VSTGUI::UTF8String(textval));
        }
        break;

    case kCurrentSceneId:
        if (scenKnob_)
        {
            scenKnob_->setValueNormalized((float)value);
        }
        if(sceneValue_){
            int currentScene = floor(value * float(EMaximumScenes - 1) + 0.5);
            String textval;
            textval = textval.printf("%d",currentScene + 1);
            sceneValue_->setText(VSTGUI::UTF8String(textval));
        }
        break;

    case kCurrentEntryId:
    {
        int64_t newCurEntry = floor(value * float(EMaximumSamples - 1) + 0.5);
        if (newCurEntry != currentEntry_) {
            currentEntry_ = floor(value * float(EMaximumSamples - 1) + 0.5);

            if ((currentEntry_ < int64_t(sampleBitmaps_.size())) && (currentEntry_ >= 0)) {

                if (wavView_) {
                    wavView_->setWave(sampleBitmaps_.at(currentEntry_));
                    wavView_->setPosition(0);
                }
                if (sampleBase_) {
                    sampleBase_->setCurrent(int32_t(currentEntry_), true);
                }
                String textval;
                if (sampleNumber_) {
                    sampleNumber_->setText(VSTGUI::UTF8String(textval.printf("#%03d", currentEntry_ + 1)));
                }
                if (nameEdit_ && sampleBase_) {
                    nameEdit_->setText(sampleBase_->getEntry(int32_t(currentEntry_))->getTitle());
                }
            }
        }
    }
    break;

    case kVuLeftId:
        lastVuLeftMeterValue_ = float(value);
        break;

    case kVuRightId:
        lastVuRightMeterValue_ = float(value);
        break;

    case kDistorsionId:
        lastDistort_ = float(value) > 0.5f;
        break;

    case kPreRollId:
        lastPreRoll_ = float(value) > 0.5f;
        break;

    case kPostRollId:
        lastPostRoll_ = float(value) > 0.5f;
        break;

    case kHoldId:
        lastHold_ = float(value) > 0.5;
        break;

    case kFreezeId:
        lastFreeze_ = float(value) > 0.5;
        break;

    case kVintageId:
        lastVintage_ = float(value) > 0.5;
        break;

    case kLockToneId:
        lastLockTone_ = float(value) > 0.5;
        break;

    case kPositionId:
        if (wavView_) {
            wavView_->setPosition(float(value));
        }
        break;
    default:
        break;
    }
}

VSTGUI::CMessageResult AVinylEditorView::notify(CBaseObject* sender, const char* message)
{

    if (message == VSTGUI::CVSTGUITimer::kMsgTimer) {
        if (vuLeftMeter_) {
            vuLeftMeter_->setValue((vuLeftMeter_->getValue() * 1. + sqrt(lastVuLeftMeterValue_)) / 2.);
            lastVuLeftMeterValue_ /= 2.;
        }
        if (vuRightMeter_) {
            vuRightMeter_->setValue((vuRightMeter_->getValue() * 1. + sqrt(lastVuRightMeterValue_)) / 2.);
            lastVuRightMeterValue_ /= 2.;
        }
        if (dispDistorsion_) {
            dispDistorsion_->setValue(lastDistort_ ? 1 : 0);
        }
        if (dispPreRoll_) {
            dispPreRoll_->setValue(lastPreRoll_ ? 1 : 0);
        }
        if (dispPostRoll_) {
            dispPostRoll_->setValue(lastPostRoll_ ? 1 : 0);
        }
        if (dispHold_) {
            dispHold_->setValue(lastHold_ ? 1 : 0);
        }
        if (dispFreeze_) {
            dispFreeze_->setValue (lastFreeze_ ? 1 : 0);
        }
        if (dispVintage_) {
            dispVintage_->setValue (lastVintage_ ? 1 : 0);
        }
        if (dispLockTone_) {
            dispLockTone_->setValue (lastLockTone_ ? 1 : 0);
        }
    }
    if (message == VSTGUI::CNewFileSelector::kSelectEndMessage) {
        VSTGUI::CNewFileSelector* sel = dynamic_cast<VSTGUI::CNewFileSelector*>(sender);
        if (sel) {
            if (strcmp(sel->kSelectEndMessage,"LoadingFile")==0) {
                size_t MaxCount = (sel->getNumSelectedFiles()<(EMaximumSamples - sampleBitmaps_.size())) ?
                                      sel->getNumSelectedFiles() :
                                      EMaximumSamples-sampleBitmaps_.size();
                for(size_t i = 0; i < MaxCount; i++)
                {
                    IMessage* msg = controller->allocateMessage ();
                    if (msg) {
                        using namespace std::filesystem;
                        msg->setMessageID ("loadNewEntry");
                        msg->getAttributes ()->setString("File",
                                                        String(sel->getSelectedFile(int32_t(i))));
                        msg->getAttributes ()->setString("Sample",
                                                        path(sel->getSelectedFile(int32_t(i))).filename().u16string().data());
                        controller->sendMessage (msg);
                        msg->release ();
                    }
                }

            }

            if (strcmp(sel->kSelectEndMessage,"ReplaceFile")==0) {
                IMessage* msg = controller->allocateMessage ();
                if (msg) {
                    using namespace std::filesystem;
                    msg->setMessageID ("replaceEntry");
                    msg->getAttributes ()->setString("File", String(sel->getSelectedFile(0)));
                    msg->getAttributes ()->setString("Sample", path(sel->getSelectedFile(0)).filename().u16string().data());
                    controller->sendMessage (msg);
                    msg->release ();
                }
            }
            return VSTGUI::kMessageNotified;
        }
    }

    return VSTGUIEditor::notify (sender, message);
}

void AVinylEditorView::setSpeedMonitor(double _speed)
{
    if (_speed != lastSpeedValue_) {
        lastSpeedValue_ = _speed;
        if (speedValue_) {
            String tmp;
            if (lastSpeedValue_ > 0) {
                tmp = tmp.printf("> %0.3f",fabs(lastSpeedValue_));
            } else if (lastSpeedValue_ < 0) {
                tmp = tmp.printf("< %0.3f",fabs(lastSpeedValue_));
            } else {
                tmp = tmp.printf("|| %0.3f",fabs(lastSpeedValue_));
            }
            speedValue_->setText(VSTGUI::UTF8String(tmp));
        }

    }
}

void AVinylEditorView::setPositionMonitor(double _position)
{
    if (_position != lastPositionValue_) {
        lastPositionValue_ = _position;
        if (wavView_) {
            wavView_->setPosition((float)_position);
        }

    }
}

AVinylEditorView::SharedPointer<VSTGUI::CBitmap> AVinylEditorView::generateWaveform(SampleEntry<Sample64> &sample, bool normolize)
{
    size_t bitmapWidth = sample.bufferLength();
    if (bitmapWidth < 400) {
        bitmapWidth = 400;
    }
    else if (bitmapWidth > 800) {
        bitmapWidth = 800;
    }
    else if (bitmapWidth % 2) {
        bitmapWidth++;
    }
    size_t halfWidth  = bitmapWidth / 2;


    SampleEntry<Sample64>::Type peak = sample.peakSample(0, sample.bufferLength());
    SampleEntry<Sample64>::Type norm = (normolize && peak > 1.) ? 1. / peak : 1.;

    bool drawBeats =  sample.acidBeats() > 0;

    double beatPeriodic = drawBeats ? double(bitmapWidth) / (2. * sample.acidBeats()) : 0;
    auto digits = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("digits.png"));
    auto waveForm = make_shared<VSTGUI::CBitmap>(bitmapWidth, 83);
    if (!waveForm || !digits || sample.bufferLength() == 0) {
        return nullptr;
    }

    auto pixelMap = VSTGUI::SharedPointer(VSTGUI::CBitmapPixelAccess::create(waveForm), false);
    auto pixelDigits = VSTGUI::SharedPointer(VSTGUI::CBitmapPixelAccess::create(digits), false);
    pixelMap->setPosition(0, 61);
    pixelMap->setColor(VSTGUI::CColor(0, 255, 0, 220));
    double stretch = double(sample.bufferLength()) / halfWidth;
    for (uint32_t i = 1; i < halfWidth; i++) {
        pixelMap->setPosition((i - 1) * 2, 61);
        pixelMap->setColor(VSTGUI::CColor(0, 255, 0, 120));
        SampleEntry<Sample64>::Type height = norm * sample.peakSample((i - 1) * stretch, i * stretch);
        SampleEntry<Sample64>::Type heightStretch = 60. * height;
        for (uint32_t j = 1; j < 40 * height; j++) {
            pixelMap->setPosition((i - 1) * 2, 61 - j * 2);
            pixelMap->setColor(VSTGUI::CColor(100. * (1. - j / heightStretch), 255, 0, 40 + 180 * sqr(1. - j / heightStretch)));
            if ((61 + j * 2) <= 81) {
                pixelMap->setPosition((i - 1) * 2, 61 + j * 2);
                pixelMap->setColor(VSTGUI::CColor(100. * (1. - j / heightStretch), 255, 0, 80. * sqr(1. - j / heightStretch)));
            }
        }
    }

    for (uint32_t i = 1; i < (drawBeats ? halfWidth : 0U); i++) {
        if (((i == 1) || ((uint32_t((i - 2) / beatPeriodic)) != (uint32_t((i - 1) / beatPeriodic))))) {
            for (uint32_t j = 0; j <= 5; j++) {
                for(uint32_t k = 0; k <= 5; k++) {
                    pixelMap->setPosition((i - 1) * 2 + j + 2, 76 + k);
                    pixelDigits->setPosition((((i - 1) / uint32_t(beatPeriodic)) % 10) * 5 + j, k);
                    VSTGUI::CColor digPixel;
                    pixelDigits->getColor(digPixel);
                    pixelMap->setColor(digPixel);
                }
            }
            for (uint32_t j = 61; j < 85; j += 2) {
                pixelMap->setPosition((i - 1) * 2, j);
                pixelMap->setColor(VSTGUI::CColor(255, 255, 255, (j - 58) * 6 + 10));
            }
        }
    }
    return waveForm;
}

void AVinylEditorView::delEntry(size_t delEntryIndex)
{
    if ((delEntryIndex >= 0) && (delEntryIndex < sampleBitmaps_.size())) {

        sampleBitmaps_.erase(sampleBitmaps_.begin() + delEntryIndex);

        if (sampleBase_) {
            sampleBase_->removeEntry(int32_t(delEntryIndex));
        }
        if (padBase_) {
            padBase_->removeEntry(int32_t(delEntryIndex));
        }
        if (currentEntry_ >= sampleBase_->getNbEntries()) {
            currentEntry_ = sampleBase_->getNbEntries() - 1;
        }
        if (sampleBase_->getNbEntries() == 0) {
            sampleBase_->addEntry(EEmptyBaseTitle, 0, VSTGUI::CMenuItem::kTitle|VSTGUI::CMenuItem::kDisabled);
            if (padBase_) {
                padBase_->addEntry(EEmptyBaseTitle, 0, VSTGUI::CMenuItem::kTitle|VSTGUI::CMenuItem::kDisabled);
            }
            //currentEntry_ = 0;
            if (wavView_) {
                wavView_->setWave(nullptr);
            }
            if (nameEdit_) {
                nameEdit_->setText(0);
            }
            if (sampleNumber_) {
                sampleNumber_->setText(" ");
            }

            if (samplePopup_) {
                samplePopup_->getEntry(3)->setEnabled(false);
                samplePopup_->getEntry(4)->setEnabled(false);
                if (EMaximumSamples > samplePopup_->getNbEntries()) {
                    samplePopup_->getEntry(2)->setEnabled(true);
                }
            }
        } else {
            if (wavView_ && currentEntry_ < int64_t(sampleBitmaps_.size())) {
                wavView_->setWave(sampleBitmaps_.at(currentEntry_));
                wavView_->setPosition(0);
            }
            controller->setParamNormalized(kCurrentEntryId, currentEntry_ / double(EMaximumSamples - 1));
            controller->performEdit(kCurrentEntryId, currentEntry_ / double(EMaximumSamples - 1));
        }
        
    }
}

void AVinylEditorView::initEntry(SampleEntry<Sample64> &newEntry)
{

    auto waveForm = generateWaveform(newEntry);
    if (!waveForm) {
        // not readable format
        return;
    }

    size_t index = newEntry.index() - 1;
    if (index < sampleBitmaps_.size()) {
        sampleBitmaps_[index] = std::move(waveForm);

        if (sampleBase_) {
            sampleBase_->getEntry(int32_t(index))->setTitle(VSTGUI::UTF8String(newEntry.name()));
        }
        if (padBase_) {
            padBase_->getEntry(int32_t(index))->setTitle(VSTGUI::UTF8String(newEntry.name()));
        }
    } else {

        index = sampleBitmaps_.size();
        sampleBitmaps_.push_back(std::move(waveForm));

        if (sampleBase_) {
            if (strcmp(sampleBase_->getEntry(0)->getTitle(), EEmptyBaseTitle)==0) {
                sampleBase_->removeAllEntry();
                if (padBase_) padBase_->removeAllEntry();
                if (samplePopup_) {
                    samplePopup_->getEntry(3)->setEnabled(true);
                    samplePopup_->getEntry(4)->setEnabled(true);
                    if (EMaximumSamples <= samplePopup_->getNbEntries()) {
                        samplePopup_->getEntry(2)->setEnabled(false);
                    }
                }
            }
            sampleBase_->addEntry(VSTGUI::UTF8String(newEntry.name()), sampleBase_->getNbEntries(), VSTGUI::CMenuItem::kChecked);
            if (padBase_) {
                padBase_->addEntry(VSTGUI::UTF8String(newEntry.name()), sampleBase_->getNbEntries(), 0);
            }
        }
    }

    if (index == currentEntry_) {
        if (samplePopup_) {
            sampleBase_->setCurrent(currentEntry_, true);
        }
        if (wavView_) {
            wavView_->setWave(sampleBitmaps_[index]);
            wavView_->setLoop(newEntry.Loop);
        }
        if (nameEdit_) {
            nameEdit_->setText(VSTGUI::UTF8String(newEntry.name()));
        }
        if (sampleNumber_) {
            String tmp;
            sampleNumber_->setText(VSTGUI::UTF8String(tmp.printf("#%03d", newEntry.index())));
        }
    }
}

void AVinylEditorView::debugFft(SampleEntry<Sample64> &newEntry)
{
    auto waveForm = generateWaveform(newEntry, true);
    if (!waveForm) {
        // not readable format
        return;
    }

    if (debugFftView_) {
        debugFftView_->setWave(waveForm);
    }
}

void AVinylEditorView::debugInput(SampleEntry<Sample64> &newEntry)
{
    auto waveForm = generateWaveform(newEntry, true);
    if (!waveForm) {
        // not readable format
        return;
    }

    if (debugInputView_) {
        debugInputView_->setWave(waveForm);
    }
}

void AVinylEditorView::setPadState(int pad, bool state)
{
    padKick_[pad]->setActive(state);
}
void  AVinylEditorView::setPadType(int pad,int type)
{
    padType_[pad]->setPosition(type);
}

void  AVinylEditorView::setPadTag(int pad,int tag)
{
    padTag_[pad] = tag;
}

bool AVinylEditorView::callBeforePopup(VSTGUI::IControlListener *listener, VSTGUI::CControl* pControl)
{
    AVinylEditorView *view = dynamic_cast<AVinylEditorView*>(listener);
    if (!view) {
        return false;
    }
    view->padForSetting_ = pControl->getTag() - 'Pd00';

    switch (view->padType_[view->padForSetting_]->getPosition()) {
    case PadEntry::SamplePad:
        if (view->padBase_) {
            view->padBase_->setCurrent(view->padTag_[view->padForSetting_], false);
        }
        if (view->effectBase1_) {
            view->effectBase1_->setCurrent(-1, false);
        }
        if (view->effectBase2_) {
            view->effectBase2_->setCurrent(-1, false);
        }
        break;
    case PadEntry::SwitchPad:
        if (view->padBase_) {
            view->padBase_->setCurrent(-1, false);
        }
        if (view->effectBase1_) {
            view->effectBase1_->setCurrent(bitNumber(view->padTag_[view->padForSetting_]), false);
        }
        if (view->effectBase2_) {
            view->effectBase2_->setCurrent(-1, false);
        }
        break;
    case PadEntry::KickPad:
        if (view->padBase_) {
            view->padBase_->setCurrent(-1, false);
        }
        if (view->effectBase1_) {
            view->effectBase1_->setCurrent(-1, false);
        }
        if (view->effectBase2_) {
            view->effectBase2_->setCurrent(bitNumber(view->padTag_[view->padForSetting_]), false);
        }
        break;
    default:
        if (view->padBase_) {
            view->padBase_->setCurrent(-1, false);
        }
        if (view->effectBase1_) {
            view->effectBase1_->setCurrent(-1, false);
        }
        if (view->effectBase2_) {
            view->effectBase2_->setCurrent(-1, false);
        }
        break;
    }
    return true;
}

void AVinylEditorView::setPadMessage(int pad, int type, int tag)
{
    IMessage* msg = controller->allocateMessage ();
    if (msg)
    {
        msg->setMessageID("setPad");
        msg->getAttributes()->setInt("PadNumber", pad);
        msg->getAttributes()->setInt("PadType", type);
        msg->getAttributes()->setInt("PadTag", tag);
        controller->sendMessage(msg);
        msg->release();
    }
}

void AVinylEditorView::setPadMessage(int pad, int type)
{
    IMessage* msg = controller->allocateMessage ();
    if (msg)
    {
        msg->setMessageID("setPad");
        msg->getAttributes()->setInt("PadNumber", pad);
        msg->getAttributes()->setInt("PadType", type);
        controller->sendMessage(msg);
        msg->release();
    }
}

void AVinylEditorView::touchPadMessage(int pad, double value)
{
    IMessage *msg = controller->allocateMessage();
    if (msg) {
        msg->setMessageID("touchPad");
        msg->getAttributes()->setInt("PadNumber", pad);
        msg->getAttributes()->setFloat("PadValue", value);
        controller->sendMessage(msg);
        msg->release();
    }
}

}} // namespaces
