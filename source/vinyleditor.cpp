#include "pluginterfaces/base/geoconstants.h"
#include "vinyleditor.h"
#include "vinylcontroller.h"
#include "vinylparamids.h"
#include "helpers/padentry.h"

#include "base/source/fstring.h"

#include <cmath>
#include <filesystem>

namespace {

inline double sqr(double x) {
    return x * x;
}

template <typename T, typename ... Args>
auto make_shared(Args&&... arg) {
    return VSTGUI::SharedPointer<T>(new T(std::forward<Args>(arg)...),
                                    true /* vstsdk takes ownership instead of sharing,
                                      probably to support old way of work with raw pointers,
                                      so we need here one extra refcounter inc*/);
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
    , lastVuLeftMeterValue(0.f)
    , lastVuRightMeterValue(0.f)
    , currentEntry(0)
    , lastSpeedValue(0)
    , lastPositionValue(0)
{
    setIdleRate(50); // 1000ms/50ms = 20Hz
    setRect ({0, 0, kEditorMaxWidth, kEditorHeight});
}

//------------------------------------------------------------------------
tresult PLUGIN_API AVinylEditorView::onSize (ViewRect* newSize)
{
    return VSTGUIEditor::onSize (newSize);
}

//------------------------------------------------------------------------
tresult PLUGIN_API AVinylEditorView::checkSizeConstraint (ViewRect* rect)
{
    if ((rect->right - rect->left) <= ((kEditorMinWidth + kEditorMaxWidth) / 2)) {
        rect->right = rect->left + kEditorMinWidth;
        //result = result && kResultFalse;
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
    if (volumeSlider->hitTest (where, 0)) {
        resultTag = kVolumeId;
        return kResultOk;
    }
    if (pitchSlider->hitTest (where, 0)) {
        resultTag = kPitchId;
        return kResultOk;
    }
    if (gainKnob->hitTest (where, 0)) {
        resultTag = kGainId;
        return kResultOk;
    }
    if (scenKnob->hitTest (where, 0)) {
        resultTag = kCurrentSceneId;
        return kResultOk;
    }
    if (loopBox->hitTest (where, 0)) {
        resultTag = kLoopId;
        return kResultOk;
    }
    if (syncBox->hitTest (where, 0)) {
        resultTag = kSyncId;
        return kResultOk;
    }
    if (rvrsBox->hitTest (where, 0)) {
        resultTag = kReverseId;
        return kResultOk;
    }
    if (curvSwitch->hitTest (where, 0)) {
        resultTag = kVolCurveId;
        return kResultOk;
    }
    if (pitchSwitch->hitTest (where, 0)) {
        resultTag = kPitchSwitchId;
        return kResultOk;
    }
    if (ampSlide->hitTest (where, 0)) {
        resultTag = kAmpId;
        return kResultOk;
    }
    if (tuneSlide->hitTest (where, 0)) {
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

    sampleBase = make_shared<VSTGUI::COptionMenu>(size, this, 'Base', nullptr, nullptr, VSTGUI::CVinylPopupMenu::kCheckStyle);
    sampleBase->addEntry(EEmptyBaseTitle, 0, VSTGUI::CMenuItem::kTitle | VSTGUI::CMenuItem::kDisabled);

    padBase = make_shared<VSTGUI::COptionMenu>(size, this, 'BsPd', nullptr, nullptr, VSTGUI::CVinylPopupMenu::kCheckStyle);
    padBase->addEntry(EEmptyBaseTitle, 0, VSTGUI::CMenuItem::kTitle | VSTGUI::CMenuItem::kDisabled);

    effectBase1 = make_shared<VSTGUI::COptionMenu>(size, this, 'BsE1', nullptr, nullptr, VSTGUI::CVinylPopupMenu::kCheckStyle);
    for (size_t i = 0; i < sizeof(switchedFx) / sizeof(int); i++) {
        effectBase1->addEntry(effectNames[bitNumber(switchedFx[i])], int32_t(i), 0);
    }

    effectBase2 = make_shared<VSTGUI::COptionMenu>(size, this, 'BsE2', nullptr, nullptr, VSTGUI::CVinylPopupMenu::kCheckStyle);
    for (size_t i = 0; i < sizeof(punchFx)/sizeof(int); i++) {
        effectBase2->addEntry(effectNames[bitNumber(punchFx[i])], int32_t(i), 0);
    }

    {
        auto menuOn = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("fx_yellow.png"));
        popupPad = make_shared<VSTGUI::CVinylPopupMenu>(this, 'PpUp', menuOn, VSTGUI::CRect(-4, -4, 4, 4));
    }
    popupPad->addEntry("Assign MIDI key",0,0);
    popupPad->addEntry("-", 1, VSTGUI::CMenuItem::kSeparator);
    popupPad->addEntry(padBase, "Select sample");
    popupPad->addEntry(effectBase1, "Switch Fxs");
    popupPad->addEntry(effectBase2, "Kick Fxs");
    popupPad->addEntry("-", 5, VSTGUI::CMenuItem::kSeparator);
    popupPad->addEntry("Clear", 6, 0);


    //---Knobs-------
    VSTGUI::CPoint offset (0,0);
    {
        auto handle = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("knob_handle.png"));
        auto background = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("gain_back.png"));

        size(0, 0, 24, 24);
        size.offset(17, 17);
        gainKnob = make_shared<VSTGUI::CKnob>(size, this, 'Gain', background, handle, offset);
        frame->addView(gainKnob);

        size.offset (63, 0);
        scenKnob = make_shared<VSTGUI::CKnob>(size, this, 'Scen', background, handle, offset);
        scenKnob->setMin(1);
        scenKnob->setMax(EMaximumScenes);
        frame->addView(scenKnob);
    }

    //---Pitch slider-------
    {
        auto handle = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("pitch_handler.png"));
        auto background = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("pitch_slider.png"));

