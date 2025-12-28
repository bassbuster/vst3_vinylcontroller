#pragma once

namespace Steinberg::Vst {

class Effect {
public:

    enum Type {
        NoEffects		= 0,
        Distorsion		= 1 << 0,
        PreRoll		    = 1 << 1,
        PostRoll  		= 1 << 2,
        PunchIn 		= 1 << 3,
        PunchOut		= 1 << 4,
        Hold			= 1 << 5,
        Freeze			= 1 << 6,
        Vintage		    = 1 << 7,
        LockTone		= 1 << 8
    };

    virtual ~Effect() = default;

    virtual void process(double &left, double &right, double &speed, double &tempo, double &volume) = 0;
    virtual void activate() = 0;
    virtual void disactivate() = 0;
    virtual Type type() const noexcept = 0;
};

}
