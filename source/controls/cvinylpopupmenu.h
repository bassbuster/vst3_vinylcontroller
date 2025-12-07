#pragma once

#include "vstgui/lib/controls/ccontrol.h"
#include "vstgui/lib/controls/coptionmenu.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/ccolor.h"

namespace VSTGUI {

//-----------------------------------------------------------------------------
// CCheckBox Declaration
//-----------------------------------------------------------------------------
class CVinylPopupMenu : public COptionMenu
{
private:
	CRect sizeClick;
public:
    CVinylPopupMenu (IControlListener* listener, int32_t tag, CBitmap* bgWhenClick = 0, CRect clickedOffsets = {});
	//~CVinylPopupMenu();
	bool popup(CControl * popupControl);
    void draw (CDrawContext* pContext) override;
};

} // namespace

