#pragma once

#include <math.h>
#include "effect.h"
#include "../helpers/filtred.h"

namespace Steinberg::Vst {

class Distortion: public Effect {
public:

    Distortion()
        : active_(false)
    {}

    ~Distortion() override
    {}

    void process(double &outL, double &outR, double &/*speed*/, double &/*tempo*/, double &/*volume*/) override {
        volume_.append(active_ ? 1. : 0.);
        if (volume_ > .0001) {
            if (outL > 0) {
                outL = outL * (1. - volume_ * .6) + .4 * sqrt(outL) * volume_;
            } else {
                outL = outL * (1. - volume_ * .6) - .3 * sqrt(-outL) * volume_;
            }
            if (outR > 0) {
                outR = outR * (1. - volume_ * .6) + .4 * sqrt(outR) * volume_;
            } else {
                outR = outR * (1. - volume_ * .6) - .3 * sqrt(-outR) * volume_;
            }
        }
    }

    void activate() override {
        active_ = true;
    }

    void disactivate() override {
        active_ = false;
    }

    Type type() const noexcept override {
        return Effect::Distorsion;
    }

private:

    Filtred<float, 8> volume_;
    bool active_;
};

}
