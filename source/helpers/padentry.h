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

    void updateState(int currentEntry, uint32_t effectorSet)
    {
        switch (padType) {
        case PadEntry::SamplePad:
            padState = (padTag == currentEntry);
            break;
        case PadEntry::SwitchPad:
            padState = (padTag & effectorSet);
            break;
        default:
            padState = false;
        }
    }
};

}
