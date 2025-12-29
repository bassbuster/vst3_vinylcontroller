#pragma once

#include <math.h>
#include "effect.h"
#include "../helpers/filtred.h"

namespace Steinberg::Vst {

template<typename MaxValue>
class PunchIn: public Effect {
public:

    PunchIn(MaxValue &&maxValue)
        : active_(false)
        , maxValue_(std::forward<MaxValue>(maxValue))
    {}

    ~PunchIn() override
    {}

    void process(double &/*outL*/, double &/*outR*/, double &/*speed*/, double &/*tempo*/, double &volume) override {
        volume_.append(active_ ? maxValue_() : volume);
        volume = volume_;
    }

    void activate() override {
        active_ = true;
    }

    void disactivate() override {
        active_ = false;
    }

    Type type() const noexcept override {
        return Effect::PunchIn;
    }

private:

    Filtred<float, 8> volume_;
    bool active_;
    MaxValue maxValue_;
};

class PunchOut: public Effect {
public:

    PunchOut()
        : active_(false)
    {}

    ~PunchOut() override
    {}

    void process(double &/*outL*/, double &/*outR*/, double &/*speed*/, double &/*tempo*/, double &volume) override {
        volume_.append(active_ ? 0. : 1.);
        volume *= volume_;
    }

    void activate() override {
        active_ = true;
    }

    void disactivate() override {
        active_ = false;
    }

    Type type() const noexcept override {
        return Effect::PunchOut;
    }

private:

    Filtred<float, 8> volume_;
    bool active_;
};

}
