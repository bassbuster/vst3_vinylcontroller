#pragma once

#include <math.h>
#include "effect.h"
#include "../helpers/filtred.h"
#include "../helpers/sampleentry.h"

namespace Steinberg::Vst {

template<typename SampleGetter>
class Hold: public Effect {
public:

    Hold(double &sampleRate, size_t &noteLength, SampleGetter &&sampler)
        : active_(false)
        , sampler_(std::forward<SampleGetter>(sampler))
        , sampleRate_(sampleRate)
        , noteLength_(noteLength)
        , holdCounter_(0)
    {
    }

    ~Hold() override
    {};

    void process(double &outL, double &outR, double &speed, double &tempo, double &/*volume*/) override {
        volume_.append(active_ ? 1. : 0.);
        if (active_) {
            holdCounter_++;
            auto samplePtr = sampler_();
            if ((volume_ > 0.00001) && (volume_ < 0.99)) {
                double left = 0;
                double right = 0;
                const auto& PushCue = samplePtr->cue();
                samplePtr->cue(endHoldCue_);
                samplePtr->playStereoSample(&left,
                    &right,
                    speed,
                    samplePtr->tempo(),
                    sampleRate_,
                    true);
                outL = outL * volume_ + left * (1. - volume_);
                outR = outR * volume_ + right * (1. - volume_);
                endHoldCue_ = samplePtr->cue();
                samplePtr->cue(PushCue);
            }

            if (holdCounter_ >= noteLength_) {
                endHoldCue_ = samplePtr->cue();
                samplePtr->cue(holdCue_);
                holdCounter_ = 0;
                volume_ = 0;
            }
        }
    }

    void activate() override {
        active_ = true;
        holdCue_ = sampler_()->cue();
    }

    void disactivate() override {
        active_ = false;
    }

    Type type() const noexcept override {
        return Effect::Hold;
    }

private:

    Filtred<float, 8> volume_;
    bool active_;
    SampleGetter sampler_;
    double &sampleRate_;
    size_t &noteLength_;
    size_t holdCounter_;
    SampleEntry<double>::CuePoint holdCue_;
    SampleEntry<double>::CuePoint endHoldCue_;


};

}
