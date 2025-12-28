#pragma once

#include <math.h>
#include "effect.h"
#include "../helpers/filtred.h"
#include "../helpers/sampleentry.h"

namespace Steinberg::Vst {

template<typename SampleGetter>
class Freeze: public Effect {
public:

    Freeze(double &sampleRate, size_t &noteLength, SampleGetter &&sampler)
        : active_(false)
        , sampler_(std::forward<SampleGetter>(sampler))
        , sampleRate_(sampleRate)
        , noteLength_(noteLength)
        , freezeCounter_(0)
    {
    }

    ~Freeze() override
    {};

    void process(double &outL, double &outR, double &speed, double &tempo, double &volume) override {
        volume_.append(active_ ? 1. : 0.);
        if (active_) {
            freezeCounter_++;
            auto sample = sampler_();
            bool pushSync = sample->Sync;
            sample->Sync = false;
            auto PushCue = sample->cue();
            sample->cue(freezeCue_);
            sample->playStereoSample(&outL,
                &outR,
                speed,
                tempo,
                sampleRate_,
                true);
            freezeCue_ = sample->cue();
            if ((volume_ > 0.00001) && (volume_ < 0.99)) {
                double left = 0;
                double right = 0;
                sample->cue(endFreezeCue_);
                sample->playStereoSample(&left,
                    &right,
                    speed,
                    tempo,
                    sampleRate_,
                    true);
                outL = outL * volume_ + left * (1. - volume_);
                outR = outR * volume_ + right * (1. - volume_);
                endFreezeCue_ = sample->cue();
            }

            if (speed != 0.0) {
                if (freezeCounter_ >= noteLength_) {
                    freezeCounter_ = 0;
                    endFreezeCue_ = freezeCue_;
                    freezeCue_ = beginFreezeCue_;
                    volume_ = 0;
                }
            }
            sample->Sync = pushSync;
            sample->cue(PushCue);
        }
    }

    void activate() override {
        active_ = true;
        beginFreezeCue_ = sampler_()->cue();
        freezeCue_ = beginFreezeCue_;
    }

    void disactivate() override {
        active_ = false;
        endFreezeCue_ = freezeCue_;
    }

    Type type() const noexcept override {
        return Effect::Freeze;
    }

private:

    Filtred<float, 8> volume_;
    bool active_;

    SampleGetter sampler_;

    double &sampleRate_;
    size_t &noteLength_;

    size_t freezeCounter_;
    SampleEntry<double>::CuePoint beginFreezeCue_;
    SampleEntry<double>::CuePoint endFreezeCue_;
    SampleEntry<double>::CuePoint freezeCue_;


};

}