        size(0, 0, 350, 39);
        size.offset(267, 182);
        pitchSlider = make_shared<VSTGUI::CHorizontalSlider>(size, this, 'Pith', offset, size.getWidth(), handle, background, offset, VSTGUI::CSliderBase::kLeft);
        frame->addView(pitchSlider);
    }


    //---Mix slider-------
    {
        auto handle = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("volume_handler.png"));
        auto background = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("volume_slider.png"));
        size(0, 0, 38, 145);
        size.offset(9, 75);
        volumeSlider = make_shared<VSTGUI::CVerticalSlider>(size, this, 'Fadr', offset, size.getHeight(), handle, background, offset);
        frame->addView(volumeSlider);
    }


    //---VuMeter--------------------
    {
        auto onBitmap = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("vu_on.png"));
        auto offBitmap = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("vu_off.png"));

        size(0, 0, 352, 11);
        size.offset(265, 142);
        vuLeftMeter = make_shared<VSTGUI::CVuMeter>(size, onBitmap, offBitmap, 37, VSTGUI::CVuMeter::Style::kHorizontal);
        frame->addView(vuLeftMeter);
        size.offset(0, 12);
        vuRightMeter = make_shared<VSTGUI::CVuMeter>(size, onBitmap, offBitmap, 37, VSTGUI::CVuMeter::Style::kHorizontal);
        frame->addView(vuRightMeter);
    }

    //---3posSwitch--------------------
    {
        auto oPos1 = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("curve_1.png"));
        auto oPos2 = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("curve_2.png"));
        auto oPos3 = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("curve_3.png"));

        size(0, 0, 18, 10);
        size.offset(18, 225);
        curvSwitch = make_shared<VSTGUI::C3PositionSwitchBox>(size, this, 'Curv', oPos1, oPos2, oPos3, offset);
        frame->addView(curvSwitch);
    }

    {
        auto oPos1 = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("pitch_1.png"));
        auto oPos2 = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("pitch_2.png"));
        auto oPos3 = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("pitch_3.png"));
        size(0, 0, 28, 10);
        size.offset(264, 226);
        pitchSwitch = make_shared<VSTGUI::C3PositionSwitchBox>(size, this, 'PtCr', oPos1, oPos2, oPos3, offset);
        frame->addView(pitchSwitch);
    }


    //---PadTypes--------------------
    {
        auto padType1 = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("fx_blue.png"));
        auto padType2 = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("fx_green.png"));
        auto padType3 = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("x_red.png"));
        auto padType4 = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("x_empty.png"));

        size(0, 0, 47, 47);
        size.offset(61, 52);
        padType[0] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt00', padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType[0]);
        size.offset(47, 0);
        padType[1] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt01', padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType[1]);
        size.offset(47, 0);
        padType[2] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt02', padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType[2]);
        size.offset(47, 0);
        padType[3] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt03', padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType[3]);
        size.offset(-141, 47);
        padType[4] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt04', padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType[4]);
        size.offset(47, 0);
        padType[5] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt05', padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType[5]);
        size.offset(47, 0);
        padType[6] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt06', padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType[6]);
        size.offset(47, 0);
        padType[7] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt07', padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType[7]);
        size.offset(-141, 47);
        padType[8] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt08', padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType[8]);
        size.offset(47, 0);
        padType[9] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt09', padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType[9]);
        size.offset(47, 0);
        padType[10] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt00' + 10, padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType[10]);
        size.offset(47, 0);
        padType[11] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt00' + 11, padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType[11]);
        size.offset(-141, 47);
        padType[12] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt00' + 12, padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType[12]);
        size.offset(47, 0);
        padType[13] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt00' + 13, padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType[13]);
        size.offset(47, 0);
        padType[14] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt00' + 14, padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType[14]);
        size.offset(47, 0);
        padType[15] = make_shared<VSTGUI::C5PositionView>(size, this, 'Pt00' + 15, padType4, padType1, padType2, padType3, padType4, offset);
        frame->addView(padType[15]);
    }

    //---Pads--------------------
    {
        auto kickOn = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("kick_button_act.png"));
        auto kickOff = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("kick_button.png"));

        size(0, 0, 40, 40);
        size.offset(65, 56);

        padKick[0] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd00', kickOff, kickOn, offset);
        padKick[0]->setPopupMenu(popupPad);
        padKick[0]->BeforePopup = &AVinylEditorView::callBeforePopup;
        frame->addView(padKick[0]);
        size.offset(47, 0);
        padKick[1] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd01', kickOff, kickOn, offset);
        padKick[1]->setPopupMenu(popupPad);
        padKick[1]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView(padKick[1]);
        size.offset(47, 0);
        padKick[2] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd02', kickOff, kickOn, offset);
        padKick[2]->setPopupMenu(popupPad);
        padKick[2]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView (padKick[2]);
        size.offset(47, 0);
        padKick[3] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd03', kickOff, kickOn, offset);
        padKick[3]->setPopupMenu(popupPad);
        padKick[3]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView (padKick[3]);
        size.offset(-141, 47);
        padKick[4] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd04', kickOff, kickOn, offset);
        padKick[4]->setPopupMenu(popupPad);
        padKick[4]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView (padKick[4]);
        size.offset(47, 0);
        padKick[5] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd05', kickOff, kickOn, offset);
        padKick[5]->setPopupMenu(popupPad);
        padKick[5]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView (padKick[5]);
        size.offset(47, 0);
        padKick[6] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd06', kickOff, kickOn, offset);
        padKick[6]->setPopupMenu(popupPad);
        padKick[6]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView(padKick[6]);
        size.offset(47, 0);
        padKick[7] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd07', kickOff, kickOn, offset);
        padKick[7]->setPopupMenu(popupPad);
        padKick[7]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView(padKick[7]);
        size.offset(-141, 47);
        padKick[8] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd08', kickOff, kickOn, offset);
        padKick[8]->setPopupMenu(popupPad);
        padKick[8]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView(padKick[8]);
        size.offset(47, 0);
        padKick[9] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd09', kickOff, kickOn, offset);
        padKick[9]->setPopupMenu(popupPad);
        padKick[9]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView(padKick[9]);
        size.offset(47, 0);
        padKick[10] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd00' + 10, kickOff, kickOn, offset);
        padKick[10]->setPopupMenu(popupPad);
        padKick[10]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView(padKick[10]);
        size.offset(47, 0);
        padKick[11] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd00' + 11, kickOff, kickOn, offset);
        padKick[11]->setPopupMenu(popupPad);
        padKick[11]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView(padKick[11]);
        size.offset(-141, 47);
        padKick[12] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd00' + 12, kickOff, kickOn, offset);
        padKick[12]->setPopupMenu(popupPad);
        padKick[12]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView(padKick[12]);
        size.offset(47, 0);
        padKick[13] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd00' + 13, kickOff, kickOn, offset);
        padKick[13]->setPopupMenu(popupPad);
        padKick[13]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView(padKick[13]);
        size.offset(47, 0);
        padKick[14] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd00' + 14, kickOff, kickOn, offset);
        padKick[14]->setPopupMenu(popupPad);
        padKick[14]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView (padKick[14]);
        size.offset(47, 0);
        padKick[15] = make_shared<VSTGUI::CVinylKickButton>(size,this, 'Pd00' + 15, kickOff, kickOn, offset);
        padKick[15]->setPopupMenu(popupPad);
        padKick[15]->BeforePopup=&AVinylEditorView::callBeforePopup;
        frame->addView(padKick[15]);
    }
    //

    ////Wave View Window
    {
        auto glass = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("glass.png"));
        size(0, 0, 343, 83);
        size.offset(270, 54);
        wavView = make_shared<VSTGUI::CWaveView>(size, this, 'Wave', glass);
        frame->addView(wavView);
    }

    {
        size(0, 0, 512, 100);
        size.offset(0, kEditorHeight);

        debugFftView = make_shared<VSTGUI::CDebugFftView>(size, this, '_FFT');
        frame->addView(debugFftView);

        size.offset(0, 100);

        debugInputView = make_shared<VSTGUI::CDebugFftView>(size, this, '_INP');
        frame->addView(debugInputView);
    }


    ///////CheckBoxes/////////////////////////////////////////////////////////////////////
    {
        auto checkOff = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("loop_check.png"));
        {
            auto checkOn = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("loop_check_on.png"));
            size(0, 0, 55, 16);
            size.offset(267, 32);
            loopBox = make_shared<VSTGUI::CVinylCheckBox>(size,this, 'Loop', checkOn, checkOff, offset);
            frame->addView(loopBox);
        }

        {
            auto checkOn = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("sync_check_on.png"));

            size.offset(58, 0);
            syncBox = make_shared<VSTGUI::CVinylCheckBox>(size,this, 'Sync', checkOn, checkOff, offset);
            frame->addView(syncBox);
        }

        {
            auto checkOn = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("revers_check_on.png"));
            size.offset(58, 0);
            rvrsBox = make_shared<VSTGUI::CVinylCheckBox>(size,this, 'Rvrs', checkOn, checkOff, offset);
            frame->addView(rvrsBox);
        }
    }

    ///////Sample Knobs
    {
        auto handle = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("smal_handler.png"));
        auto background = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("smal_slider.png"));

        size(0, 0, 55, 12);
        size.offset(467, 34);
        ampSlide = make_shared<VSTGUI::CHorizontalSlider>(size, this, 'Ampl', offset, size.getWidth(), handle, background, offset, VSTGUI::CSliderBase::kLeft);
        frame->addView(ampSlide);

        size.offset (89, 0);
        tuneSlide = make_shared<VSTGUI::CHorizontalSlider>(size, this, 'Tune', offset, size.getWidth(), handle, background, offset, VSTGUI::CSliderBase::kLeft);
        frame->addView(tuneSlide);
    }


    //////////TimeCodeLearn
    {
        auto checkOff = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("tclearn_check.png"));
        auto checkOn = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("tclearn_check_on.png"));
        size(0, 0, 39, 8);
        size.offset(272, 124);
        tcLearnBox = make_shared<VSTGUI::CVinylCheckBox>(size,this, 'TCLr', checkOff, checkOn, offset);
        frame->addView(tcLearnBox);
    }


    /////////////////////////////////////////////////////////////
    {
        auto comboOn = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("yellow_combo.png"));
        auto comboOff = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("sample_combo.png"));
        size(0, 0, 354, 18);
        size.offset(263, 8);
        samplePopup = make_shared<VSTGUI::COptionMenu>(size, this, 'Comb', comboOff, comboOn, VSTGUI::CVinylPopupMenu::kNoTextStyle);
        samplePopup->addEntry(sampleBase, "Select sample");
        samplePopup->addEntry("-", 3, VSTGUI::CMenuItem::kSeparator);
        samplePopup->addEntry("Add new sample in base", 2, 0);
        samplePopup->addEntry("Replace this sample", 3, VSTGUI::CMenuItem::kDisabled);
        samplePopup->addEntry("Delete this sample from base", 4, VSTGUI::CMenuItem::kDisabled);

        frame->addView(samplePopup);
    }


    {
        auto fxBit = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("distorsion.png"));
        size(0, 0, 10, 11);
        size.offset(271, 55);
        dispDistorsion = make_shared<VSTGUI::CVuMeter>(size, fxBit, nullptr, 1, VSTGUI::CVuMeter::Style::kHorizontal);
        frame->addView(dispDistorsion);
    }
    {
        auto fxBit = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("preroll.png"));
        size.offset(12, 0);
        dispPreRoll = make_shared<VSTGUI::CVuMeter>(size, fxBit, nullptr, 1, VSTGUI::CVuMeter::Style::kHorizontal);
        frame->addView(dispPreRoll);
    }
    {
        auto fxBit = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("postroll.png"));
        size.offset(12, 0);
        dispPostRoll = make_shared<VSTGUI::CVuMeter>(size, fxBit, nullptr, 1, VSTGUI::CVuMeter::Style::kHorizontal);
        frame->addView(dispPostRoll);
    }
    {
        auto fxBit = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("hold.png"));
        size.offset (12, 0);
        dispHold = make_shared<VSTGUI::CVuMeter>(size, fxBit, nullptr, 1, VSTGUI::CVuMeter::Style::kHorizontal);
        frame->addView(dispHold);
    }
    {
        auto fxBit = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("freeze.png"));
        size.offset (12, 0);
        dispFreeze = make_shared<VSTGUI::CVuMeter>(size, fxBit, nullptr, 1, VSTGUI::CVuMeter::Style::kHorizontal);
        frame->addView(dispFreeze);
    }
    {
        auto fxBit = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("vintage.png"));
        size.offset (12, 0);
        dispVintage = make_shared<VSTGUI::CVuMeter>(size, fxBit, nullptr, 1, VSTGUI::CVuMeter::Style::kHorizontal);
        frame->addView(dispVintage);
    }
    {
        auto fxBit = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("locktone.png"));
        size.offset (12, 0);
        dispLockTone = make_shared<VSTGUI::CVuMeter>(size, fxBit, nullptr, 1, VSTGUI::CVuMeter::Style::kHorizontal);
        frame->addView(dispLockTone);
    }

    /////////////Text and labels ////////////////////////////////
    size(0, 0, 50, 10);
    size.offset(422, 230);
    pitchValue = make_shared<VSTGUI::CTextLabel>(size, "0.00%", nullptr, VSTGUI::CVinylPopupMenu::kNoFrame);
    pitchValue->setHoriAlign(VSTGUI::kCenterText);
    pitchValue->setBackColor(VSTGUI::CColor(0, 0, 0, 0));

    pitchValue->setFontColor(VSTGUI::CColor(200, 200, 200, 200));
    pitchValue->setAntialias(true);
    auto litleFont = make_shared<VSTGUI::CFontDesc>(*(pitchValue->getFont()));
    litleFont->setSize(10);
    litleFont->setStyle(VSTGUI::kNormalFace);
    pitchValue->setFont(litleFont);
    frame->addView(pitchValue);

    size(0, 0, 40, 10);
    size.offset(572, 124);
    speedValue = make_shared<VSTGUI::CTextLabel>(size, "|| 0.000", nullptr, VSTGUI::CVinylPopupMenu::kNoFrame);
    speedValue->setHoriAlign(VSTGUI::kLeftText);
    speedValue->setBackColor(VSTGUI::CColor(0, 0, 0, 0));
    speedValue->setFontColor(VSTGUI::CColor(10, 255, 10, 100));
    speedValue->setAntialias(true);
    speedValue->setFont(litleFont);
    frame->addView(speedValue);

    size(0, 0, 40, 25);
    size.offset(271, 52);
    sampleNumber = make_shared<VSTGUI::CTextLabel>(size, " ", nullptr, VSTGUI::CVinylPopupMenu::kNoFrame);
    sampleNumber->setHoriAlign(VSTGUI::kLeftText);
    sampleNumber->setBackColor(VSTGUI::CColor(0, 0, 0, 0));
    sampleNumber->setFontColor(VSTGUI::CColor(10,255,10, 100));
    sampleNumber->setAntialias(true);
    sampleNumber->setFont(litleFont);
    frame->addView(sampleNumber);

    size(0, 0, 210, 22); //15
    size.offset(367, 7);  //12
    nameEdit = make_shared<VSTGUI::CTextEdit>(size, this, 'Name', " ", nullptr, VSTGUI::CVinylPopupMenu::kNoFrame);
    nameEdit->setHoriAlign(VSTGUI::kLeftText);
    nameEdit->setBackColor(VSTGUI::CColor(0,0,0,0));
    auto medFont = make_shared<VSTGUI::CFontDesc>(*(nameEdit->getFont()));
    medFont->setSize(12);
    medFont->setStyle(VSTGUI::kBoldFace);
    nameEdit->setFont(medFont);
    frame->addView(nameEdit);

    size(0, 0, 40, 35);
    size.offset(198, 11);
    sceneValue = make_shared<VSTGUI::CTextLabel>(size, "1", nullptr, VSTGUI::CVinylPopupMenu::kNoFrame);
    sceneValue->setHoriAlign(VSTGUI::kCenterText);
    sceneValue->setBackColor(VSTGUI::CColor(0,0,0,0));
    sceneValue->setFontColor(VSTGUI::CColor(0,0,0,255));
    sceneValue->setAntialias(true);
    auto bigFont = make_shared<VSTGUI::CFontDesc>(*(sceneValue->getFont()));
    bigFont->setSize(28);
    bigFont->setStyle(VSTGUI::kBoldFace);
    sceneValue->setFont(bigFont);
    frame->addView(sceneValue);


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
    // while(!sampleBitmaps.empty()) {
    //     sampleBitmaps.back()->forget();
    //     sampleBitmaps.pop_back();
    // }
    sampleBitmaps.clear();

    gainKnob = nullptr;
    scenKnob = nullptr;
    ampSlide = nullptr;
    tuneSlide = nullptr;
    nameEdit = nullptr;

    pitchSlider = nullptr;
    volumeSlider = nullptr;
    for (int i = 0; i < ENumberOfPads; i++) {
        padKick[i] = nullptr;
    }
    for (int i = 0; i < ENumberOfPads; i++) {
        padType[i] = nullptr;
    }
    loopBox = nullptr;
    syncBox = nullptr;
    rvrsBox = nullptr;
    tcLearnBox = nullptr;
    pitchValue = nullptr;
    sceneValue = nullptr;
    speedValue = nullptr;
    sampleNumber = nullptr;
    vuLeftMeter = nullptr;
    vuRightMeter = nullptr;

    dispDistorsion = nullptr;
    dispPreRoll = nullptr;
    dispPostRoll = nullptr;
    dispHold = nullptr;
    dispFreeze = nullptr;
    dispVintage = nullptr;
    dispLockTone = nullptr;

    curvSwitch = nullptr;
    pitchSwitch = nullptr;
    samplePopup = nullptr;
    popupPad = nullptr;
    sampleBase = nullptr;
    padBase = nullptr;
    effectBase1 = nullptr;
    effectBase2 = nullptr;

    wavView = nullptr;
    debugFftView = nullptr;
    debugInputView = nullptr;

    getFrame()->removeAll(true);
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

    case 'Pd00': {
        IMessage *msg = controller->allocateMessage();
        if (msg) {
            String sampleName(nameEdit->getText());
            msg->setMessageID("touchPad");
            msg->getAttributes()->setInt("PadNumber", 0);
            msg->getAttributes()->setFloat("PadValue",
                                           pControl->getValueNormalized());
            controller->sendMessage(msg);
            msg->release();
        }
    }
        break;

    case 'Pd01': {
        IMessage *msg = controller->allocateMessage();
        if (msg) {
            String sampleName(nameEdit->getText());
            msg->setMessageID("touchPad");
            msg->getAttributes()->setInt("PadNumber", 1);
            msg->getAttributes()->setFloat("PadValue",
                                           pControl->getValueNormalized());
            controller->sendMessage(msg);
            msg->release();
        }
    }
        break;

    case 'Pd02': {
        IMessage *msg = controller->allocateMessage();
        if (msg) {
            String sampleName(nameEdit->getText());
            msg->setMessageID("touchPad");
            msg->getAttributes()->setInt("PadNumber", 2);
            msg->getAttributes()->setFloat("PadValue",
                                           pControl->getValueNormalized());
            controller->sendMessage(msg);
            msg->release();
        }
    }
        break;

    case 'Pd03': {
        IMessage *msg = controller->allocateMessage();
        if (msg) {
            String sampleName(nameEdit->getText());
            msg->setMessageID("touchPad");
            msg->getAttributes()->setInt("PadNumber", 3);
            msg->getAttributes()->setFloat("PadValue",
                                           pControl->getValueNormalized());
            controller->sendMessage(msg);
            msg->release();
        }
    }
        break;

    case 'Pd04': {
        IMessage *msg = controller->allocateMessage();
        if (msg) {
            String sampleName(nameEdit->getText());
            msg->setMessageID("touchPad");
            msg->getAttributes()->setInt("PadNumber", 4);
            msg->getAttributes()->setFloat("PadValue",
                                           pControl->getValueNormalized());
            controller->sendMessage(msg);
            msg->release();
        }
    }
        break;

    case 'Pd05': {
        IMessage *msg = controller->allocateMessage();
        if (msg) {
            String sampleName(nameEdit->getText());
            msg->setMessageID("touchPad");
            msg->getAttributes()->setInt("PadNumber", 5);
            msg->getAttributes()->setFloat("PadValue",
                                           pControl->getValueNormalized());
            controller->sendMessage(msg);
            msg->release();
        }
    }
        break;

    case 'Pd06': {
        IMessage *msg = controller->allocateMessage();
        if (msg) {
            String sampleName(nameEdit->getText());
            msg->setMessageID("touchPad");
            msg->getAttributes()->setInt("PadNumber", 6);
            msg->getAttributes()->setFloat("PadValue",
                                           pControl->getValueNormalized());
            controller->sendMessage(msg);
            msg->release();
        }
    }
        break;

    case 'Pd07': {
        IMessage *msg = controller->allocateMessage();
        if (msg) {
            String sampleName(nameEdit->getText());
            msg->setMessageID("touchPad");
            msg->getAttributes()->setInt("PadNumber", 7);
            msg->getAttributes()->setFloat("PadValue",
                                           pControl->getValueNormalized());
            controller->sendMessage(msg);
            msg->release();
        }
    }
        break;

    case 'Pd08': {
        IMessage *msg = controller->allocateMessage();
        if (msg) {
            String sampleName(nameEdit->getText());
            msg->setMessageID("touchPad");
            msg->getAttributes()->setInt("PadNumber", 8);
            msg->getAttributes()->setFloat("PadValue",
                                           pControl->getValueNormalized());
            controller->sendMessage(msg);
            msg->release();
        }
    }
        break;

    case 'Pd09': {
        IMessage *msg = controller->allocateMessage();
        if (msg) {
            String sampleName(nameEdit->getText());
            msg->setMessageID("touchPad");
            msg->getAttributes()->setInt("PadNumber", 9);
            msg->getAttributes()->setFloat("PadValue",
                                           pControl->getValueNormalized());
            controller->sendMessage(msg);
            msg->release();
        }
    }
        break;

    case 'Pd00' + 10: {
        IMessage *msg = controller->allocateMessage();
        if (msg) {
            String sampleName(nameEdit->getText());
            msg->setMessageID("touchPad");
            msg->getAttributes()->setInt("PadNumber", 10);
            msg->getAttributes()->setFloat("PadValue",
                                           pControl->getValueNormalized());
            controller->sendMessage(msg);
            msg->release();
        }
    }
        break;

    case 'Pd00' + 11: {
        IMessage *msg = controller->allocateMessage();
        if (msg) {
            String sampleName(nameEdit->getText());
            msg->setMessageID("touchPad");
            msg->getAttributes()->setInt("PadNumber", 11);
            msg->getAttributes()->setFloat("PadValue",
                                           pControl->getValueNormalized());
            controller->sendMessage(msg);
            msg->release();
        }
    }
        break;

    case 'Pd00' + 12: {
        IMessage *msg = controller->allocateMessage();
        if (msg) {
            String sampleName(nameEdit->getText());
            msg->setMessageID("touchPad");
            msg->getAttributes()->setInt("PadNumber", 12);
            msg->getAttributes()->setFloat("PadValue",
                                           pControl->getValueNormalized());
            controller->sendMessage(msg);
            msg->release();
        }
    }
        break;

    case 'Pd00' + 13: {
        IMessage *msg = controller->allocateMessage();
        if (msg) {
            String sampleName(nameEdit->getText());
            msg->setMessageID("touchPad");
            msg->getAttributes()->setInt("PadNumber", 13);
            msg->getAttributes()->setFloat("PadValue",
                                           pControl->getValueNormalized());
            controller->sendMessage(msg);
            msg->release();
        }
    }
        break;

    case 'Pd00' + 14: {
        IMessage *msg = controller->allocateMessage();
        if (msg) {
            String sampleName(nameEdit->getText());
            msg->setMessageID("touchPad");
            msg->getAttributes()->setInt("PadNumber", 14);
            msg->getAttributes()->setFloat("PadValue",
                                           pControl->getValueNormalized());
            controller->sendMessage(msg);
            msg->release();
        }
    }
        break;

    case 'Pd00' + 15: {
        IMessage *msg = controller->allocateMessage();
        if (msg) {
            String sampleName(nameEdit->getText());
            msg->setMessageID("touchPad");
            msg->getAttributes()->setInt("PadNumber", 15);
            msg->getAttributes()->setFloat("PadValue",
                                           pControl->getValueNormalized());
            controller->sendMessage(msg);
            msg->release();
        }
    }
        break;

    case 'Name':
    {
        if (sampleBase) sampleBase->getEntry(int32_t(currentEntry))->setTitle(nameEdit->getText());
        if (padBase) padBase->getEntry(int32_t(currentEntry))->setTitle(nameEdit->getText());
        IMessage* msg = controller->allocateMessage ();
        if (msg)
        {
            String sampleName (nameEdit->getText());
            msg->setMessageID ("renameEntry");
            msg->getAttributes ()->setInt("SampleNumber", currentEntry);
            msg->getAttributes ()->setString("SampleName", sampleName);
            controller->sendMessage (msg);
            msg->release ();
        }

    }	break;
    //------------------
    case 'PpUp':
    {
        int menuIndex;
        VSTGUI::COptionMenu * SelectedMenu = popupPad->getLastItemMenu(menuIndex);
        if (SelectedMenu == padBase) {
            setPadMessage(padForSetting + 1, PadEntry::SamplePad, menuIndex);
        }
        if (SelectedMenu == effectBase1) {
            setPadMessage(padForSetting + 1, PadEntry::SwitchPad, switchedFx[menuIndex]);
        }
        if (SelectedMenu == effectBase2) {
            setPadMessage(padForSetting + 1, PadEntry::KickPad, punchFx[menuIndex]);
        }
        if (SelectedMenu == popupPad) {
            if (menuIndex==0){
                setPadMessage(padForSetting + 1, PadEntry::AssigMIDI);
            }
            if (menuIndex==6){
                setPadMessage(padForSetting + 1, PadEntry::EmptyPad, -1);
            }
        }


    }	break;
    //------------------
    case 'Comb':
    {
        int menuIndex;
        VSTGUI::COptionMenu * SelectedMenu = samplePopup->getLastItemMenu(menuIndex);
        if (SelectedMenu == sampleBase) {
            float normalValue = (float)menuIndex/(float)(EMaximumSamples-1);
            controller->setParamNormalized (kCurrentEntryId, normalValue);
            controller->performEdit (kCurrentEntryId, normalValue);
        }
        if (SelectedMenu == samplePopup) {
            if (menuIndex==2) {

                ////////////Open FileSelector Multy/////////////////////////
                auto selector = VSTGUI::SharedPointer(VSTGUI::CNewFileSelector::create(frame, VSTGUI::CNewFileSelector::kSelectFile), false);
                if (selector) {
                    selector->kSelectEndMessage = "LoadingFile";
                    selector->setDefaultExtension (VSTGUI::CFileExtension ("WAVE", "wav"));
                    selector->setTitle("Choose Samples for load in base");
                    selector->setAllowMultiFileSelection(true);
                    selector->run(this);
                    //selector->forget();
                }
            }
            if (menuIndex==3){
                ////////////Open FileSelector Single/////////////////////////
                auto selector = VSTGUI::SharedPointer(VSTGUI::CNewFileSelector::create(frame, VSTGUI::CNewFileSelector::kSelectFile), false);
                if (selector) {
                    selector->kSelectEndMessage = "ReplaceFile";
                    selector->setDefaultExtension(VSTGUI::CFileExtension ("WAVE", "wav"));
                    selector->setTitle("Choose Sample for replace");
                    selector->setAllowMultiFileSelection(false);
                    selector->run(this);
                    //selector->forget();
                }
            }
            if (menuIndex==4){
                //TODO: create own dialogue
                //if (IDYES==MessageBox(0,STR("Are you sure?"),STR("Delete from base"),MB_YESNO|MB_ICONWARNING|MB_DEFBUTTON2|MB_APPLMODAL)) {
                IMessage* msg = controller->allocateMessage();
                if (msg) {
                    msg->setMessageID("deleteEntry");
                    msg->getAttributes()->setInt("SampleNumber", currentEntry);
                    //msg->getAttributes()->setString("SampleName", STR(SelectedMenu->getEntry(menuIndex)->getTitle()));
                    controller->sendMessage(msg);
                    msg->release();
                }
                //}
            }
        }
    }	break;

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
        if (gainKnob) {
            gainKnob->setValueNormalized((float)value);
        }
        break;

    case kAmpId:
        if (ampSlide) {
            ampSlide->setValueNormalized((float)value);
        }
        break;

    case kTuneId:
        if (tuneSlide) {
            tuneSlide->setValueNormalized((float)value);
        }
        break;

    case kVolumeId:
        if (volumeSlider) {
            volumeSlider->setValueNormalized((float)value);
        }
        break;

    case kSyncId:
        if (syncBox) {
            syncBox->setValueNormalized((float)value);
        }
        break;

    case kReverseId:
        if (rvrsBox) {
            rvrsBox->setValueNormalized((float)value);
        }
        break;

    case kTimecodeLearnId:
        if (tcLearnBox) {
            tcLearnBox->setValueNormalized((float)value);
        }
        break;

    case kVolCurveId:
        if (curvSwitch) {
            curvSwitch->setValueNormalized((float)value);
        }
        break;

    case kLoopId:
        if (loopBox) {
            loopBox->setValueNormalized((float)value);
            if(wavView) {
                wavView->setLoop(loopBox->getChecked());
            }
        }
        break;

    case kPitchId:
        if (pitchSlider) {
            pitchSlider->setValueNormalized((float)value);
        }
        if(pitchValue && pitchSwitch && pitchSlider) {
            String textval;
            if (pitchSwitch->getPosition() == 0) {
                textval = "0.00%";
            } else if (pitchSwitch->getPosition() == 2) {
                textval.printf(STR("%0.2f%"), pitchSlider->getValue() * 100. - 50.);
            } else {
                textval.printf(STR("%0.2f%"), pitchSlider->getValue() * 20. - 10.);
            }
            pitchValue->setText(VSTGUI::UTF8String(textval));
        }
        break;

    case kPitchSwitchId:
        if (pitchSwitch) {
            pitchSwitch->setValueNormalized((float)value);
        }
        if(pitchValue && pitchSwitch && pitchSlider) {
            String textval;
            if (pitchSwitch->getPosition() == 0) {
                textval = "0.00%";
            } else if (pitchSwitch->getPosition() == 2) {
                textval.printf(STR("%0.2f%"), pitchSlider->getValue() * 100. - 50.);
            } else {
                textval.printf(STR("%0.2f%"), pitchSlider->getValue() * 20. - 10.);
            }
            pitchValue->setText(VSTGUI::UTF8String(textval));
        }
        break;

    case kCurrentSceneId:
        if (scenKnob)
        {
            scenKnob->setValueNormalized((float)value);
        }
        if(sceneValue){
            int currentScene = floor(value * float(EMaximumScenes - 1) + 0.5);
            String textval;
            textval = textval.printf("%d",currentScene + 1);
            sceneValue->setText(VSTGUI::UTF8String(textval));
        }
        break;

    case kCurrentEntryId:
    {
        int64_t newCurEntry = floor(value * float(EMaximumSamples - 1) + 0.5);
        if (newCurEntry != currentEntry) {
            currentEntry = floor(value * float(EMaximumSamples - 1) + 0.5);

            if ((currentEntry < int64_t(sampleBitmaps.size())) && (currentEntry >= 0)) {

                if (wavView) {
                    wavView->setWave(sampleBitmaps.at(currentEntry));
                    wavView->setPosition(0);
                }
                if (sampleBase) {
                    sampleBase->setCurrent(int32_t(currentEntry), true);
                }
                String textval;
                if (sampleNumber) {
                    sampleNumber->setText(VSTGUI::UTF8String(textval.printf("#%03d", currentEntry + 1)));
                }
                if (nameEdit && sampleBase) {
                    nameEdit->setText(sampleBase->getEntry(int32_t(currentEntry))->getTitle());
                }
            }
        }
    }
        break;

    case kVuLeftId:
        lastVuLeftMeterValue = float(value);
        break;

    case kVuRightId:
        lastVuRightMeterValue = float(value);
        break;

    case kDistorsionId:
        lastDistort = float(value) > 0.5f;
        break;

    case kPreRollId:
        lastPreRoll = float(value) > 0.5f;
        break;

    case kPostRollId:
        lastPostRoll = float(value) > 0.5f;
        break;

    case kHoldId:
        lastHold = float(value) > 0.5;
        break;

    case kFreezeId:
        lastFreeze = float(value) > 0.5;
        break;

    case kVintageId:
        lastVintage = float(value) > 0.5;
        break;

    case kLockToneId:
        lastLockTone = float(value) > 0.5;
        break;

    case kPositionId:
        if (wavView) {
            wavView->setPosition(float(value));
        }
        break;
    default:
        break;
    }
}

