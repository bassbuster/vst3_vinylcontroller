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

#include "cwaveview.h"

#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cframe.h"


namespace VSTGUI {

//------------------------------------------------------------------------
CWaveView::CWaveView (const CRect &size, IControlListener *listener, int32_t tag, CBitmap *_pBackground, CBitmap *_pWave)
:CControl(size,listener,tag,_pBackground)
,pWave(_pWave)
{
	pixel_preroll = 100;
	if (pWave)
		pWave->remember ();
	setWantsFocus (true);
}

CWaveView::CWaveView (const CWaveView &v)
: CControl (v)
,pWave(v.pWave)
,pixel_preroll(v.pixel_preroll)

{
	if (pWave)
		pWave->remember ();
	setWantsFocus (true);
}

CWaveView::~CWaveView ()
{
	if (pWave)
		pWave->forget ();
}


//------------------------------------------------------------------------
void CWaveView::setWave(CBitmap *newWave)
{
	if (pWave)
		pWave->forget ();
	pWave = newWave;
	if (pWave)
		pWave->remember ();
	setDirty (true);
}

//------------------------------------------------------------------------
void CWaveView::setPosition(float _position)
{
	if (pWave) {
		if (_position != value) {
			value = _position;
			setDirty (true);
		}
	}
}

//------------------------------------------------------------------------
void CWaveView::draw (CDrawContext* pContext)
{
    for (int i = 0; i < getVisibleViewSize().getWidth() / 2; i++) {
        pContext->drawPoint(CPoint(getVisibleViewSize().left+i*2,getVisibleViewSize().top+61),CColor(0,255,0,120));
	}
	if (pWave) {
		int position = ((int)((pWave->getWidth() * getValueNormalized())/2))*2;


        pWave->draw(pContext,getVisibleViewSize(),CPoint(position-pixel_preroll,0));
        if ((position<pixel_preroll) && Loop) {
            pWave->draw(pContext,getVisibleViewSize(),CPoint(position + pWave->getWidth()-pixel_preroll,0));
		}else if (Loop) {
            pWave->draw(pContext,getVisibleViewSize(),CPoint(position - pWave->getWidth()-pixel_preroll,0));
		}

	}
    if (getDrawBackground()) {
        getDrawBackground()->draw(pContext,getVisibleViewSize(),CPoint(0,0));
	}
}

CMouseEventResult CWaveView::onMouseDown (CPoint& where, const CButtonState& buttons)
{
	valueChanged ();
	invalid();
	return kMouseEventHandled;
}

CMouseEventResult CWaveView::onMouseUp (CPoint& where, const CButtonState& buttons)
{
	return kMouseEventHandled;
}

CMouseEventResult CWaveView::onMouseMoved (CPoint& where, const CButtonState& buttons)
{
	return kMouseEventHandled;
}

void CWaveView::setLoop(bool _loop)
{
	if (_loop != Loop) {
		Loop  = _loop;
		invalid();
	}
}



} // namespace
