#include "cvinylpopupmenu.h"

#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/cframe.h"


namespace VSTGUI {

//------------------------------------------------------------------------
CVinylPopupMenu::CVinylPopupMenu(IControlListener* listener, int32_t tag, CBitmap* bgWhenClick, CRect clickedOffsets)
    : COptionMenu(CRect(0, 0, 48, 48), listener, tag, bgWhenClick, bgWhenClick, kNoTextStyle)
    , sizeClick(clickedOffsets)
{
 ///
}



//------------------------------------------------------------------------
bool CVinylPopupMenu::popup(CControl * popupControln)
{
    if (popupControln == nullptr) {
		return false;
    }
    if (isAttached()) {
		return false;
    }
    CView* oldFocusView = popupControln->getFrame()->getFocusView();
    CRect size(popupControln->getVisibleViewSize().left + sizeClick.left, popupControln->getVisibleViewSize().top + sizeClick.top,
               popupControln->getVisibleViewSize().right + sizeClick.right, popupControln->getVisibleViewSize().bottom + sizeClick.bottom);

    setViewSize(size);
    popupControln->getFrame()->addView(this);
    COptionMenu::popup();
    popupControln->getFrame()->removeView(this, false);
    popupControln->getFrame()->setFocusView(oldFocusView);
	int32_t index;
    return getLastItemMenu(index);
}

//------------------------------------------------------------------------
void CVinylPopupMenu::draw(CDrawContext * pContext)
{
    if (bgWhenClick) {
        bgWhenClick->draw(pContext, getVisibleViewSize(), CPoint(0, 0));
    }
}

} // namespace