//------------------------------------------------------------------------
VSTGUI::CMessageResult AVinylEditorView::notify (CBaseObject* sender, const char* message)
{

    if (message == VSTGUI::CVSTGUITimer::kMsgTimer) {
        if (vuLeftMeter) {
            vuLeftMeter->setValue((vuLeftMeter->getValue() * 4. + (1. - lastVuLeftMeterValue)) / 5.);
            lastVuLeftMeterValue = 1.f;
        }
        if (vuRightMeter) {
            vuRightMeter->setValue((vuRightMeter->getValue() * 4. + (1. - lastVuRightMeterValue)) / 5.);
            lastVuRightMeterValue = 1.f;
        }
        if (dispDistorsion) {
            dispDistorsion->setValue(lastDistort ? 1 : 0);
        }
        if (dispPreRoll) {
            dispPreRoll->setValue(lastPreRoll ? 1 : 0);
        }
        if (dispPostRoll) {
            dispPostRoll->setValue(lastPostRoll ? 1 : 0);
        }
        if (dispHold) {
            dispHold->setValue(lastHold ? 1 : 0);
        }
        if (dispFreeze) {
            dispFreeze->setValue (lastFreeze ? 1 : 0);
        }
        if (dispVintage) {
            dispVintage->setValue (lastVintage ? 1 : 0);
        }
        if (dispLockTone) {
            dispLockTone->setValue (lastLockTone ? 1 : 0);
        }
    }
    if (message == VSTGUI::CNewFileSelector::kSelectEndMessage) {
        VSTGUI::CNewFileSelector* sel = dynamic_cast<VSTGUI::CNewFileSelector*>(sender);
        if (sel) {
            if (strcmp(sel->kSelectEndMessage,"LoadingFile")==0) {
                size_t MaxCount = (sel->getNumSelectedFiles()<(EMaximumSamples - sampleBitmaps.size())) ?
                                     sel->getNumSelectedFiles() :
                                     EMaximumSamples-sampleBitmaps.size();
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
    if (_speed != lastSpeedValue) {
        lastSpeedValue = _speed;
        if (speedValue) {
            String tmp;
            if (lastSpeedValue > 0) {
                tmp = tmp.printf("> %0.3f",fabs(lastSpeedValue));
            } else if (lastSpeedValue < 0) {
                tmp = tmp.printf("< %0.3f",fabs(lastSpeedValue));
            } else {
                tmp = tmp.printf("|| %0.3f",fabs(lastSpeedValue));
            }
            speedValue->setText(VSTGUI::UTF8String(tmp));
        }

    }
}

void AVinylEditorView::setPositionMonitor(double _position)
{
    if (_position != lastPositionValue) {
        lastPositionValue = _position;
        if (wavView) {
            wavView->setPosition((float)_position);
        }

    }
}

AVinylEditorView::SharedPointer<VSTGUI::CBitmap> AVinylEditorView::generateWaveform(SampleEntry<Sample32> * newEntry, bool normolize)
{
    //int bitmapWidth = double(newEntry->bufferLength()) * 0.0015;
    size_t bitmapWidth = newEntry->bufferLength();
    if (bitmapWidth < 400) {
        bitmapWidth = 400;
    }
    else if (bitmapWidth > 800) {
        bitmapWidth = 800;
    }
    else if (bitmapWidth % 2) {
        bitmapWidth++;
    }

    //if (bitmapWidth <  2) {
    //    return nullptr;
    //}

    SampleEntry<Sample32>::Type norm = normolize ? 1. / newEntry->peakSample(0, newEntry->bufferLength()) : 1.;

    bool drawBeats =  newEntry->getACIDbeats() > 0;

    double beatPeriodic = drawBeats ? double(bitmapWidth) / (2. * newEntry->getACIDbeats()) : 0;
    auto Digits = make_shared<VSTGUI::CBitmap>(VSTGUI::CResourceDescription("digits.png"));
    auto waveForm = make_shared<VSTGUI::CBitmap>(bitmapWidth, 83);
    if (waveForm && newEntry->bufferLength() > 0) {
        auto PixelMap = VSTGUI::SharedPointer(VSTGUI::CBitmapPixelAccess::create(waveForm), false);
        auto PixelDigits = VSTGUI::SharedPointer(VSTGUI::CBitmapPixelAccess::create(Digits), false);
        PixelMap->setPosition(0, 61);
        PixelMap->setColor(VSTGUI::CColor(0, 255, 0, 220));
        double stretch = double(newEntry->bufferLength()) / (bitmapWidth / 2.);
        for (uint32_t i = 1; i < bitmapWidth / 2; i++) {
            SampleEntry<Sample32>::Type height = norm * newEntry->peakSample((i - 1) * stretch, i * stretch);
            PixelMap->setPosition((i - 1) * 2, 61);
            PixelMap->setColor(VSTGUI::CColor(0, 255, 0, 120));
            for (uint32_t j = 1; j < 40 * height; j++) {
                PixelMap->setPosition((i - 1) * 2, 61 - j * 2);
                PixelMap->setColor(VSTGUI::CColor(100. * (1. - j / (60. * height)), 255, 0, 40 + 180 * sqr(1. - j / (60. * height))));
                if ((61 + j * 2) <= 81) {
                    PixelMap->setPosition((i - 1) * 2, 61 + j * 2);
                    PixelMap->setColor(VSTGUI::CColor(100. * (1. - j / (60. * height)), 255, 0, 80. * sqr(1. - j / (60. * height))));
                }
            }
        }
        for (uint32_t i = 1; i <= bitmapWidth / 2; i++) {
            //if ((drawBeats)&&((((double)i-1.0) / beatPeriodic) >= (double)((int)(((double)i-1.0) / beatPeriodic)))&&((((double)i-1.0) / beatPeriodic) < (double)((int)(((double)i) / beatPeriodic)+1.0))) {
            if (drawBeats && ((i == 1) || ((int((i - 2.) / beatPeriodic)) != (int((i - 1.) / beatPeriodic))))) {
                for (uint32_t j = 0; j <= 5; j++) {
                    for(uint32_t k = 0; k <= 5; k++) {
                        PixelMap->setPosition((i - 1) * 2 + j + 2, 76 + k);
                        PixelDigits->setPosition((((i - 1) / (int)beatPeriodic) % 10) * 5 + j, k);
                        VSTGUI::CColor DigPixel;
                        PixelDigits->getColor(DigPixel);
                        PixelMap->setColor(DigPixel);
                    }
                }
                for (uint32_t j = 61; j < 85; j += 2) {
                    PixelMap->setPosition((i - 1) * 2, j);
                    PixelMap->setColor(VSTGUI::CColor(255, 255, 255, (j - 58) * 6 + 10));
                }
            }
        }
        return waveForm;
    }
    return nullptr;
}

void AVinylEditorView::delEntry(size_t delEntryIndex)
{
    if ((delEntryIndex >= 0) && (delEntryIndex < sampleBitmaps.size())) {

        //VSTGUI::CBitmap * waveForm = sampleBitmaps.at(delEntryIndex);

        sampleBitmaps.erase(sampleBitmaps.begin() + delEntryIndex);
        //waveForm->forget();
        if (sampleBase) {
            sampleBase->removeEntry(int32_t(delEntryIndex));
            if (padBase) {
                padBase->removeEntry(int32_t(delEntryIndex));
            }
            if (currentEntry >= sampleBase->getNbEntries()) {
                currentEntry = sampleBase->getNbEntries() - 1;
            }
            if (sampleBase->getNbEntries()==0) {
                sampleBase->addEntry(EEmptyBaseTitle, 0, VSTGUI::CMenuItem::kTitle|VSTGUI::CMenuItem::kDisabled);
                if (padBase) {
                    padBase->addEntry(EEmptyBaseTitle, 0, VSTGUI::CMenuItem::kTitle|VSTGUI::CMenuItem::kDisabled);
                }
                currentEntry = 0;
                if (wavView) {
                    wavView->setWave(nullptr);
                }
                if (nameEdit) {
                    nameEdit->setText(0);
                }
                if (sampleNumber) {
                    sampleNumber->setText(" ");
                }

                if (samplePopup) {
                    samplePopup->getEntry(3)->setEnabled(false);
                    samplePopup->getEntry(4)->setEnabled(false);
                    if (EMaximumSamples > samplePopup->getNbEntries()) {
                        samplePopup->getEntry(2)->setEnabled(true);
                    }
                }
            } else {
                controller->setParamNormalized (kCurrentEntryId, (float)currentEntry/(EMaximumSamples-1));
                controller->performEdit (kCurrentEntryId, (float)currentEntry/(EMaximumSamples-1));
            }
        }
    }
}

void AVinylEditorView::initEntry(SampleEntry<Sample32> * newEntry)
{
    if (newEntry) {
        auto waveForm = generateWaveform(newEntry);
        if (!waveForm) {
            // not readable format
            return;
        }

        size_t index = newEntry->index() - 1;
        if (index < sampleBitmaps.size()) {
            sampleBitmaps[index] = std::move(waveForm);

            if (sampleBase) {
                sampleBase->getEntry(int32_t(index))->setTitle(VSTGUI::UTF8String(newEntry->name()));
            }
            if (padBase) {
                padBase->getEntry(int32_t(index))->setTitle(VSTGUI::UTF8String(newEntry->name()));
            }
        } else {

            index = sampleBitmaps.size();
            sampleBitmaps.push_back(std::move(waveForm));

            if (sampleBase) {
                if (strcmp(sampleBase->getEntry(0)->getTitle(), EEmptyBaseTitle)==0) {
                    sampleBase->removeAllEntry();
                    if (padBase) padBase->removeAllEntry();
                    if (samplePopup) {
                        samplePopup->getEntry(3)->setEnabled(true);
                        samplePopup->getEntry(4)->setEnabled(true);
                        if (EMaximumSamples <= samplePopup->getNbEntries()) {
                            samplePopup->getEntry(2)->setEnabled(false);
                        }
                    }
                }
                sampleBase->addEntry(VSTGUI::UTF8String(newEntry->name()),sampleBase->getNbEntries(), VSTGUI::CMenuItem::kChecked);
                if (padBase) {
                    padBase->addEntry(VSTGUI::UTF8String(newEntry->name()), sampleBase->getNbEntries(), 0);
                }
            }
        }

        if (index == currentEntry) {
            if (samplePopup) {
                sampleBase->setCurrent(currentEntry, true);
            }
            if (wavView) {
                wavView->setWave(sampleBitmaps[index]);
                wavView->setLoop(newEntry->Loop);
            }
            if (nameEdit) {
                nameEdit->setText(VSTGUI::UTF8String(newEntry->name()));
            }
            if (sampleNumber) {
                String tmp;
                sampleNumber->setText(VSTGUI::UTF8String(tmp.printf("#%03d",newEntry->index())));
            }
        }
    }
}

void AVinylEditorView::debugFft(SampleEntry<Sample32> *newEntry)
{
    if (newEntry) {
        auto waveForm = generateWaveform(newEntry, true);
        if (!waveForm) {
            // not readable format
            return;
        }

        if (debugFftView) {
            debugFftView->setWave(waveForm);
        }
    }
}

void AVinylEditorView::debugInput(SampleEntry<Sample32> *newEntry)
{
    if (newEntry) {
        auto waveForm = generateWaveform(newEntry, true);
        if (!waveForm) {
            // not readable format
            return;
        }

        if (debugInputView) {
            debugInputView->setWave(waveForm);
        }
    }
}

void AVinylEditorView::setPadState(int _pad, bool _state)
{
    padKick[_pad]->setActive(_state);
}
void  AVinylEditorView::setPadType(int _pad,int _type)
{
    padType[_pad]->setPosition(_type);
}

void  AVinylEditorView::setPadTag(int _pad,int _tag)
{
    PadTag[_pad] = _tag;
}

bool AVinylEditorView::callBeforePopup(VSTGUI::IControlListener *listener, VSTGUI::CControl* pControl)
{
    AVinylEditorView *view = dynamic_cast<AVinylEditorView*>(listener);
    if (!view) {
        return false;
    }
    view->padForSetting = pControl->getTag() - 'Pd00';

    switch (view->padType[view->padForSetting]->getPosition()) {
    case PadEntry::SamplePad:
        if (view->padBase) {
            view->padBase->setCurrent(view->PadTag[view->padForSetting],false);
        }
        if (view->effectBase1) {
            view->effectBase1->setCurrent(-1,false);
        }
        if (view->effectBase2) {
            view->effectBase2->setCurrent(-1,false);
        }
        break;
    case PadEntry::SwitchPad:
        if (view->padBase) {
            view->padBase->setCurrent(-1,false);
        }
        if (view->effectBase1) {
            view->effectBase1->setCurrent(bitNumber(view->PadTag[view->padForSetting]),false);
        }
        if (view->effectBase2) {
            view->effectBase2->setCurrent(-1,false);
        }
        break;
    case PadEntry::KickPad:
        if (view->padBase) {
            view->padBase->setCurrent(-1,false);
        }
        if (view->effectBase1) {
            view->effectBase1->setCurrent(-1,false);
        }
        if (view->effectBase2) {
            view->effectBase2->setCurrent(bitNumber(view->PadTag[view->padForSetting]),false);
        }
        break;
    default:
        if (view->padBase) {
            view->padBase->setCurrent(-1,false);
        }
        if (view->effectBase1) {
            view->effectBase1->setCurrent(-1,false);
        }
        if (view->effectBase2) {
            view->effectBase2->setCurrent(-1,false);
        }
        break;
    }
    return true;
}

void AVinylEditorView::setPadMessage(int _pad,int _type,int _tag)
{
    IMessage* msg = controller->allocateMessage ();
    if (msg)
    {
        msg->setMessageID("setPad");
        msg->getAttributes()->setInt("PadNumber", _pad);
        msg->getAttributes()->setInt("PadType", _type);
        msg->getAttributes()->setInt("PadTag", _tag);
        controller->sendMessage(msg);
        msg->release();
    }
}

void AVinylEditorView::setPadMessage(int _pad,int _type)
{
    IMessage* msg = controller->allocateMessage ();
    if (msg)
    {
        msg->setMessageID("setPad");
        msg->getAttributes()->setInt("PadNumber", _pad);
        msg->getAttributes()->setInt("PadType", _type);
        controller->sendMessage(msg);
        msg->release();
    }
}

}} // namespaces
