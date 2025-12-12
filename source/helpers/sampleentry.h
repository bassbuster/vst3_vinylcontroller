#pragma once

#include "cuepoint.h"

#include <vector>
#include <string>
#include <inttypes.h>
#include <cmath>

namespace Steinberg {
namespace Vst {

template<typename SampleType, typename ParameterType = double>
class SampleEntry {
public:
    using CuePoint = Helper::CuePoint<int64_t, ParameterType>;

    explicit SampleEntry(const char *name = nullptr, const char *fileName = nullptr):
        currentBeat(0),
        Loop(false),
        Sync(false),
        Reverse(false),
        Tune(1.),
        Level(1.),
        sampleName_(name),
        index_(0),
        acidBeats_(0),
        beatLength(0),
        beatOverlap(0),
        SmoothOverlap(-1)
    {
        if (fileName) {
            loadFromFile(fileName);
        }
    }

    ~SampleEntry() {
        clear();
    }

    void name(const char* text) {
        sampleName_ = text;
    }

    void fileName(const char* text) {
        sampleFile_ = text;
    }

    const char* name() const {
        return sampleName_.c_str();
    }

    const char* fileName() const {
        return sampleFile_.c_str();
    }

    bool loadFromFile(const char *fileName) {
        clear();

        std::vector<uint8_t> buffer;
        uint8_t header[9];

        FILE * fileHandle = fopen(fileName, "rb");
        if (!fileHandle) {
            fprintf(stderr,
                    "[SampeEntry] Error: File not found or not access(%s)",
                    fileName);
            return false;
        }

        size_t bytesRead = fread(header, 1, 8, fileHandle);
        if (bytesRead != 8) {
            fprintf(stderr,
                    "[SampeEntry] Error: File empty or not access (%s)",
                    fileName);
            fclose(fileHandle);
            return false;
        }

        size_t riffSize = analyseWavHeader(header);
        if (riffSize == 0) {
            fprintf(stderr,
                    "[SampeEntry] Error: Wrong file format (not RIFF file in %s)",
                    fileName);
            fclose(fileHandle);
            return false;
        }

        buffer.resize(riffSize + 1);
        bytesRead = fread(buffer.data(), 1, riffSize, fileHandle);
        fclose(fileHandle);

        if (bytesRead != riffSize) {

            fprintf(stderr,
                    "[SampeEntry] Error: Corrupted file(%s)",
                    fileName);
            return false;
        }

        if (analyseContainers(buffer.data(), riffSize, fileName)) {
            sampleFile_ = fileName;
            return true;
        }
        return false;
    }

    void resetCursor() {
        RealCursor.clear();
        OverlapCursorFirst.clear();
        beginLockStrobe();
    }

    bool moveCursor(ParameterType offset) {
        if (soundBufferLeft_.size() >= 4) {
            RealCursor = calcNewCursor(offset);
            return true;
        }
        return false;
    }

    void cue(CuePoint newCue) {
        RealCursor = normalizeCue(newCue);
        OverlapCursorFirst = RealCursor;
    }

    const CuePoint& cue() const {
        return RealCursor;
    }

    //unsigned NormalizeBeat(long _Beat);
    void beginLockStrobe() {
        OverlapCursorFirst = RealCursor;
        SmoothOverlap = -1;
    }

    void playStereoSample(SampleType *left, SampleType *right, ParameterType speed, ParameterType tempo, ParameterType sampleRate, bool changeCursors) {
        if (Sync && (acidBeats_>0)) {
            playStereoSampleTempo(left, right, speed * (ParameterType(SampleRate) / ParameterType(sampleRate)), fabs(tempo * speed), sampleRate, changeCursors);
        } else {
            playStereoSample(left, right, calcRealSpeed(speed, sampleRate), changeCursors);
        }
    }

