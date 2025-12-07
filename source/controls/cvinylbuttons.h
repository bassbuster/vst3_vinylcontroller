#pragma once

#include "vstgui/lib/controls/ccontrol.h"
#include "cvinylpopupmenu.h"
#include "vstgui/lib/cfont.h"
#include "vstgui/lib/ccolor.h"

namespace VSTGUI {

//typedef bool ( *CControlEvent)(CControl*);

//-----------------------------------------------------------------------------
// CCheckBox Declaration
//-----------------------------------------------------------------------------
class CVinylCheckBox : public CControl, public IMultiBitmapControl
{
public:
    CVinylCheckBox (const CRect& size,
                   IControlListener* listener,
                   int32_t tag,
                   CBitmap* background,
                   CBitmap* checkedBitmap,
                   const CPoint& offset = CPoint (0, 0));
	CVinylCheckBox (const CVinylCheckBox& kickButton);

    void draw (CDrawContext*) override;

    CMouseEventResult onMouseDown (CPoint& where, const CButtonState& buttons) override;
    CMouseEventResult onMouseUp (CPoint& where, const CButtonState& buttons) override;
    CMouseEventResult onMouseMoved (CPoint& where, const CButtonState& buttons) override;
    int32_t onKeyDown (VstKeyCode& keyCode) override;
    int32_t onKeyUp (VstKeyCode& keyCode) override;

	bool getChecked() ;
	void setChecked(bool chk) ;
	void switchChecked() ;

	CLASS_METHODS(CVinylCheckBox, CControl)
protected:
	~CVinylCheckBox ();
	CPoint	offset;
	CBitmap* pChecked;

private:
	//float   fEntryState;
	bool mousedown;

};

//-----------------------------------------------------------------------------
// CKickButton Declaration
//!
/// @ingroup controls
//-----------------------------------------------------------------------------
class CVinylKickButton : public CControl, public IMultiBitmapControl
{
public:
    CVinylKickButton (const CRect& size,
                     IControlListener* listener,
                     int32_t tag,
                     CBitmap* background,
                     CBitmap* activeBitmap,
                     const CPoint& offset = CPoint (0, 0));
	//CVinylKickButton (const CRect& size, CControlListener* listener, int32_t tag, CCoord heightOfOneImage, CBitmap* background, const CPoint& offset = CPoint (0, 0));
	CVinylKickButton (const CVinylKickButton& kickButton);

    void draw (CDrawContext*) override;

    CMouseEventResult onMouseDown (CPoint& where, const CButtonState& buttons) override;
    CMouseEventResult onMouseUp (CPoint& where, const CButtonState& buttons) override;
    CMouseEventResult onMouseMoved (CPoint& where, const CButtonState& buttons) override;
    int32_t onKeyDown (VstKeyCode& keyCode) override;
    int32_t onKeyUp (VstKeyCode& keyCode) override;

	//virtual bool sizeToFit ();

	//void setNumSubPixmaps (int32_t numSubPixmaps) { IMultiBitmapControl::setNumSubPixmaps (numSubPixmaps); invalid (); }
	bool getActive() ;
	void setActive(bool act) ;
	void switchActive() ;
	void setPopupMenu(CVinylPopupMenu * menu);
    bool (* BeforePopup)(IControlListener*, CControl *);
    //CControlEvent BeforePopup;
    CVinylPopupMenu * getPopupMenu();

	CLASS_METHODS(CVinylKickButton, CControl)
protected:
	~CVinylKickButton ();
	CPoint	offset;
	CBitmap* pActive;
	CVinylPopupMenu * popupMenu;

private:
	//float   fEntryState;
	//bool mousedown;
	bool activated;

};

//////////////////////////////////////////////////////
class C5PositionView : public CControl, public IMultiBitmapControl
{
public:
    C5PositionView (const CRect& size,
                   IControlListener* listener,
                   int32_t tag,
                   CBitmap* position1,
                   CBitmap* position2,
                   CBitmap* position3,
                   CBitmap* position4,
                   CBitmap* position5,
                   const CPoint& offset = CPoint (0, 0));

	C5PositionView (const C5PositionView& _oldview);

    void draw (CDrawContext*) override;

	//virtual CMouseEventResult onMouseDown (CPoint& where, const CButtonState& buttons);
	//virtual CMouseEventResult onMouseUp (CPoint& where, const CButtonState& buttons);
	//virtual CMouseEventResult onMouseMoved (CPoint& where, const CButtonState& buttons);
	//virtual int32_t onKeyDown (VstKeyCode& keyCode);
	//virtual int32_t onKeyUp (VstKeyCode& keyCode);


	int32_t getPosition() ;
	void setPosition(int32_t position) ;
	void switchPosition() ;

	CLASS_METHODS(C5PositionView, CControl)
protected:
	~C5PositionView ();
	CPoint	offset;
	CBitmap* pPosition2;
	CBitmap* pPosition3;
	CBitmap* pPosition4;
	CBitmap* pPosition5;

};

class C3PositionSwitchBox : public CControl, public IMultiBitmapControl
{
public:
    C3PositionSwitchBox (const CRect& size,
                        IControlListener* listener,
                        int32_t tag,
                        CBitmap* position1,
                        CBitmap* position2,
                        CBitmap* position3,
                        const CPoint& offset = CPoint (0, 0));

	C3PositionSwitchBox (const C3PositionSwitchBox& _swbox);

    void draw (CDrawContext*) override;

    CMouseEventResult onMouseDown (CPoint& where, const CButtonState& buttons) override;
    CMouseEventResult onMouseUp (CPoint& where, const CButtonState& buttons) override;
    CMouseEventResult onMouseMoved (CPoint& where, const CButtonState& buttons) override;
    int32_t onKeyDown (VstKeyCode& keyCode) override;
    int32_t onKeyUp (VstKeyCode& keyCode) override;


	int32_t getPosition() ;
	void setPosition(int32_t position) ;
	void switchPosition() ;

	CLASS_METHODS(C3PositionSwitchBox, CControl)
protected:
	~C3PositionSwitchBox ();
	CPoint	offset;
	CBitmap* pPosition2;
	CBitmap* pPosition3;

private:
	//float   fEntryState;
	bool mousedown;

};


} // namespace
