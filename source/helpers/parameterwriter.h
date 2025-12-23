#pragma once

#include "pluginterfaces/vst/ivstparameterchanges.h"

namespace Steinberg {
namespace Vst {

class ParameterWriter {
public:

    ParameterWriter(ParamID id, IParameterChanges *outParamChanges):
        id_(id),
        outParamChanges_(outParamChanges),
        index_(-1)
    {}

    void store(int32 sample, ParamValue val) {
        IParamValueQueue* paramQueue = (index_ < 0) ? outParamChanges_->addParameterData(id_, index_) : outParamChanges_->getParameterData(index_);
        if (paramQueue) {
            int32 ignore = 0;
            paramQueue->addPoint(sample, val, ignore);
        }
    }

private:
    ParamID id_;
    IParameterChanges *outParamChanges_;

    int32 index_;
};

}
}
