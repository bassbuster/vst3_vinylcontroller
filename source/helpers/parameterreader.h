#pragma once

#include <map>
#include <memory>

#include "pluginterfaces/vst/ivstparameterchanges.h"

namespace Steinberg {
namespace Vst {

template<typename Callable>
class Finalizer {
public:

    Finalizer(Callable&& fin):
        fin(std::forward<Callable>(fin))
    {}

    ~Finalizer() {
        fin();
    }

private:
    Callable fin;
};

class Reader {
public:

    virtual ~Reader() = default;

    virtual void setQueue(IParamValueQueue* paramQueue) = 0;

    virtual void checkOffset(int32 sampleOffset) = 0;

    //virtual void set(Sample64 initial) = 0;

    virtual void reset() = 0;

    virtual void flush() = 0;
};

template<typename Initializer, typename Setter, typename Approximator>
class ParameterReader: public Reader {
public:

    ParameterReader(Initializer && init,
                    Setter &&set,
                    Approximator &&approx):
        initializer_(std::forward<Initializer>(init)),
        setter_(std::forward<Setter>(set)),
        approximator_(std::forward<Approximator>(approx)),
        value_(0),
        queue_(nullptr),
        index_(0),
        offset_(0)
    {}

    ~ParameterReader() override {
        flush();
    }

    void setQueue(IParamValueQueue* paramQueue) override final {
        queue_ = paramQueue;
        if (queue_->getPoint(index_++, offset_, value_) != kResultTrue) {
            queue_ = nullptr;
        }
    }

    void checkOffset(int32 sampleOffset) override final {
        if (queue_ && offset_ == sampleOffset){
            setter_(value_);
            if (queue_->getPoint(index_++, offset_, value_) != kResultTrue) {
                queue_ = nullptr;
            }
        } else if (queue_ && offset_ > sampleOffset){
            approximator_(sampleOffset, offset_, value_);
        }
        lastCheckdOffset_ = sampleOffset;
    }

    // void set(Sample64 initial) override final {
    //     value = initial;
    //     queue = nullptr;
    //     index = 0;
    //     offset = 0;
    // }

    void reset() override final {
        value_ = initializer_();
        queue_ = nullptr;
        index_ = 0;
        offset_ = 0;
        lastCheckdOffset_ = -1;
    }

    void flush() override final {
        while (queue_){
            if (lastCheckdOffset_ < offset_) {
                setter_(value_);
            }
            if (queue_->getPoint(index_++, offset_, value_) != kResultTrue) {
                queue_ = nullptr;
            }
        }
    }

private:

    Initializer initializer_;
    Setter setter_;
    Approximator approximator_;
    ParamValue value_ = {};
    IParamValueQueue* queue_ = 0;
    int32 index_ = 0;
    int32 offset_ = 0;
    int32 lastCheckdOffset_ = 0;

};

class ReaderManager {
public:

    ReaderManager() = default;

    template<typename Initializer, typename Setter, typename Approximator>
    void addReader(ParamID id,
                   Initializer &&init,
                   Setter &&setter,
                   Approximator &&approx) {
        readers_.emplace(id, std::make_unique<ParameterReader<Initializer, Setter, Approximator>>(std::forward<Initializer>(init),
                                                               std::forward<Setter>(setter),
                                                               std::forward<Approximator>(approx)));
    }

    template<typename Initializer, typename Setter>
    void addReader(ParamID id,
                   Initializer &&init,
                   Setter &&setter) {
        auto approx = [](int32, int32, Sample64) {};
        readers_.emplace(id, std::make_unique<ParameterReader<Initializer, Setter, decltype(approx)>>(std::forward<Initializer>(init),
                                                               std::forward<Setter>(setter),
                                                               std::move(approx)));
    }

    void flush() {
        for(auto &[_, reader]: readers_) {
            reader->flush();
        }
    }

    void reset() {
        for(auto &[_, reader]: readers_) {
            reader->reset();
        }
    }

    void checkOffset(int32 sampleOffset) {
        for(auto &[_, reader]: readers_) {
            reader->checkOffset(sampleOffset);
        }
    }

    // void set(ParamID id, Sample64 value) {
    //     auto found = readers_.find(id);
    //     if (found != readers_.end()) {
    //         found->second->set(value);
    //     }
    // }

    void setQueue(IParameterChanges* paramChanges) {
        reset();
        if (!paramChanges) {
            return;
        }
        for (int32 i = 0, numParamsChanged = paramChanges->getParameterCount(); i < numParamsChanged; i++) {
            IParamValueQueue* paramQueue = paramChanges->getParameterData(i);
            if (paramQueue) {
                auto found = readers_.find(paramQueue->getParameterId());
                if (found != readers_.end()) {
                    found->second->setQueue(paramQueue);
                }
            }
        }
    }

private:
    std::map<ParamID, std::unique_ptr<Reader>> readers_;
};


}
}

