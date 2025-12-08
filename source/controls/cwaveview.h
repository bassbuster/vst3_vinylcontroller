#pragma once

#include "vstgui/lib/controls/ccontrol.h"
#include "vstgui/lib/controls/coptionmenu.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/ccolor.h"

namespace VSTGUI {

//-----------------------------------------------------------------------------
// CWaveView Declaration
//-----------------------------------------------------------------------------
class CWaveView : public CControl
{
public:
    CWaveView (const CRect &size, IControlListener *listener=0, int32_t tag=0, CBitmap *pBackground=0, CBitmap *_pWave=0);
	CWaveView (const CWaveView &v);
	void setWave(CBitmap *newWave);
	void setPosition(float _position);
	void setLoop(bool _loop);
	bool getLoop (void) {return Loop;}

    void draw (CDrawContext* pContext) override;

    CMouseEventResult onMouseDown (CPoint& where, const CButtonState& buttons) override;
    CMouseEventResult onMouseUp (CPoint& where, const CButtonState& buttons) override;
    CMouseEventResult onMouseMoved (CPoint& where, const CButtonState& buttons) override;

	CLASS_METHODS(CWaveView, CControl)
protected:
	~CWaveView();
	CBitmap * pWave;
private:
	int pixel_preroll;
	bool Loop;

};

} // namespace

