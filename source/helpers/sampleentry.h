#pragma once

#include "base/source/fstring.h"
#include "pluginterfaces/vst/vsttypes.h"
#include "cuepoint.h"

#include <math.h>
#include <vector>

namespace Steinberg {
namespace Vst {

typedef uint8_t  BYTE;
typedef uint32_t DWORD;
constexpr double Pi = 3.14159265358979323846264338327950288;
constexpr Sample32 defaultBeats = 8.;
constexpr Sample32 beatOverlapKoef = 1./2.;
constexpr uint32_t beatOverlapMultiple = 8;

class PadEntry {
public:
    enum TypePad {
        kSamplePad = 0,
        kSwitchPad,
        kKickPad,
        kAssigMIDI,
        kEmptyPad = -1
    };
    int padTag;
    int padMidi;
    bool padState;
    TypePad padType;
};

// class CuePoint {
// public:
//     explicit CuePoint(long integerPart = 0, double floatPart = 0.):
//         IntegerPart{integerPart},
//         FloatPart{floatPart}
//     {}

//     CuePoint(const CuePoint &other) = default;
//     CuePoint(CuePoint &&other) = default;
//     CuePoint& operator=(const CuePoint &other) = default;
//     CuePoint& operator=(CuePoint &&other) = default;

//     void SetCue(long _integer, double _float) {IntegerPart = _integer;FloatPart=_float;}
//     void SetCue(CuePoint _cue) {IntegerPart = _cue.IntegerPart;FloatPart=_cue.FloatPart;}

//     CuePoint& operator = (double _offset) { IntegerPart = (long) _offset; FloatPart=_offset - (double)IntegerPart; return *this;}
//     CuePoint& operator = (long _offset) { IntegerPart = _offset;FloatPart=0; return *this;}
//     bool operator > (const CuePoint& _cue) const { if ((IntegerPart > _cue.IntegerPart)||((IntegerPart == _cue.IntegerPart)&&(FloatPart > _cue.FloatPart))) return true; return false;}
//     bool operator < (const CuePoint& _cue) const { if ((IntegerPart < _cue.IntegerPart)||((IntegerPart == _cue.IntegerPart)&&(FloatPart < _cue.FloatPart))) return true; return false;}
//     bool operator == (const CuePoint& _cue) const { if ((IntegerPart == _cue.IntegerPart)&&(FloatPart == _cue.FloatPart)) return true; return false;}
//     bool operator >= (const CuePoint& _cue) const { if ((*this > _cue)||(*this == _cue)) return true; return false; }
//     bool operator <= (const CuePoint& _cue) const { if ((*this < _cue)||(*this == _cue)) return true; return false; }
//     CuePoint& operator += (double _offset) { FloatPart += _offset; Normalize(); return *this; }
//     CuePoint& operator -= (double _offset) { FloatPart -= _offset; Normalize(); return *this; }

//     friend CuePoint operator + (const CuePoint& cue, double offset) {CuePoint ret(cue);  ret += offset; return ret;}
//     friend CuePoint operator - (const CuePoint& cue, double offset) {CuePoint ret(cue);  ret -= offset; return ret;}

//     inline void Normalize() {
//         if (FloatPart > 1.0) {
//             IntegerPart += (long)FloatPart;
//             FloatPart = FloatPart - (long)FloatPart;
//         }
//         if (FloatPart < 0) {
//             IntegerPart += ((long)FloatPart - 1);
//             FloatPart = 1 + (FloatPart - (long)FloatPart);
//         }
//     }

//     Sample64 GetPointDouble() {
//         return Sample64(IntegerPart) + FloatPart;
//     }

//     void Clear() {
//         IntegerPart = 0;
//         FloatPart=0;
//     }

//     long IntegerPart;
//     Sample64 FloatPart;
// };

class SampleEntry {
public:
    using CuePoint = Helper::CuePoint<int64_t, double>;

    enum channel{
        kBufferLeftChannel = 0,
        kBufferRightChannel
    };

    explicit SampleEntry(const char *name = nullptr, const char *fileName = nullptr);

    ~SampleEntry();

    void SetName(const char *  text);
    void SetFileName(const char *  text);
    const char * GetName();
    const char * GetFileName();
    bool LoadFromFile(const char *  FileName);


    void ResetCursor();
    bool MoveCursor(Sample64 _offset);
    void SetCue(CuePoint cue) {
        RealCursor = NormalizeCue(cue);
        OverlapCursorFirst = RealCursor;
    }

