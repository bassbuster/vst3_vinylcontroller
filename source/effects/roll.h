#pragma once

#include <math.h>
#include "effect.h"
#include "../helpers/filtred.h"
#include "../helpers/sampleentry.h"

namespace Steinberg::Vst {

template<typename SampleGetter>
class PreRoll: public Effect {
public:

    PreRoll(SampleGetter &&sampler)
        : active_(false)
        , sampler_(std::forward<SampleGetter>(sampler))
    {
    }

    ~PreRoll() override
    {};

    void process(double &outL, double &outR, double &speed, double &tempo, double &/*volume*/) override {
        volume_.append(active_ ? 1. : 0.);

        auto sample = sampler_();
        double offset = sample->noteLength(rollNote_, tempo);
        double pre_offset = -offset;

        double left = 0;
        double right = 0;
        for (size_t i = 0; i < rollCount_; i++) {
            double rollVolume = (1. - double(i) / double(rollCount_)) * .6;
            if (volume_ > 0.0001) {
                sample->playStereoSample(&left, &right, pre_offset, false);
                outL = outL * (1. - volume_ * .1) + left * rollVolume * volume_;
                outR = outR * (1. - volume_ * .1) + right * rollVolume * volume_;
                pre_offset = pre_offset + offset;
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
        return Effect::PreRoll;
    }

private:

    static constexpr double rollNote_ {1./32.};
    static constexpr size_t rollCount_ {8};

    Filtred<float, 8> volume_;
    bool active_;

    SampleGetter sampler_;
};

template<typename SampleGetter>
class PostRoll: public Effect {
public:

    PostRoll(SampleGetter &&sampler)
        : active_(false)
        , sampler_(std::forward<SampleGetter>(sampler))
    {
    }

    ~PostRoll() override
    {};

    void process(double &outL, double &outR, double &speed, double &tempo, double &/*volume*/) override {
        volume_.append(active_ ? 1. : 0.);

        auto sample = sampler_();
        double offset = sample->noteLength(rollNote_, tempo);
        double pos_offset = offset;

        double left = 0;
        double right = 0;
        for (size_t i = 0; i < rollCount_; i++) {
            double rollVolume = (1. - double(i) / double(rollCount_)) * .6;
            if (volume_ > 0.0001) {
                sample->playStereoSample(&left, &right, pos_offset, false);
                outL = outL * (1. - volume_ * .1) + left * rollVolume * volume_;
                outR = outR * (1. - volume_ * .1) + right * rollVolume * volume_;
                pos_offset = pos_offset - offset;
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
        return Effect::PostRoll;
    }

private:

    static constexpr double rollNote_ {1./32.};
    static constexpr size_t rollCount_ {8};

    Filtred<float, 8> volume_;
    bool active_;

    SampleGetter sampler_;
};

}
