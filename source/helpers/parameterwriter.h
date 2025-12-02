#pragma once

//#include <map>
//#include <memory>

#include "pluginterfaces/vst/ivstparameterchanges.h"

namespace Steinberg {
namespace Vst {



class ParameterWriter {
public:

    ParameterWriter(ParamID id, IParameterChanges *outParamChanges):
        id(id),
        outParamChanges(outParamChanges),
        index(-1)
    {}

    void store(int32 sample, ParamValue val) {
        IParamValueQueue* paramQueue = (index < 0) ? outParamChanges->addParameterData(id, index) : outParamChanges->getParameterData(index);
        if (paramQueue) {
            int32 ignore = 0;
            paramQueue->addPoint(sample, val, ignore);
        }
    }

private:
    ParamID id;
    IParameterChanges *outParamChanges;

    int32 index;
    //IParamValueQueue* paramQueue;
    // IParamValueQueue* getParamQueue() {
    //     if (index < 0) {
    //         return outParamChanges->addParameterData(id, index);
    //     }
    //     return outParamChanges->getParameterData(index);

    // }
};

}
}
