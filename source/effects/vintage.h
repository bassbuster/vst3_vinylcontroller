#pragma once

#include <math.h>
#include "effect.h"
#include "../helpers/filtred.h"
#include "../helpers/sampleentry.h"
#include "../helpers/resourcepath.h"

namespace Steinberg::Vst {

class Vintage: public Effect {
public:

    Vintage(double &sampleRate)
        : active_(false)
        , vintageSample_("vintage", (getResourcePath() + "\\vintage.wav").c_str())
        , sampleRate_(sampleRate)
    {
        vintageSample_.Loop = true;
        vintageSample_.Sync = false;
        vintageSample_.Reverse = false;
    }

    ~Vintage() override
    {};

    void process(double &outL, double &outR, double &speed, double &tempo, double &/*volume*/) override {
        volume_.append(active_ ? 1. : 0.);
        if (volume_ > .0001) {
            double VintageLeft = 0;
            double VintageRight = 0;

            vintageSample_.playStereoSample(&VintageLeft, &VintageRight, speed, tempo, sampleRate_, true);

            outL = outL * (1. - volume_ * .3) + (-pow(outR * .5, 2) + VintageLeft) * volume_;
            outR = outR * (1. - volume_ * .3) + (-pow(outL * .5, 2) + VintageRight) * volume_;
        }
    }

    void activate() override {
        active_ = true;
    }

    void disactivate() override {
        active_ = false;
    }

    Type type() const noexcept override {
        return Effect::Vintage;
    }

private:

    Filtred<float, 8> volume_;
    bool active_;
    SampleEntry<double> vintageSample_;
    double &sampleRate_;
};

}
