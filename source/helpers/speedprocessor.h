#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "filtred.h"
#include "ringbuffer.h"
#include "fft.h"

namespace Steinberg::Vst {

template<typename SampleType, size_t SpeedFrame = 128, size_t SpectrumFame = 512, size_t PreFilterFrame = 80>
class SpeedProcessor {
public:

    SpeedProcessor()
        : speedFrameIndex_(0)
        , oldSignalLeft_(0)
        , oldSignalRight_(0)
        , directionBits_(0)
        , direction_(1.)
        , speedCounter_(0)
        , stateRight_(0)
        , stateLeft_(0)
        , prevStateRight_(0)
        , prevStateLeft_(0)
        , timecode_(ETimeCodeCoeff)
        , realSpeed_(0)
        , timecodeLearnCounter_(0)
    {}

    SampleType volume() const noexcept {
        return volume_;
    }

    SampleType direction() const noexcept {
        return direction_;
    }

    SampleType realSpeed() const noexcept {
        return realSpeed_;
    }

    SampleType timecode() const noexcept {
        return timecode_;
    }

    bool isLearning() const noexcept {
        return timecodeLearnCounter_ > 0;
    }

    void startLearn() noexcept {
        timecodeLearnCounter_ = ETimecodeLearnCount;
    }

    void volume(SampleType vol) noexcept {
        volume_ = vol;
    }

    void timecode(SampleType tc) noexcept {
        timecode_ = tc;
    }

#ifdef DEVELOPMENT
    template<typename DebugInput, typename DebugOutput>
    void process(SampleType inL,
                 SampleType inR,
                 const DebugInput& debugInput,
                 const DebugOutput& debugOutput)
#else
    void process(SampleType inL, SampleType inR)
#endif // DEBUG
    {

        filterBufferLeft_.writeHead(filtredHiLeft_.append(inL));
        filterBufferRight_.writeHead(filtredHiRight_.append(inR));

        SampleType newSignalLeft = filterBufferLeft_.readTail() - filtredLoLeft_.append(inL);
        SampleType newSignalRight = filterBufferRight_.readTail() - filtredLoRight_.append(inR);

        deltaLeft_.append(newSignalLeft - oldSignalLeft_);
        deltaRight_.append(newSignalRight - oldSignalRight_);

        calcDirectionTimeCodeAmplitude();

        oldSignalLeft_ = newSignalLeft;
        oldSignalRight_ = newSignalRight;

        SampleType smoothWindow = sin(Pi * SampleType(speedFrameIndex_ + SpectrumFame - SpeedFrame) / SampleType(SpectrumFame));
        originalBuffer_[speedFrameIndex_ + SpectrumFame - SpeedFrame] = oldSignalLeft_ - oldSignalRight_;
        fftBuffer_[speedFrameIndex_ + SpectrumFame - SpeedFrame] = (smoothWindow * originalBuffer_[speedFrameIndex_ + SpectrumFame - SpeedFrame]);

        speedFrameIndex_++;
        if (speedFrameIndex_ >= SpeedFrame) {
            speedFrameIndex_ = 0;

            if (timeCodeAmplytude_ >= ETimeCodeMinAmplytude) {

#ifdef DEVELOPMENT
                debugInput(fftBuffer_.data(), SpectrumFame);
#endif // DEBUG
                fastsine(fftBuffer_.data(), SpectrumFame);

                for (size_t i = 0; i < 10; i++) {
                    SampleType SmoothCoef =  i * .1 + .01;
                    fftBuffer_[i] = SmoothCoef * fftBuffer_[i];
                }

                calcAbsSpeed();

#ifdef DEVELOPMENT
                debugOutput(fftBuffer_.data(), SpectrumFame);
#endif // DEBUG

                if (timecodeLearnCounter_ > 0) {
                    timecodeLearnCounter_--;
                    timecode_.append(direction_ * absAvgSpeed_);
                    realSpeed_ = 1.;
                } else {
                    realSpeed_ = absAvgSpeed_ / timecode_;
                }
                volume_.append(sqrt(fabs(realSpeed_)));
                realSpeed_ = direction_ * realSpeed_;
            }

            for (size_t i = 0; i < SpectrumFame - SpeedFrame; i++) {
                SampleType smoothWindow = sin(Pi * SampleType(i) / SampleType(SpectrumFame));
                originalBuffer_[i] = originalBuffer_[SpeedFrame + i];
                fftBuffer_[i] = (smoothWindow * originalBuffer_[SpeedFrame + i]);
            }
        }

        if (timeCodeAmplytude_ < ETimeCodeMinAmplytude) {
            if (volume_ >= 0.00001) {
                absAvgSpeed_ = absAvgSpeed_ / 1.07;
                realSpeed_ = direction_ * absAvgSpeed_ / timecode_;
                volume_.append(sqrt(fabs(realSpeed_)));
            } else {
                absAvgSpeed_ = 0.;
                realSpeed_ = 0.;
                volume_ = 0.;
            }
        }
    }

private:

    static constexpr SampleType ETimeCodeMinAmplytude = 0.009;
    static constexpr SampleType ETimeCodeCoeff = 22.9;
    static constexpr size_t ETimecodeLearnCount = 1024;

