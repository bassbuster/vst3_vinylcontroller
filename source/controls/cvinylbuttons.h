//-----------------------------------------------------------------------------
// VST Plug-Ins SDK
// VSTGUI: Graphical User Interface Framework for VST plugins : 
//
// Version 4.0
//
//-----------------------------------------------------------------------------
// VSTGUI LICENSE
// (c) 2010, Steinberg Media Technologies, All Rights Reserved
//-----------------------------------------------------------------------------
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
// 
//   * Redistributions of source code must retain the above copyright notice, 
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation 
//     and/or other materials provided with the distribution.
//   * Neither the name of the Steinberg Media Technologies nor the names of its
//     contributors may be used to endorse or promote products derived from this 
//     software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A  PARTICULAR PURPOSE ARE DISCLAIMED. 
// IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
// OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE  OF THIS SOFTWARE, EVEN IF ADVISED
// OF THE POSSIBILITY OF SUCH DAMAGE.
//-----------------------------------------------------------------------------

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