    unsigned NormalizeBeat(long _Beat);
    void BeginLockStrobe();
    //void Hold(long iterationCount);
    //void Resume();
    CuePoint& GetCue() {return RealCursor;}
    Sample64 RecalcSpeed(Sample64 _Speed, Sample64 _Tempo, Sample64 _SampleRate);
    Sample64 CalcTempoSpeed(Sample64 _Speed, Sample64 _Tempo, Sample64 _SampleRate);
    Sample64 CalcRealSpeed(Sample64 _Speed, Sample64 _SampleRate);
    void PlayStereoSample(Sample32 *Left, Sample32 *Right,
                          Sample64 _Speed,bool _changeCursors);
    void PlayStereoSample(Sample32 *Left, Sample32 *Right,
                          Sample64 _Speed,Sample64 _Tempo, Sample64 _SampleRate,bool _changeCursors);
    void PlayStereoSampleTempo(Sample32 *Left, Sample32 *Right,
                               Sample64 _Speed,Sample64 _Tempo, Sample64 _SampleRate,bool _changeCursors);
    CuePoint CalcNewCursor(Sample64 _offset);
    void MoveCursorEnv(Sample64 _offset, Sample64 _Tempo, Sample64 _SampleRate);
    Sample64 GetNoteLength(Sample64 _note, Sample64 _Tempo);
    Sample64 GetNoteLengthEnv(Sample64 _note, Sample64 _Tempo, Sample64 _SampleRate);
    Sample32* GetBuffer(channel channelname);
    Sample32 GetSample(unsigned position,channel channelname);
    Sample32 GetAvgSample(unsigned position);
    Sample32 GetPeakSample(unsigned from_position,unsigned to_position);
    Sample64 GetTempo() {
        return ACIDbeats > 0
                   ? 60. / (Sample64(SoundBufferLeft.size()) / Sample64(SampleRate)) * ACIDbeats
                   : 60. / (Sample64(SoundBufferLeft.size()) / Sample64(SampleRate)) * defaultBeats;
    }
    void setCursorOnBeat(unsigned _BeatNumber);
    unsigned setNextBeat(short _direction);
    void setFirstBeat(void);
    void setLastBeat(void);
    //void PlayBeatNumberEnv(Sample32 *Left, Sample32 *Right,
    //	Sample64 _Speed,Sample64 _Tempo, Sample64 _SampleRate,bool _changeCursors);

    unsigned GetBufferLength();
    unsigned GetACIDbeats() {return ACIDbeats;}
    unsigned GetIndex() {return index;}
    void SetIndex(unsigned idx) {index = idx;}
    void ClearBuffer();

    bool operator == (const SampleEntry& Other) const ;
    bool operator != (const SampleEntry& Other) const ;

    //long IntegerCursor;
    //Sample64 FloatCursor;
    bool Loop;
    bool Sync;
    bool Reverse;
    Sample64 Tune;
    Sample64 Level;

private:

    bool AnalyseContainers(BYTE * Buffer,unsigned size,const char * ResourceName);

    bool CheckSyncroEvent(CuePoint &cue,Sample64 offset);
    bool CheckOverlapEvent(CuePoint &cue,Sample64 offset);
    bool CheckStratchEvent(CuePoint &cue,Sample64 speed, Sample64 speedtempo);
    bool CheckRestratchEvent(const CuePoint &cue, Sample64 offset);
    void OverlapStrobe(double speed, double tune);
    void StratchStrobe(const CuePoint &cue);
    void SyncStrobe();
    unsigned NormalizeStretchBeat(long _Beat);

    CuePoint& NormalizeCue(CuePoint &cue);
    CuePoint NormalizeCue(const CuePoint &cue);

    std::vector<Sample32> SoundBufferLeft;
    std::vector<Sample32> SoundBufferRight;
    //long SoundBufferLength;
    String SampleName;
    String SampleFile;
    unsigned index;
    unsigned ACIDbeats;
    unsigned beatLength;
    unsigned beatOverlap;
    unsigned SampleRate;
    unsigned currentBeat;
    CuePoint RealCursor;
    CuePoint OverlapCursorFirst;
    CuePoint OverlapCursorSecond;
    Sample32 SmoothOverlap;

};

inline bool between(long int MinValue, long int Value,
                    long int MaxValue) {
    if (MinValue < MaxValue) {
        if ((MinValue <= Value) && (Value < MaxValue)) {
            return true;
        } else {
            return false;
        }
    }
    else {
        if ((MinValue >= Value) && (Value > MaxValue)) {
            return true;
        } else {
            return false;
        }
    }
}

inline float sqr(int x) {
    return x * x;
}

inline float sqr(float x) {
    return x * x;
}

inline double sqr(double x) {
    return x * x;
}

void fastsinetransform(Sample32* a, int tnn);

Sample64 hermite(Sample64 x, Sample64 y0, Sample64 y1, Sample64 y2,
                 Sample64 y3);

int bitNumber(int _bitSet);

inline int Sign(double _val){
    if (_val>0) {
        return 1;
    }
    if (_val<0) {
        return -1;
    }
    return 0;
}

inline Sample64 noteLengthInSamples(Sample64 note, Sample64 tempo, Sample64 sampleRate)
{
    if ((tempo > 0) && (note > 0)) {
        return sampleRate / tempo * 60.0 * note;
    }
    return 0;

}


}
}
