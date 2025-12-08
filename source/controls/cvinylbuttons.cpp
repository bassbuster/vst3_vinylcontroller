#include "cvinylbuttons.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/cframe.h"
#include <math.h>

namespace VSTGUI {



//------------------------------------------------------------------------
// CKickButton
//------------------------------------------------------------------------
/*! @class CKickButton
Define a button with 2 states using 2 subbitmaps.
One click on it, then the second subbitmap is displayed.
When the mouse button is relaxed, the first subbitmap is framed.
*/
//------------------------------------------------------------------------
/**
 * CKickButton constructor.
 * @param size the size of this view
 * @param listener the listener
 * @param tag the control tag
 * @param background the bitmap
 * @param offset unused
 */
//------------------------------------------------------------------------
CVinylKickButton::CVinylKickButton (const CRect& size, IControlListener *listener, int32_t tag, CBitmap* background, CBitmap * activeBitmap, const CPoint& offset)
: CControl (size, listener, tag, background)
, offset (offset)
, pActive (activeBitmap)
, popupMenu (0)
, activated (false)
{
    if (pActive) {
		pActive->remember ();
    }
    heightOfOneImage = size.getHeight();
	setWantsFocus (true);
}

//------------------------------------------------------------------------
/**
 * CKickButton constructor.
 * @param size the size of this view
 * @param listener the listener
 * @param tag the control tag
 * @param heightOfOneImage height of one sub bitmap in background
 * @param background the bitmap
 * @param offset of background
 */
//------------------------------------------------------------------------
//CVinylKickButton::CVinylKickButton (const CRect& size, CControlListener* listener, int32_t tag, CCoord heightOfOneImage, CBitmap* background, const CPoint& offset)
//: CControl (size, listener, tag, background)
//, offset (offset)
//{
//	setHeightOfOneImage (heightOfOneImage);
//	setWantsFocus (true);
//}

//------------------------------------------------------------------------
CVinylKickButton::CVinylKickButton (const CVinylKickButton& v)
: CControl (v)
, offset (v.offset)
, pActive (v.pActive)
, popupMenu (v.popupMenu)
, BeforePopup (0)
, activated (v.activated)
{
	if (pActive)
		pActive->remember ();
	setHeightOfOneImage (v.heightOfOneImage);
	setWantsFocus (true);
}

//------------------------------------------------------------------------
CVinylKickButton::~CVinylKickButton ()
{
    if (pActive) {
		pActive->forget ();
    }

}

//------------------------------------------------------------------------
void CVinylKickButton::draw (CDrawContext *pContext)
{
    CPoint where (offset);

	bounceValue ();

	if (value == getMax ())
        where.y += heightOfOneImage;

    if (activated && pActive) {
        pActive->draw(pContext, getViewSize(), where); // getVisibleViewSize???
    } else if (getDrawBackground()) {
        getDrawBackground()->draw(pContext, getVisibleViewSize(), where);
    }
	setDirty (false);
}

//------------------------------------------------------------------------
CMouseEventResult CVinylKickButton::onMouseDown (CPoint& where, const CButtonState& buttons)
{
    if (buttons & kLButton) {

        //mousedown = true;
        //fEntryState = value;
        beginEdit ();
        value = getMax ();
        valueChanged ();
        //if (isDirty ())
        invalid ();

    } else if ((buttons & kRButton) && (popupMenu || BeforePopup)) {
			//getFrame()->addView(popupMenu);
        if (!BeforePopup || (BeforePopup && BeforePopup(getListener(), this))) {
            if (popupMenu) {
                popupMenu->popup(this);
                return kMouseEventHandled;
            }
        }
        // if (!BeforePopup && popupMenu ) {
        //     popupMenu->popup(this);
        // }
			//getFrame()->removeView(popupMenu,false);

    }
    return onMouseMoved (where, buttons);
}

//------------------------------------------------------------------------
CMouseEventResult CVinylKickButton::onMouseUp (CPoint& where, const CButtonState& buttons)
{
	//if (value)
	//	valueChanged ();
	//value = getMin ();  // set button to UNSELECTED state
	//valueChanged ();
	//if (isDirty ())
	//	invalid ();
	//mousedown= false;
	value = getMin ();
	valueChanged ();
	invalid ();
	endEdit ();
	return kMouseEventHandled;
}

//------------------------------------------------------------------------
CMouseEventResult CVinylKickButton::onMouseMoved (CPoint& where, const CButtonState& buttons)
{
    if (buttons & kLButton) {
        if (where.x >= getViewSize().left && where.y >= getViewSize().top
            && where.x <= getViewSize().right && where.y <= getViewSize().bottom) { // getVisibleViewSize???
			value = getMax ();
        } else {
			value = getMin ();
        }
        if (isDirty ()) {
			invalid ();
        }
	}
	return kMouseEventHandled;
}

//------------------------------------------------------------------------
int32_t CVinylKickButton::onKeyDown (VstKeyCode& keyCode)
{
    if (keyCode.modifier == 0 && keyCode.virt == VKEY_RETURN) {
		beginEdit ();
		//mousedown= true;
		value = getMax ();
		//if (value == getMax ()) value = getMin (); else value = getMax ();
		invalid ();
		valueChanged ();
		return 1;
	}
	return -1;
}

//------------------------------------------------------------------------
int32_t CVinylKickButton::onKeyUp (VstKeyCode& keyCode)
{
	if (keyCode.modifier == 0 && keyCode.virt == VKEY_RETURN)
	{
		//mousedown= false;
		value = getMin ();
		valueChanged ();
		invalid ();
		endEdit ();
		return 1;
	}
	return -1;
}

//------------------------------------------------------------------------
/*bool CVinylKickButton::sizeToFit ()
{
	if (pBackground)
	{
		CRect vs (getViewSize ());
		vs.setHeight (heightOfOneImage);
		vs.setWidth (pBackground->getWidth ());
		setViewSize (vs, true);
		setMouseableArea (vs);
		return true;
	}
	return false;
}*/

//------------------------------------------------------------------------
bool CVinylKickButton::getActive() {return activated;}
void CVinylKickButton::setActive(bool act) {if (act != activated){ activated=act; invalid ();}}
void CVinylKickButton::switchActive() {activated = !activated; invalid ();}
void CVinylKickButton::setPopupMenu(CVinylPopupMenu * menu) {popupMenu = menu;}
CVinylPopupMenu * CVinylKickButton::getPopupMenu() {return popupMenu;}
//bool CVinylKickButton::getActive() {if (value == getMax ()) return true; else return false;}
//void CVinylKickButton::setActive(bool act) {if (act) value = getMax (); else value = getMin (); }
//void CVinylKickButton::switchActive() {if (value == getMax ()) value = getMin (); else value = getMax (); invalid ();}








//------------------------------------------------------------------------
// CCheckBox
//------------------------------------------------------------------------
//------------------------------------------------------------------------
CVinylCheckBox::CVinylCheckBox (const CRect& size, IControlListener *listener, int32_t tag, CBitmap* background, CBitmap * checkedBitmap, const CPoint& offset)
: CControl (size, listener, tag, background)
, offset (offset)
, pChecked (checkedBitmap)
{
	if (pChecked)
		pChecked->remember ();
	//heightOfOneImage = size.height ();
	setWantsFocus (true);
}

//------------------------------------------------------------------------
CVinylCheckBox::CVinylCheckBox (const CVinylCheckBox& v)
: CControl (v)
, offset (v.offset)
, pChecked (v.pChecked)
{
	if (pChecked)
		pChecked->remember ();
	//setheightOfOneImage(v.heightOfOneImage);
	setWantsFocus (true);
}

//------------------------------------------------------------------------
CVinylCheckBox::~CVinylCheckBox ()
{
	if (pChecked)
		pChecked->forget ();

}

//------------------------------------------------------------------------
void CVinylCheckBox::draw (CDrawContext *pContext)
{
    CPoint where (offset);

	bounceValue ();

	if (mousedown) {
        where.y -= 1;
		//where.h += 1;
	}

	if ((value == getMax ())&&(pChecked)) {
        pChecked->draw (pContext, getViewSize(), where); // getVisibleViewSize???
    } else if (getDrawBackground()) {
        getDrawBackground()->draw (pContext, getVisibleViewSize(), where);
    }
	setDirty (false);
}

//------------------------------------------------------------------------
CMouseEventResult CVinylCheckBox::onMouseDown (CPoint& where, const CButtonState& buttons)
{
	if (!(buttons & kLButton))
		return kMouseEventNotHandled;
	//fEntryState = value;
	mousedown = true;
	invalid ();
	beginEdit ();
	return onMouseMoved (where, buttons);
}

//------------------------------------------------------------------------
CMouseEventResult CVinylCheckBox::onMouseUp (CPoint& where, const CButtonState& buttons)
{
	//if (value)
	//	valueChanged ();
	mousedown = false;
	switchChecked(); // set button to UNSELECTED state
	valueChanged ();
	if (isDirty ())
		invalid ();
	endEdit ();
	return kMouseEventHandled;
}

//------------------------------------------------------------------------
CMouseEventResult CVinylCheckBox::onMouseMoved (CPoint& where, const CButtonState& buttons)
{
	if (buttons & kLButton)
	{
        if (where.x >= getViewSize().left && where.y >= getViewSize().top
            && where.x <= getViewSize().right && where.y <= getViewSize().bottom) {
			mousedown = true;
        } else {
			mousedown = false;
        }
        if (isDirty ()) {
			invalid ();
        }
	}
	return kMouseEventHandled;
}

//------------------------------------------------------------------------
int32_t CVinylCheckBox::onKeyDown (VstKeyCode& keyCode)
{
	if (keyCode.modifier == 0 && keyCode.virt == VKEY_RETURN)
	{
		beginEdit ();
		switchChecked();
		valueChanged ();
 		return 1;
	}
	return -1;
}

//------------------------------------------------------------------------
int32_t CVinylCheckBox::onKeyUp (VstKeyCode& keyCode)
{
	if (keyCode.modifier == 0 && keyCode.virt == VKEY_RETURN)
	{
	    invalid ();
		endEdit ();
		return 1;
	}
	return -1;
}

//------------------------------------------------------------------------
/*bool CVinylCheckBox::sizeToFit ()
{
	if (pBackground)
	{
		CRect vs (getViewSize ());
		vs.setHeight (heightOfOneImage);
		vs.setWidth (pBackground->getWidth ());
		setViewSize (vs, true);
		setMouseableArea (vs);
		return true;
	}
	return false;
} */

//------------------------------------------------------------------------
bool CVinylCheckBox::getChecked() {if (value == getMax ()) return true; else return false;}
void CVinylCheckBox::setChecked(bool chk) {if (chk) value = getMax (); else value = getMin (); setDirty(true);}
void CVinylCheckBox::switchChecked() {if (value == getMax ()) value = getMin (); else value = getMax (); invalid (); setDirty(true);}


//------------------------------------------------------------------------
// C3PositionSwitchBox
//------------------------------------------------------------------------
//------------------------------------------------------------------------
C3PositionSwitchBox::C3PositionSwitchBox (const CRect& size,
                                         IControlListener *listener,
                                         int32_t tag,
                                         CBitmap* position1,
                                         CBitmap * position2,
                                         CBitmap * position3,
                                         const CPoint& offset)
: CControl (size, listener, tag, position1)
, offset (offset)
, pPosition2 (position2)
, pPosition3 (position3)
{
	if (pPosition2)
		pPosition2->remember ();
	if (pPosition3)
		pPosition3->remember ();
	//heightOfOneImage = size.height ();
	setWantsFocus (true);
}

//------------------------------------------------------------------------
C3PositionSwitchBox::C3PositionSwitchBox (const C3PositionSwitchBox& v)
: CControl (v)
, offset (v.offset)
, pPosition2 (v.pPosition2)
, pPosition3 (v.pPosition3)
{
	if (pPosition2)
		pPosition2->remember ();
	if (pPosition3)
		pPosition3->remember ();
	//setheightOfOneImage(v.heightOfOneImage);
	setWantsFocus (true);
}

//------------------------------------------------------------------------
C3PositionSwitchBox::~C3PositionSwitchBox ()
{
	if (pPosition2)
		pPosition2->forget ();
	if (pPosition3)
		pPosition3->forget ();

}

//------------------------------------------------------------------------
void C3PositionSwitchBox::draw (CDrawContext *pContext)
{
    CPoint where (offset);

	bounceValue ();

	if (mousedown) {
        where.y -= 1;
		//where.h += 1;
	}

	if (value == getMin ()) {
        if (getDrawBackground()) {
            getDrawBackground()->draw (pContext, getVisibleViewSize(), where);
        }
    } else if (value == getMax ()) {
        if (pPosition3) pPosition3->draw(pContext, getVisibleViewSize(), where);
    } else if (pPosition2) {
        pPosition2->draw(pContext, getVisibleViewSize(), where);
	}
	setDirty (false);
}

//------------------------------------------------------------------------
CMouseEventResult C3PositionSwitchBox::onMouseDown (CPoint& where, const CButtonState& buttons)
{
	if (!(buttons & kLButton))
		return kMouseEventNotHandled;
	//fEntryState = value;
	mousedown = true;
	invalid ();
	beginEdit ();
	return onMouseMoved (where, buttons);
}

//------------------------------------------------------------------------
CMouseEventResult C3PositionSwitchBox::onMouseUp (CPoint& where, const CButtonState& buttons)
{
	//if (value)
	//	valueChanged ();
	mousedown = false;
	switchPosition(); // set button to UNSELECTED state
	valueChanged ();
	if (isDirty ())
		invalid ();
	endEdit ();
	return kMouseEventHandled;
}

//------------------------------------------------------------------------
CMouseEventResult C3PositionSwitchBox::onMouseMoved (CPoint& where, const CButtonState& buttons)
{
	if (buttons & kLButton)
	{
        if (where.x >= getViewSize().left && where.y >= getViewSize().top  &&
            where.x <= getViewSize().right && where.y <= getViewSize().bottom) {
			mousedown = true;
        } else {
			mousedown = false;
        }
        if (isDirty ()) {
			invalid ();
        }
	}
	return kMouseEventHandled;
}

//------------------------------------------------------------------------
int32_t C3PositionSwitchBox::onKeyDown (VstKeyCode& keyCode)
{
	if (keyCode.modifier == 0 && keyCode.virt == VKEY_RETURN)
	{
		beginEdit ();
		switchPosition();
		valueChanged ();
 		return 1;
	}
	return -1;
}

//------------------------------------------------------------------------
int32_t C3PositionSwitchBox::onKeyUp (VstKeyCode& keyCode)
{
	if (keyCode.modifier == 0 && keyCode.virt == VKEY_RETURN)
	{
	    invalid ();
		endEdit ();
		return 1;
	}
	return -1;
}

//------------------------------------------------------------------------
/*bool CVinylCheckBox::sizeToFit ()
{
	if (pBackground)
	{
		CRect vs (getViewSize ());
		vs.setHeight (heightOfOneImage);
		vs.setWidth (pBackground->getWidth ());
		setViewSize (vs, true);
		setMouseableArea (vs);
		return true;
	}
	return false;
} */

//------------------------------------------------------------------------
int32_t C3PositionSwitchBox::getPosition() {
	if (value == getMin ()) return 0;
	else if (value == getMax ()) return 2;
	else return 1;
}
void C3PositionSwitchBox::setPosition(int32_t position) {
	switch (position){
		case -1: value = getMax (); break;
		case 1: value = (getMin () + getMax ())/2; break;
		case 2: value = getMax (); break;
		default: value = getMin ();
	}
	setDirty(true);
}
void C3PositionSwitchBox::switchPosition()
{
	setPosition(getPosition()+1);
}


//------------------------------------------------------------------------
// C3PositionSwitchBox
//------------------------------------------------------------------------
//------------------------------------------------------------------------
C5PositionView::C5PositionView (const CRect& size,
                               IControlListener *listener,
                               int32_t tag,
                               CBitmap* position1,
                               CBitmap * position2,
                               CBitmap * position3,
                               CBitmap * position4,
                               CBitmap * position5,
                               const CPoint& offset)
: CControl (size, listener, tag, position1)
, offset (offset)
, pPosition2 (position2)
, pPosition3 (position3)
, pPosition4 (position4)
, pPosition5 (position5)
{
	if (pPosition2)
		pPosition2->remember ();
	if (pPosition3)
		pPosition3->remember ();
	if (pPosition4)
		pPosition4->remember ();
	if (pPosition5)
		pPosition5->remember ();
	//heightOfOneImage = size.height ();
	setWantsFocus (true);
}

//------------------------------------------------------------------------
C5PositionView::C5PositionView (const C5PositionView& v)
: CControl (v)
, offset (v.offset)
, pPosition2 (v.pPosition2)
, pPosition3 (v.pPosition3)
, pPosition4 (v.pPosition4)
, pPosition5 (v.pPosition5)
{
	if (pPosition2)
		pPosition2->remember ();
	if (pPosition3)
		pPosition3->remember ();
	if (pPosition4)
		pPosition4->remember ();
	if (pPosition5)
		pPosition5->remember ();
	//setheightOfOneImage(v.heightOfOneImage);
	setWantsFocus (true);
}

//------------------------------------------------------------------------
C5PositionView::~C5PositionView ()
{
	if (pPosition2)
		pPosition2->forget ();
	if (pPosition3)
		pPosition3->forget ();
	if (pPosition4)
		pPosition4->forget ();
	if (pPosition5)
		pPosition5->forget ();

}

//------------------------------------------------------------------------
void C5PositionView::draw (CDrawContext *pContext)
{
    CPoint where (offset);

	bounceValue ();
	float _step = 0.125f * (getMax () - getMin ());
	if (value == getMin ()) {
        if (getDrawBackground()) {
            getDrawBackground()->draw (pContext, getVisibleViewSize(), where);
        }
    } else if (value == getMax ()) {
        if (pPosition5) pPosition5->draw (pContext, getVisibleViewSize(), where);
    } else if (value < _step * 3.0f) {
        if (pPosition2) pPosition2->draw (pContext, getVisibleViewSize(), where);
    } else if (value < _step * 5.0f) {
        if (pPosition3) pPosition3->draw (pContext, getVisibleViewSize(), where);
    } else if (value < _step * 7.0f) {
        if (pPosition4) pPosition4->draw (pContext, getVisibleViewSize(), where);
	}
	setDirty (false);
}

//------------------------------------------------------------------------
int32_t C5PositionView::getPosition() {
	float _step = 0.125f * (getMax () - getMin ());
	if (value == getMin ()) return 0;
	else if (value == getMax ()) return 4;
	else if (value < _step * 3.0f) return 1;
	else if (value < _step * 5.0f) return 2;
	else if (value < _step * 6.0f) return 3;
	else return 0;
}
void C5PositionView::setPosition(int32_t position) {
	float _step = 0.125f * (getMax () - getMin ());
	switch (position){
		case -1: value = getMax (); break;
		case 1: value = getMin () + _step * 2.0f; break;
		case 2: value = getMin () + _step * 4.0f; break;
		case 3: value = getMin () + _step * 6.0f; break;
		case 4: value = getMax (); break;
		default: value = getMin ();
	}
	setDirty(true);
}
void C5PositionView::switchPosition()
{
	setPosition(getPosition()+1);
}



} // namespace
