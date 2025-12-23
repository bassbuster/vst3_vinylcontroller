#pragma once

#include "vstgui/lib/controls/ccontrol.h"
// #include "vstgui/lib/controls/coptionmenu.h"
// #include "vstgui/lib/cfont.h"
// #include "vstgui/lib/ccolor.h"

namespace VSTGUI {

//-----------------------------------------------------------------------------
// CWaveView Declaration
//-----------------------------------------------------------------------------
class CWaveView : public CControl
{
public:
    CWaveView(const CRect &size, IControlListener *listener = nullptr, int32_t tag = 0, CBitmap *pBackground = nullptr);
    CWaveView(const CWaveView &v);
    ~CWaveView();

    void setWave(const SharedPointer<CBitmap> &newWave);

    void setPosition(float position);

    void setLoop(bool loop);

    bool getLoop (void) const {
        return loop_;
    }

    void draw(CDrawContext* pContext) override;

    CMouseEventResult onMouseDown(CPoint& where, const CButtonState& buttons) override;
    CMouseEventResult onMouseUp(CPoint& where, const CButtonState& buttons) override;
    CMouseEventResult onMouseMoved(CPoint& where, const CButtonState& buttons) override;

	CLASS_METHODS(CWaveView, CControl)

private:
    SharedPointer<CBitmap> wave_;
    int pixelPreroll_;
    bool loop_;

};

} // namespace