    void calcAbsSpeed()
    {
        size_t maxX = 0;
        SampleType maxY = 0.;

        for (size_t i = 0; i < SpectrumFame; ++i) {
            if (maxY < fabs(fftBuffer_[i])) {
                maxY = fabs(fftBuffer_[i]);
                maxX = i;
            }
        }

        SampleType tmp = maxX;
        for (size_t i = maxX + 1, total = maxX + 3;
             i < total;
             ++i) {
            if (i < SpectrumFame) {
                SampleType koef = 100.;
                if (fftBuffer_[i] != 0) {
                    koef = (maxY / fftBuffer_[i]) * (maxY / fftBuffer_[i]);
                }
                tmp = (koef * tmp + SampleType(i)) / (koef + 1.);
                continue;
            }
            break;
        }

        for (int i = int(maxX) - 1, total = int(maxX) - 3;
             i > total;
             --i) {
            if (i >= 0) {
                SampleType koef = 100.;
                if (fftBuffer_[i] != 0) {
                    koef = (maxY / fftBuffer_[i]) * (maxY / fftBuffer_[i]);
                }
                tmp = (koef * tmp + SampleType(i)) / (koef + 1.);
                continue;
            }
            break;
        }

        if (fabs(tmp - absAvgSpeed_) > 0.7) {
            absAvgSpeed_ = tmp;
        } else {
            absAvgSpeed_.append(tmp);
        }
    }

    uint8_t reversed(uint8_t val) {
        return ((val << 4) & 0xF0) | ((val >> 4) & 0x0F);
    }

    void calcDirectionTimeCodeAmplitude()
    {
        if ((stateRight_ & 0x0F) > 0 && (deltaRight_ < 0.)) {
            stateRight_ <<= 4;
            stateRight_ &= 0xF0;
            timeCodeAmplytude_.append(fabs(oldSignalRight_));
        } else if ((stateRight_ & 0x0F) == 0 && (deltaRight_ > 0.)) {
            stateRight_ <<= 4;
            stateRight_ |= 0x0F;
            timeCodeAmplytude_.append(fabs(oldSignalRight_));
        }

        if ((stateLeft_ & 0x0F) > 0 && (deltaLeft_ < 0.)) {
            stateLeft_ <<= 4;
            stateLeft_ &= 0xF0;
            timeCodeAmplytude_.append(fabs(oldSignalLeft_));
        } else if ((stateLeft_ & 0x0F) == 0 && (deltaLeft_ > 0.)) {
            stateLeft_ <<= 4;
            stateLeft_ |= 0x0F;
            timeCodeAmplytude_.append(fabs(oldSignalLeft_));
        }

        if (timeCodeAmplytude_ >= ETimeCodeMinAmplytude) {

            if ((stateRight_ != prevStateRight_) || (stateLeft_ != prevStateLeft_)) {

                if ((stateLeft_ == prevStateRight_)
                    && (stateRight_ == reversed(prevStateLeft_)) 
                    && (speedCounter_ >= 3)) {

                    directionBits_ <<= 8;
                    directionBits_ |= 0xff;
                    speedCounter_ = 0;

                } else if ((stateLeft_ == reversed(prevStateRight_))  
                           && (stateRight_ == prevStateLeft_)
                           && (speedCounter_ >= 3)) {

                    directionBits_ <<= 8;
                    directionBits_ &= 0xffffff00;
                    speedCounter_ = 0;
                }

                prevStateRight_ = stateRight_;
                prevStateLeft_  = stateLeft_;
            }

            if (directionBits_ == 0) {
                direction_ = -1.;
            } else if (directionBits_ == 0xffffffff) {
                direction_ = 1.;
            }

            if (speedCounter_ < 441000) {
                speedCounter_++;
            }
        }
    }

    static constexpr SampleType Pi = 3.14159265358979323846264338327950288;

    RingBuffer<SampleType, PreFilterFrame> filterBufferLeft_;
    RingBuffer<SampleType, PreFilterFrame> filterBufferRight_;

    Filtred<SampleType> filtredHiLeft_;
    Filtred<SampleType> filtredHiRight_;
    Filtred<SampleType, PreFilterFrame> filtredLoLeft_;
    Filtred<SampleType, PreFilterFrame> filtredLoRight_;

    Filtred<SampleType> deltaLeft_;
    Filtred<SampleType> deltaRight_;

    Filtred<SampleType, 64> timeCodeAmplytude_;

    std::array<SampleType, SpectrumFame> originalBuffer_;
    std::array<SampleType, SpectrumFame> fftBuffer_;

    SampleType oldSignalLeft_;
    SampleType oldSignalRight_;

    size_t speedFrameIndex_;
    uint32_t directionBits_;
    SampleType direction_;

    size_t speedCounter_;

    uint8_t stateRight_;
    uint8_t stateLeft_;
    uint8_t prevStateRight_;
    uint8_t prevStateLeft_;

    Filtred<SampleType, 16> volume_;
    Filtred<SampleType, 256> timecode_;

    SampleType realSpeed_;
    Filtred<SampleType, 10> absAvgSpeed_;

    size_t timecodeLearnCounter_;

};

}
