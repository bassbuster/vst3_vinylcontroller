#include "effector.h"

namespace Steinberg::Vst {

void Effector::process(double &left, double &right, double &speed, double &tempo, double &volume) {
    for (auto& effect : effects_) {
        effect->process(left, right, speed, tempo, volume);
    }
}

void Effector::activeSet(Effect::Type active) {

    for (auto& effect : effects_) {
        if ((effect->type() & activeSet_) && (effect->type() & active) == 0) {
            effect->disactivate();
        }
        if ((effect->type() & activeSet_) == 0 && (effect->type() & active)) {
            effect->activate();
        }
    }
    activeSet_ = active;
}


}
