#pragma once

namespace Steinberg::Vst {

class PadEntry {
public:
    enum TypePad {
        EmptyPad = -1,
        SamplePad = 0,
        SwitchPad,
        KickPad,
        AssigMIDI,
    };
    int padTag;
    int padMidi;
    bool padState;
    TypePad padType;
};

}