    void playStereoSampleTempo(SampleType *left, SampleType *right,
                               ParameterType speed, ParameterType tempo, ParameterType sampleRate,
                               bool changeCursors) {
        auto newSpeed = calcRealSpeed(speed, sampleRate);
        auto newTempoSpeed = calcTempoSpeed(speed, tempo, sampleRate);

        CuePoint PushCue = calcNewCursor(newTempoSpeed);
        CuePoint PushCue2 = RealCursor;
        if ((newSpeed / newTempoSpeed) <= 1.3) {
            if (checkOverlapEvent(PushCue, newTempoSpeed) && (SmoothOverlap <= 0.00005)) {
                overlapStrobe(newTempoSpeed, fabs(newSpeed));
            } else {
                RealCursor = OverlapCursorFirst;
            }
        } else if ((newSpeed / newTempoSpeed) > 1.3) {
            if (checkStratchEvent(PushCue, newSpeed, newTempoSpeed) && (SmoothOverlap <= 0.00005)) {
                stratchStrobe(PushCue);
            }
            RealCursor = OverlapCursorFirst;
        }

        playStereoSample(left, right, newSpeed, true);
        OverlapCursorFirst = RealCursor;

        if (SmoothOverlap > 0.0) {
            RealCursor = OverlapCursorSecond;
            SampleType OverlapLeft;
            SampleType OverlapRight;
            playStereoSample(&OverlapLeft, &OverlapRight, newSpeed, true);
            OverlapCursorSecond = RealCursor;
            ParameterType CorrectorCoef = (1. - cos(SmoothOverlap * 2. * Pi)) / 10.;

            *left = *left * (SmoothOverlap + CorrectorCoef) + OverlapLeft * (1. - SmoothOverlap + CorrectorCoef);
            *right = *right * (SmoothOverlap + CorrectorCoef) + OverlapRight * (1. - SmoothOverlap + CorrectorCoef);
            OverlapCursorSecond = RealCursor;

            if ((newSpeed / newTempoSpeed) <= 1.3) {
                SmoothOverlap -= (fabs(newTempoSpeed / (ParameterType(beatOverlap) * newSpeed)));
                if (SmoothOverlap < 0.00001) {
                    OverlapCursorFirst = OverlapCursorSecond;
                    SmoothOverlap = -1;
                }
            } else {
                SmoothOverlap -= (2.3 / ParameterType(beatOverlap));
                if (SmoothOverlap < 0.00001) {
                    SmoothOverlap = 0.00001;
                }
            }
        }
        RealCursor = changeCursors ? PushCue : PushCue2;
    }

    void playStereoSample(SampleType *Left, SampleType *Right, ParameterType offset, bool changeCursors) {

        if (soundBufferLeft_.size() >= 4) {

            CuePoint NewCursor = calcNewCursor(offset);

            SampleType Point0 = 0;
            SampleType Point1 = soundBufferLeft_[NewCursor.integerPart()];
            SampleType Point2 = 0;
            SampleType Point3 = 0;
            if (NewCursor.integerPart() > 0) {
                Point0 = soundBufferLeft_[NewCursor.integerPart() - 1];
            }
            if (NewCursor.integerPart() < int64_t(soundBufferLeft_.size()) - 2) {
                Point2 = soundBufferLeft_[NewCursor.integerPart() + 1];
            }
            if (NewCursor.integerPart() < int64_t(soundBufferLeft_.size()) - 3) {
                Point3 = soundBufferLeft_[NewCursor.integerPart() + 2];
            }
            *Left = Level * hermite(NewCursor.floatPart(), Point0, Point1, Point2, Point3);

            Point0 = 0;
            Point1 = soundBufferRight_[NewCursor.integerPart()];
            Point2 = 0;
            Point3 = 0;
            if (NewCursor.integerPart() > 0) {
                Point0 = soundBufferRight_[NewCursor.integerPart() - 1];
            }
            if (NewCursor.integerPart() < int64_t(soundBufferRight_.size()) - 2) {
                Point2 = soundBufferRight_[NewCursor.integerPart() + 1];
            }
            if (NewCursor.integerPart() < int64_t(soundBufferRight_.size()) - 3) {
                Point3 = soundBufferRight_[NewCursor.integerPart() + 2];
            }
            *Right = Level * hermite(NewCursor.floatPart(), Point0, Point1,
                                     Point2, Point3);

            if (changeCursors) {
                RealCursor = NewCursor;
            }

        } else {
            *Left = 0;
            *Right = 0;
        }
    }

