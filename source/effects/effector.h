#pragma once

#include "effect.h"

#include <vector>
#include <memory>

namespace Steinberg::Vst {


class Effector {
public:

    Effector(std::vector<std::unique_ptr<Effect>> &&effects = {})
        : effects_(std::move(effects))
    {}

    void process(double &left, double &right, double &speed, double &tempo, double &volume);

    void activeSet(Effect::Type active);

    void append(std::unique_ptr<Effect> &&effect) {
        effects_.emplace_back(std::move(effect));
    }

private:

    std::vector<std::unique_ptr<Effect>> effects_;
    Effect::Type activeSet_;
};

}
