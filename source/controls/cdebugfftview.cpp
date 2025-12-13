#include "cdebugfftview.h"

#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/cdrawcontext.h"
//#include "vstgui/lib/cframe.h"


namespace VSTGUI {

CDebugFftView::CDebugFftView (const CRect &size, IControlListener *listener, int32_t tag, CBitmap *background)
    :CControl(size, listener, tag, background)
{
    setWantsFocus (false);
}

CDebugFftView::CDebugFftView(const CDebugFftView &view)
    : CControl(view)
    , wave_(view.wave_)
{
    setWantsFocus (true);
}

CDebugFftView::~CDebugFftView ()
{
}

void CDebugFftView::setWave(const SharedPointer<CBitmap> &newWave)
{
    wave_ = newWave;
    setDirty (true);
}

void CDebugFftView::draw (CDrawContext* pContext)
{
    // for (int i = 0; i < getVisibleViewSize().getWidth() / 2; i++) {
    //     pContext->drawPoint(CPoint(getVisibleViewSize().left + i * 2,
    //                                getVisibleViewSize().top + 61),
    //                         CColor(0, 255, 0, 120));
    // }
    pContext->setFillColor(CColor(0, 0, 0, 70));
    pContext->setDrawMode(kAliasing);
    pContext->drawRect(getVisibleViewSize(), VSTGUI::kDrawFilled);
    if (wave_) {
        wave_->draw(pContext, getVisibleViewSize());
    }
    // if (getDrawBackground()) {
    //     getDrawBackground()->draw(pContext, getVisibleViewSize(),CPoint(0,0));
    // }
}

CMouseEventResult CDebugFftView::onMouseDown(CPoint& where, const CButtonState& buttons)
{
    valueChanged ();
    invalid();
    return kMouseEventHandled;
}

CMouseEventResult CDebugFftView::onMouseUp(CPoint& where, const CButtonState& buttons)
{
    return kMouseEventHandled;
}

CMouseEventResult CDebugFftView::onMouseMoved(CPoint& where, const CButtonState& buttons)
{
    return kMouseEventHandled;
}

} // namespace
