#pragma once

namespace Steinberg::Vst::Helper {

template<typename IntegerPart_, typename FloatPart_>
class CuePoint {
public:
    using IntegerType = IntegerPart_;
    using FloatType = FloatPart_;

    explicit CuePoint(IntegerType integerPart = 0, FloatType floatPart = 0.):
        integerPart_{integerPart},
        floatPart_{floatPart}
    {}

    CuePoint(const CuePoint &other) = default;
    CuePoint(CuePoint &&other) = default;
    CuePoint& operator=(const CuePoint &other) = default;
    CuePoint& operator=(CuePoint &&other) = default;

    void set(IntegerType integerPart, FloatType floatPart) {
        integerPart_ = integerPart;
        floatPart_ = floatPart;
    }

    CuePoint& operator = (FloatType offset) {
        integerPart_ = IntegerType(offset);
        floatPart_ = offset - FloatType(integerPart_);
        return *this;
    }

    CuePoint& operator = (IntegerType offset) {
        integerPart_ = offset;
        floatPart_ = 0.;
        return *this;
    }

    bool operator > (const CuePoint& cue) const {
        return ((integerPart_ > cue.integerPart_)
                || ((integerPart_ == cue.integerPart_) && (floatPart_ > cue.floatPart_)));
    }

    bool operator < (const CuePoint& cue) const {
        return ((integerPart_ < cue.integerPart_)
                || ((integerPart_ == cue.integerPart_) && (floatPart_ < cue.floatPart_)));
    }

    bool operator == (const CuePoint& cue) const {
        return ((integerPart_ == cue.integerPart_) && (floatPart_ == cue.floatPart_));
    }

    bool operator >= (const CuePoint& _cue) const {
        return ((*this > _cue) || (*this == _cue));
    }

    bool operator <= (const CuePoint& cue) const {
        return ((*this < cue) || (*this == cue));
    }

    CuePoint& operator += (double offset) {
        floatPart_ += offset;
        normalize();
        return *this;
    }

    CuePoint& operator -= (double offset) {
        floatPart_ -= offset;
        normalize();
        return *this;
    }

    friend CuePoint operator + (const CuePoint& cue, double offset) {
        CuePoint ret(cue);
        ret += offset;
        return ret;
    }

    friend CuePoint operator - (const CuePoint& cue, double offset) {
        CuePoint ret(cue);
        ret -= offset;
        return ret;
    }

    inline void normalize() {
        if (floatPart_ > 1.0) {
            integerPart_ += IntegerType(floatPart_);
            floatPart_ = floatPart_ - IntegerType(floatPart_);
        }
        if (floatPart_ < 0) {
            integerPart_ += (IntegerType(floatPart_) - 1);
            floatPart_ = 1. + (floatPart_ - IntegerType(floatPart_));
        }
    }

    double asDouble() {
        return double(integerPart_) + floatPart_;
    }

    void clear() {
        integerPart_ = 0;
        floatPart_ = 0.;
    }

    IntegerType integerPart() const {
        return integerPart_;
    }

    FloatType floatPart() const {
        return floatPart_;
    }

private:

    IntegerType integerPart_;
    FloatType floatPart_;
};

}
