#include "cwaveview.h"

#include "vstgui/lib/cbitmap.h"
#include "vstgui/lib/cdrawcontext.h"
//#include "vstgui/lib/cframe.h"


namespace VSTGUI {

//------------------------------------------------------------------------
CWaveView::CWaveView (const CRect &size, IControlListener *listener, int32_t tag, CBitmap *background)
    : CControl(size, listener, tag, background)
    , pixelPreroll_(100)
{
	setWantsFocus (true);
}

CWaveView::CWaveView (const CWaveView &v)
    : CControl (v)
    , wave_(v.wave_)
    , pixelPreroll_(v.pixelPreroll_)

{
    setWantsFocus (true);
}

CWaveView::~CWaveView ()
{

}


//------------------------------------------------------------------------
void CWaveView::setWave(const SharedPointer<CBitmap> &newWave)
{
    wave_ = newWave;
	setDirty (true);
}

//------------------------------------------------------------------------
void CWaveView::setPosition(float _position)
{
    if (wave_) {
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
        pContext->drawPoint(CPoint(getVisibleViewSize().left + i * 2,
                                   getVisibleViewSize().top + 61),
                            CColor(0, 255, 0, 120));
	}
    if (wave_) {
        int position = (int(wave_->getWidth() * getValueNormalized() / 2)) * 2;

        wave_->draw(pContext, getVisibleViewSize(), CPoint(position - pixelPreroll_, 0));
        if ((position < pixelPreroll_) && loop_) {
            wave_->draw(pContext, getVisibleViewSize(), CPoint(position + wave_->getWidth() - pixelPreroll_, 0));
        } else if (loop_) {
            wave_->draw(pContext, getVisibleViewSize(), CPoint(position - wave_->getWidth() - pixelPreroll_, 0));
		}

	}
    if (getDrawBackground()) {
        getDrawBackground()->draw(pContext, getVisibleViewSize());
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

void CWaveView::setLoop(bool loop)
{
    if (loop != loop_) {
        loop_  = loop;
		invalid();
	}
}



} // namespace