    ParameterType getNoteLength(ParameterType note, ParameterType tempo) {
        if (Sync && (acidBeats_ > 0)) {
            return ParameterType(soundBufferLeft_.size()) / acidBeats_ * note;
        } else if (tempo > 0) {
            return ParameterType(SampleRate) / tempo * 60. * note;
        }
        return 0;
    }

    SampleType peakSample(size_t from_position, size_t to_position) {
        if (from_position > soundBufferLeft_.size()) {
            return 0;
        }

        if (to_position > soundBufferLeft_.size()) {
            to_position = soundBufferLeft_.size();
        }

        if (from_position > to_position) {
            std::swap(from_position, to_position);
        }

        SampleType peak = 0;
        for (size_t i = from_position; i < to_position; i++) {
            if (peak < fabs(avgSample(i))) {
                peak = fabs(avgSample(i));
            }
        }
        return peak;
    }

    ParameterType tempo() const {
        return acidBeats_ > 0
                   ? 60. / (ParameterType(bufferLength()) / ParameterType(SampleRate)) * acidBeats_
                   : 60. / (ParameterType(bufferLength()) / ParameterType(SampleRate)) * defaultBeats;
    }

    size_t bufferLength() const {
        return soundBufferLeft_.size();
    }

    size_t getACIDbeats() const {
        return acidBeats_;
    }

    size_t index() const {
        return index_;
    }

    void index(size_t idx) {
        index_ = idx;
    }

    void clear() {
        soundBufferLeft_.clear();
        soundBufferRight_.clear();
        RealCursor.clear();
        OverlapCursorFirst.clear();
        Loop = false;
        Sync = false;
        Reverse = false;
        Tune = 1.;
        Level = 1.;
    }

    bool operator == (const SampleEntry & other) const {
        return (other.soundBufferLeft_ == soundBufferLeft_) && (other.soundBufferRight_ == soundBufferRight_);
    }

    bool operator != (const SampleEntry & other) const {
        return (other.soundBufferLeft_ != soundBufferLeft_) || (other.soundBufferRight_ != soundBufferRight_);
    }

    bool Loop;
    bool Sync;
    bool Reverse;
    ParameterType Tune;
    ParameterType Level;

private:

    static constexpr ParameterType defaultBeats = 8.;
    static constexpr ParameterType beatOverlapKoef = 1./2.;
    static constexpr size_t beatOverlapMultiple = 8;
    static constexpr SampleType Pi = 3.14159265358979323846264338327950288;


    SampleType hermite(SampleType x, SampleType y0, SampleType y1, SampleType y2, SampleType y3) {
        SampleType c0 = y1;
        SampleType c1 = 0.5 * (y2 - y0);
        SampleType c2 = y0 - 2.5 * y1 + 2. * y2 - 0.5 * y3;
        SampleType c3 = 1.5 * (y1 - y2) + 0.5 * (y3 - y0);
        return ((c3 * x + c2) * x + c1) * x + c0;
    }

    CuePoint calcNewCursor(ParameterType offset) {
        CuePoint newCursor(RealCursor);

        ParameterType CurrentSpeed;
        CurrentSpeed = offset;

        if (!Loop) {
            if ((newCursor.integerPart() == soundBufferLeft_.size() - 1) && (CurrentSpeed > 0)) {
                return newCursor;
            }
            if ((newCursor.integerPart() == 0) && (CurrentSpeed < 0)) {
                return newCursor;
            }
        }

        newCursor += CurrentSpeed;
        return normalizeCue(newCursor);
    }

