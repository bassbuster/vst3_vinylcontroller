#pragma once

#include "vstgui/lib/controls/ccontrol.h"
// #include "vstgui/lib/controls/coptionmenu.h"
// #include "vstgui/lib/cfont.h"
// #include "vstgui/lib/ccolor.h"

namespace VSTGUI {

//-----------------------------------------------------------------------------
// CDebugFftView Declaration
//-----------------------------------------------------------------------------
class CDebugFftView : public CControl
{
public:
    CDebugFftView(const CRect &size, IControlListener *listener = nullptr, int32_t tag = 0, CBitmap *background = nullptr);
    CDebugFftView(const CDebugFftView &v);
    ~CDebugFftView();

    void setWave(const SharedPointer<CBitmap> &newWave);

    void draw (CDrawContext* pContext) override;

    CMouseEventResult onMouseDown(CPoint& where, const CButtonState& buttons) override;
    CMouseEventResult onMouseUp(CPoint& where, const CButtonState& buttons) override;
    CMouseEventResult onMouseMoved(CPoint& where, const CButtonState& buttons) override;

    CLASS_METHODS(CDebugFftView, CControl)

private:
    SharedPointer<CBitmap> wave_;
};

} // namespace


