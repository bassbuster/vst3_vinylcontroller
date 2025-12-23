#pragma once

#include <cstddef>
#include <utility>

namespace Steinberg {
namespace Vst {

template<typename SampleType, size_t BufferLen>
class RingBuffer {
public:
    RingBuffer()
        : head_(0)
        , tail_(BufferLen - 1)
    {
        reset();
    }

    // SampleType readHead() const {
    //     return buffer_[advance(head_)];
    // }

    SampleType readTail() {
        return buffer_[advance(tail_)];
    }

    SampleType writeHead(SampleType value) {
        return std::exchange(buffer_[advance(head_)], value);
    }

    // SampleType writeTail(SampleType value) {
    //     return std::exchange(buffer_[advance(tail_)], value);
    // }

    void reset() {
        for (size_t i = 0; i < BufferLen; i++) {
            buffer_[i] = 0.;
        }
        head_ = 0;
        tail_ = BufferLen - 1;
    }

private:

    // size_t advanceHead() {
    //     size_t next = (head_ + 1) % BufferLen;
    //     return std::exchange(head_, next);
    // }

    // size_t advanceTail() {
    //     size_t next = (head_ + 1) % BufferLen;
    //     return std::exchange(head_, next);
    // }

    size_t advance(size_t &index) {
        return std::exchange(index, (index + 1) % BufferLen);
    }

    SampleType buffer_[BufferLen];
    size_t head_;
    size_t tail_;
};

}
}