    bool analyseContainers(uint8_t *buffer, size_t bufferSize, const char *resourceName) {

        size_t iFormLength = analyseWavForm(buffer);
        if (iFormLength == 0) {
            fprintf(stderr,
                    "[SampeEntry] Error: Wrong format (not WAVE form in %s)",
                    resourceName);
            return false;
        }

        uint16_t nCannels = 0;
        uint32_t iSamplesPerSec = 0;
        uint16_t iBitsPerSample = 0;
        size_t iCursor = iFormLength + 12;
        bool foundDataContainer = false;
        size_t SoundBufferLength = 0;
        if (!analysePCMCodec(buffer, nCannels, iSamplesPerSec, iBitsPerSample)) {
            fprintf(stderr,
                    "[SampeEntry] Error: Wrong format (not PCM wave in %s)",
                    resourceName);
            return false;
        }

        while (iCursor < (bufferSize - 8)) {

            iFormLength = containerSize(buffer + iCursor + 4);

            if (isDataContainer(buffer + iCursor)) {
                foundDataContainer = true;
                SoundBufferLength = iFormLength / (nCannels * iBitsPerSample / 8);
                soundBufferLeft_.resize(SoundBufferLength + 1);
                soundBufferRight_.resize(SoundBufferLength + 1);

                uint8_t *Data = buffer + iCursor + 8;
                unsigned step = nCannels * iBitsPerSample / 8;
                for (unsigned i = 0; i < iFormLength; i = i + step) {

                    for (unsigned j = 0; j < nCannels; j++) {
                        uint32_t CannelData = getChannelData(Data + i, j, iBitsPerSample);
                        SampleType FSample = convertToSample(CannelData, buffer[12], iBitsPerSample);

                        switch (j) {
                        case 0:
                            soundBufferLeft_[i / step] = FSample;
                            if (nCannels >= 2) {
                                break;
                            }
                        case 1:
                            soundBufferRight_[i / step] = FSample;
                            break;
                        default:
                            break;
                        }

                    }
                }
                SampleRate = iSamplesPerSec;
                beatLength = SoundBufferLength / defaultBeats / beatOverlapMultiple;
                beatOverlap = beatLength * beatOverlapKoef;

            }

            // TODO: analyze containers second time if ACID container is before Data
            if (isAcidContainer(buffer + iCursor) && foundDataContainer) {
                if (isLoop(buffer + iCursor)) {
                    acidBeats_ = getAcidBeats(buffer + iCursor);
                    beatLength = SoundBufferLength / acidBeats_ / beatOverlapMultiple;
                    beatOverlap = beatLength * beatOverlapKoef;
                    Loop = true;
                    Sync = true;
                } else if (isOneShoot(buffer + iCursor)) {
                    acidBeats_ = getAcidBeats(buffer + iCursor);
                    beatLength = SoundBufferLength / acidBeats_ / beatOverlapMultiple;
                    beatOverlap = beatLength * beatOverlapKoef;
                    Loop = false;
                    Sync = true;
                }
            }
            iCursor += iFormLength + 8;
        }

        if (!foundDataContainer) {
            fprintf(stderr,
                    "[SampeEntry] Error: Wrong format (not found 'data' container in %s)",
                    resourceName);
            return false;
        }
        return true;
    }

    bool checkOverlapEvent(CuePoint &cue, ParameterType offset) {

        if (((offset > 0) && (cue < RealCursor))
            || ((offset < 0) && (cue > RealCursor))) {
            return false;
        }
        if (((RealCursor.integerPart()  + beatOverlap) / beatLength) != ((cue.integerPart() + beatOverlap) / beatLength)) {
            return true;
        }
        return false;
    }

    bool checkStratchEvent(CuePoint &cue, ParameterType speed, ParameterType speedtempo) {
        if ((speedtempo > 0) && (cue > OverlapCursorSecond)) {
            return (std::abs(OverlapCursorSecond.integerPart() + int64_t(soundBufferLeft_.size()) - cue.integerPart()) > int64_t(beatLength / 2));
        } else if ((speedtempo < 0) && (cue < OverlapCursorSecond)) {
            return (std::abs(OverlapCursorSecond.integerPart() - cue.integerPart() + int64_t(soundBufferLeft_.size())) > int64_t(beatLength / 2));
        } else {
            return (std::abs(OverlapCursorSecond.integerPart() - cue.integerPart()) > int64_t(beatLength / 2));
        }
        return false;
    }

