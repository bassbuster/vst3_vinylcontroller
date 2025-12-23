#pragma once


namespace Steinberg::Vst {

template<typename SampleType, int round = 4>
class Filtred {
public:

    Filtred(SampleType val = 0):
        value_(val)
    {}

    SampleType set(SampleType val) {
        value_ = (SampleType(round - 1) * value_ + val) / SampleType(round);
        return value_;
    }

    Filtred& append(SampleType val) {
        value_ = (SampleType(round - 1) * value_ + val) / SampleType(round);
        return *this;
    }

    operator SampleType() const {
        return value_;
    }

private:
    SampleType value_;
};

}
