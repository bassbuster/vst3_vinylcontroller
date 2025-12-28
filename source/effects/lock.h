#pragma once

#include <math.h>
#include "effect.h"
#include "../helpers/filtred.h"
#include "../helpers/sampleentry.h"

namespace Steinberg::Vst {

template<typename SampleGetter>
class Lock: public Effect {
public:

    Lock(double &sampleRate, SampleGetter &&sampler)
        : active_(false)
        , init_(false)
        , sampleRate_(sampleRate)
        , sampler_(std::forward<SampleGetter>(sampler))
    {
    }

    ~Lock() override
    {};

    void process(double &outL, double &outR, double &speed, double &tempo, double &volume) override {

        auto sample = sampler_();

        if (active_) {
            if (!init_) {
                lockSpeed_ = fabs(speed);
                lockVolume_ = volume;
                lockTune_ = sample->Tune;
                sample->beginLockStrobe();
                init_ = true;
            }
            else {
                volume = lockVolume_;
            }
            tempo = sample->Sync
                ? fabs(tempo * speed)
                : sample->tempo() * fabs(speed) * lockTune_;

            speed = (speed >= 0.) ? lockSpeed_ : -lockSpeed_;

            sample->playStereoSampleTempo(&outL,
                &outR,
                speed,
                tempo,
                sampleRate_,
                true);
        } else {

            sample->playStereoSample(&outL,
                &outR,
                speed,
                tempo,
                sampleRate_,
                true);
        }

    }

    void activate() override {
        active_ = true;
        init_ = false;
    }

    void disactivate() override {
        active_ = false;
    }

    Type type() const noexcept override {
        return Effect::LockTone;
    }

private:

    bool active_;
    bool init_;

    double &sampleRate_;
    SampleGetter sampler_;

    double lockSpeed_;
    double lockVolume_;
    double lockTune_;
};

}