    void overlapStrobe(ParameterType speed, ParameterType tune) {
        if (speed < 0) {
            OverlapCursorSecond.set(beatLength * normalizeStretchBeat(int64_t(RealCursor.integerPart() / beatLength)) - beatOverlap * tune / speed, 0);
        } else {
            OverlapCursorSecond.set(beatLength * normalizeStretchBeat(int64_t(RealCursor.integerPart() / beatLength) + 1) - beatOverlap * tune / speed, 0);
        }
        OverlapCursorSecond = normalizeCue(OverlapCursorSecond);
        RealCursor = OverlapCursorFirst;
        SmoothOverlap = 1;
    }

    void stratchStrobe(const CuePoint &cue) {
        OverlapCursorFirst = OverlapCursorSecond;
        OverlapCursorSecond = cue;
        SmoothOverlap = 1;
    }

    int64_t normalizeStretchBeat(int64_t beat){
        if (acidBeats_ > 0) {
            if (Loop) {
                return (beat >= int64_t(acidBeats_ * beatOverlapMultiple)) ? (beat % (acidBeats_ * beatOverlapMultiple)) : ((beat < 0) ? ((acidBeats_ * beatOverlapMultiple) + (beat % (acidBeats_ * beatOverlapMultiple))) : beat);
            } else {
                return (beat >= int64_t(acidBeats_ * beatOverlapMultiple)) ? int64_t(acidBeats_ * beatOverlapMultiple) - 1 : 0;
            }
        }
        if constexpr (defaultBeats > 0) {
            if (Loop) {
                return (beat >= int64_t(defaultBeats * beatOverlapMultiple)) ? (beat % int64_t(defaultBeats * beatOverlapMultiple)) : ((beat < 0) ? (defaultBeats * beatOverlapMultiple + (beat % int64_t(defaultBeats * beatOverlapMultiple))) : beat);
            } else {
                return (beat >= int64_t(defaultBeats * beatOverlapMultiple)) ? defaultBeats * beatOverlapMultiple - 1 : 0;
            }
        }
        return 0;
    }

    SampleEntry::CuePoint normalizeCue(const CuePoint& cue) {
        CuePoint ret(cue);
        if (Loop
            || ((ret.integerPart() >= 0) && (ret.integerPart() < int64_t(soundBufferLeft_.size())))) {

            if (ret.integerPart() >= int64_t(soundBufferLeft_.size())) {
                ret.set(ret.integerPart() % soundBufferLeft_.size(), cue.floatPart());
            }
            if (ret.integerPart() < 0) {
                ret.set(soundBufferLeft_.size() - (ret.integerPart() % soundBufferLeft_.size()), cue.floatPart());
            }

        } else if (ret.integerPart() >= int64_t(soundBufferLeft_.size())) {
            ret.set(soundBufferLeft_.size() - 1, 0.);
        } else if (ret.integerPart() < 0) {
            ret.clear();
        }
        return ret;
    }

    std::vector<SampleType> soundBufferLeft_;
    std::vector<SampleType> soundBufferRight_;

    std::string sampleName_;
    std::string sampleFile_;

    size_t index_;
    size_t acidBeats_;
    size_t beatLength;
    size_t beatOverlap;
    size_t SampleRate;
    size_t currentBeat;

    CuePoint RealCursor;
    CuePoint OverlapCursorFirst;
    CuePoint OverlapCursorSecond;

    ParameterType SmoothOverlap;


    size_t analyseWavHeader(uint8_t *header) {
        if ((header[0] == 'R') && (header[1] == 'I') && (header[2] == 'F') && (header[3] == 'F')) {
            return containerSize(header + 4);
        }
        return 0;
    }

    size_t analyseWavForm(uint8_t *form) {
        if ((form[0] == 'W') && (form[1] == 'A') && (form[2] == 'V') &&
            (form[3] == 'E') && (form[4] == 'f') && (form[5] == 'm') &&
            (form[6] == 't') && (form[7] == ' ')) {
            return containerSize(form + 8);
        }
        return 0;
    }

    bool analysePCMCodec(uint8_t *form, uint16_t &nCannels, uint32_t &iSamplesPerSec, uint16_t &iBitsPerSample) {
        if ((((form[12] == 3)) || (form[12] == 1)) && (form[13] == 0)) {
            nCannels = (form[14] & 0xff) + ((form[15] & 0xff) << 8);
            iSamplesPerSec = (form[16] & 0xff) + ((form[17] & 0xff) << 8) +
                             ((form[18] & 0xff) << 16) + ((form[19] & 0xff) << 24);
            iBitsPerSample = (form[26] & 0xff) + ((form[27] & 0xff) << 8);
            return true;
        }
        return false;
    }

    size_t containerSize(uint8_t *container) {
        size_t length = container[3] & 0xff;
        length = (length << 8) + (container[2] & 0xff);
        length = (length << 8) + (container[1] & 0xff);
        length = (length << 8) + (container[0] & 0xff);
        return length;
    }

    bool isDataContainer(uint8_t *container) {
        if ((container[0] == 'd') && (container[1] == 'a') &&
            (container[2] == 't') && (container[3] == 'a')) {
            return true;
        }
        return false;
    }

    bool isAcidContainer(uint8_t *container) {
        if ((container[0] == 'a') && (container[1] == 'c') &&
            (container[2] == 'i') && (container[3] == 'd')) {
            return true;
        }
        return false;
    }

    bool isLoop(uint8_t *container) {
        if ((container[8] == 0) || (container[8] == 2)) {
            return true;
        }
        return false;
    }

    bool isOneShoot(uint8_t *container) {
        if ((container[8] == 1) || (container[8] == 3)) {
            return true;
        }
        return false;
    }

    uint8_t getAcidBeats(uint8_t *container) {
        return container[20];
    }

    uint32_t getChannelData(uint8_t *Buffer, uint8_t channel, uint8_t BitsPerSample) {
        uint32_t cannelData = (Buffer[BitsPerSample / 8 - 1 + channel * BitsPerSample / 8] >= 0) ? 0. : 0xffffffff;
        for (int k = BitsPerSample / 8 - 1; k >= 0; k--) {
            cannelData = (cannelData << 8) + (Buffer[k + channel * BitsPerSample / 8] & 0xff);
        }
        return cannelData;
    }

    inline int sign(ParameterType val) {
        return (val > 0) ? 1 : (val < 0) ? -1 : 0;
    }

    SampleType convertToSample(uint32_t cannelData, uint8_t sampleType, uint8_t bitsPerSample) {
        /**
         *  Supported bitrates 8/16/24/32(IEEE Float)
         **/
        if (sampleType == 1) {
            if (bitsPerSample == 8) {
                return int8_t(cannelData) / 127.0;
            }
            if (bitsPerSample == 16) {
                return int16_t(cannelData) / 32767.0;
            }
            if (bitsPerSample == 24) {
                return int32_t(cannelData) / 8388607.0;
            }
        } else if (bitsPerSample == 32) { // 32bit IEEE Float
            return *reinterpret_cast<float *>(&cannelData);
        }
        return 0.;
    }

    ParameterType calcTempoSpeed(ParameterType speed, ParameterType tempo, ParameterType sampleRate) {
        ParameterType dir = Reverse? -sign(speed) : sign(speed);
        if ((acidBeats_ > 0) && (sampleRate > 0)) {
            return dir * soundBufferLeft_.size() * tempo / 60. / sampleRate / acidBeats_;
        } else if (sampleRate > 0) {
            return dir * soundBufferLeft_.size() * tempo / 60. / sampleRate / defaultBeats;
        }
        return calcRealSpeed(speed, sampleRate);
    }

    ParameterType calcRealSpeed(ParameterType speed, ParameterType sampleRate) {
        ParameterType dir = Reverse ? -1 : 1;
        return (sampleRate > 0) ? speed * Tune * (ParameterType(SampleRate) / ParameterType(sampleRate)) * dir : 0;
    }

    SampleType avgSample(size_t position) {
        if (position < soundBufferLeft_.size()) {
            return (soundBufferLeft_[position] +
                    soundBufferRight_[position]) / 2.0;
        }
        return 0;
    }

};

}
}
