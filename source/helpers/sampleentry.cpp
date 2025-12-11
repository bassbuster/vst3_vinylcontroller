#include <stdio.h>

#include "sampleentry.h"


namespace Steinberg {
namespace Vst {

    template<typename SampleType>
    SampleType hermite(SampleType x, SampleType y0, SampleType y1, SampleType y2, SampleType y3) {
        SampleType c0 = y1;
        SampleType c1 = 0.5 * (y2 - y0);
        SampleType c2 = y0 - 2.5 * y1 + 2. * y2 - 0.5 * y3;
        SampleType c3 = 1.5 * (y1 - y2) + 0.5 * (y3 - y0);
        return ((c3 * x + c2) * x + c1) * x + c0;
    }

// SampleEntry::SampleEntry() {
// 	SoundBufferLeft = NULL;
// 	SoundBufferRight = NULL;
// 	SoundBufferLength = 0;
// 	// IntegerCursor = 0;
// 	// FloatCursor = 0;
//           // RealCursor.Clear();
//           // OverlapCursorFirst.Clear();
// 	currentBeat = 0;
// 	//holdSize = -1;
// 	Loop = false;
// 	Sync = false;
// 	Reverse = false;
// 	Tune = 1.0f;
// 	Level = 1.0f;
// 	index = 0;
// 	ACIDbeats = 0;
// 	beatLength = 0;
// 	beatOverlap = 0;
// 	SmoothOverlap = -1;

// }

// SampleEntry::SampleEntry(const char * Name) {
// 	SoundBufferLeft = NULL;
// 	SoundBufferRight = NULL;
// 	SoundBufferLength = 0;
// 	// IntegerCursor = 0;
// 	// FloatCursor = 0;
//           // RealCursor.Clear();
//           // OverlapCursorFirst.Clear();
// 	currentBeat = 0;
// 	//holdSize = -1;
// 	Loop = false;
// 	Sync = false;
// 	Reverse = false;
// 	Tune = 1.0f;
// 	Level = 1.0f;
// 	SampleName = Name;
// 	index = 0;
// 	ACIDbeats = 0;
// 	beatLength = 0;
// 	// String tmp(Name);
// 	beatOverlap = 0;
// 	SmoothOverlap = -1;
// 	// tmp.fro
// 	// tmp.copyTo(SampleName, 0, 127);
// }

SampleEntry::SampleEntry(const char * name, const char * file):
    // SoundBufferLeft{},
    // SoundBufferRight{},
    // SoundBufferLength(0),
    currentBeat(0),
    Loop(false),
    Sync(false),
    Reverse(false),
    Tune(1.0f),
    Level(1.0f),
    SampleName(name),
    index(0),
    ACIDbeats(0),
    beatLength(0),
    beatOverlap(0),
    SmoothOverlap(-1)

{
    if (file) {
        LoadFromFile(file);
    }
}

// SampleEntry::SampleEntry(void * hInstance, const char * ResourceName) {
// 	SoundBufferLeft = NULL;
// 	SoundBufferRight = NULL;
// 	SoundBufferLength = 0;
//           // RealCursor.Clear();
//           // OverlapCursorFirst.Clear();
// 	currentBeat = 0;
// 	//holdSize = -1;
// 	Loop = false;
// 	Sync = false;
// 	Reverse = false;
// 	Tune = 1.0f;
// 	Level = 1.0f;
// 	SampleName = ResourceName;
// 	index = 0;
// 	ACIDbeats = 0;
// 	beatLength = 0;
// 	beatOverlap = 0;
// 	SmoothOverlap = -1;
// 	LoadFromResource(hInstance, ResourceName);
// }

SampleEntry::~SampleEntry() {
    ClearBuffer();
}

static unsigned AnalyseWavHeader(BYTE * Header);
static unsigned AnalyseWavForm(BYTE * Form);
static bool AnalysePCMCodec(BYTE * Form,unsigned & nCannels,unsigned & iSamplesPerSec,unsigned & iBitsPerSample);
static unsigned GetContainerSize(BYTE * Container);
static bool isDataContainer(BYTE * Container);
static bool isAcidContainer(BYTE * Container);
static bool isLoop(BYTE * Container);
static bool isOneShoot(BYTE * Container);
static BYTE GetAcidBeats(BYTE * Container);
static DWORD GetChannelData(BYTE * Buffer,unsigned channel,unsigned BitsPerSample);
static Sample32 ConvertToSample(DWORD CannelData,BYTE Type,unsigned BitsPerSample);


unsigned AnalyseWavHeader(BYTE * Header) {
    if ((Header[0] == 'R') && (Header[1] == 'I') && (Header[2] == 'F')
        && (Header[3] == 'F')) {
        return GetContainerSize(Header + 4);
    }
    return 0;
}

unsigned AnalyseWavForm(BYTE * Form) {
    if ((Form[0] == 'W') && (Form[1] == 'A') && (Form[2] == 'V') &&
        (Form[3] == 'E') && (Form[4] == 'f') && (Form[5] == 'm') &&
        (Form[6] == 't') && (Form[7] == ' ')) {
        return GetContainerSize(Form + 8);
    }
    return 0;
}

bool AnalysePCMCodec(BYTE * Form, unsigned & nCannels,
                                  unsigned & iSamplesPerSec, unsigned & iBitsPerSample) {
    if ((((Form[12] == 3)) || (Form[12] == 1)) && (Form[13] == 0)) {
        nCannels = (Form[14] & 0xff) + ((Form[15] & 0xff) << 8);
        iSamplesPerSec = (Form[16] & 0xff) + ((Form[17] & 0xff) << 8) +
                         ((Form[18] & 0xff) << 16) + ((Form[19] & 0xff) << 24);
        iBitsPerSample = (Form[26] & 0xff) + ((Form[27] & 0xff) << 8);
        return true;
    }
    return false;
}

unsigned GetContainerSize(BYTE * Container) {
    unsigned iFormLength = Container[3] & 0xff;
    iFormLength = (iFormLength << 8) + (Container[2] & 0xff);
    iFormLength = (iFormLength << 8) + (Container[1] & 0xff);
    iFormLength = (iFormLength << 8) + (Container[0] & 0xff);
    return iFormLength;
}

bool isDataContainer(BYTE * Container) {
    if ((Container[0] == 'd') && (Container[1] == 'a') &&
        (Container[2] == 't') && (Container[3] == 'a')) {
        return true;
    }
    return false;
}

bool isAcidContainer(BYTE * Container) {
    if ((Container[0] == 'a') && (Container[1] == 'c') &&
        (Container[2] == 'i') && (Container[3] == 'd')) {
        return true;
    }
    return false;
}

bool isLoop(BYTE * Container) {
    if ((Container[8] == 0) || (Container[8] == 2)) {
        return true;
    }
    return false;
}

bool isOneShoot(BYTE * Container) {
    if ((Container[8] == 1) || (Container[8] == 3)) {
        return true;
    }
    return false;
}

BYTE GetAcidBeats(BYTE * Container) {
    return Container[20];
}

DWORD GetChannelData(BYTE * Buffer, unsigned channel, unsigned BitsPerSample) {
    DWORD CannelData = (Buffer[BitsPerSample / 8 - 1 + channel * BitsPerSample / 8] >= 0) ? 0. : 0xffffffff;
    for (int k = BitsPerSample / 8 - 1; k >= 0; k--) {
        CannelData = (CannelData << 8) + (Buffer[k + channel * BitsPerSample / 8] & 0xff);
    }
    return CannelData;
}

Sample32 ConvertToSample(DWORD CannelData, BYTE Type, unsigned BitsPerSample) {
    /////////Supported bitrates 8/16/24/32(IEEE Float)
    if (Type == 1) {
        if (BitsPerSample == 8) {
            return (int(CannelData)) / 127.0;
        }
        if (BitsPerSample == 16) {
            return (short(CannelData)) / 32767.0;
        }
        if (BitsPerSample == 24) {
            return (int(CannelData)) / 8388607.0;
        }
    } else if (BitsPerSample == 32) { // 32bit IEEE Float
        return *reinterpret_cast<float *>(&CannelData);
    }
    return 0.;
}

bool SampleEntry::AnalyseContainers(BYTE * Buffer, unsigned BufferSize, const char * ResourceName) {
    unsigned iFormLength = AnalyseWavForm(Buffer);
    if (iFormLength == 0) {
        fprintf(stderr,
                "[SampeEntry] Error: Wrong format (not WAVE form in %s)",
                ResourceName);
        return false;
    }

    unsigned nCannels = 0;
    unsigned iSamplesPerSec = 0;
    unsigned iBitsPerSample = 0;
    unsigned iCursor = iFormLength + 12;
    bool foundDataContainer = false;
    unsigned SoundBufferLength = 0;
    if (!AnalysePCMCodec(Buffer, nCannels, iSamplesPerSec, iBitsPerSample)) {
        fprintf(stderr,
                "[SampeEntry] Error: Wrong format (not PCM wave in %s)",
                ResourceName);
        return false;
    }

    while (iCursor < (BufferSize - 8)) {

        iFormLength = GetContainerSize(Buffer + iCursor + 4);

        if (isDataContainer(Buffer + iCursor)) {
            foundDataContainer = true;
            SoundBufferLength = iFormLength / (nCannels * iBitsPerSample / 8);
            SoundBufferLeft.resize(SoundBufferLength + 1);
            SoundBufferRight.resize(SoundBufferLength + 1);

            BYTE * Data = Buffer + iCursor + 8;
            unsigned step = nCannels * iBitsPerSample / 8;
            for (unsigned i = 0; i < iFormLength; i = i + step) {

                for (unsigned j = 0; j < nCannels; j++) {
                    DWORD CannelData = GetChannelData(Data + i, j, iBitsPerSample);
                    Sample32 FSample = ConvertToSample(CannelData, Buffer[12], iBitsPerSample);

                    // if (nCannels < 2) {
                    //     SoundBufferLeft[i / step] = FSample;
                    //     SoundBufferRight[i / step] = FSample;
                    // } else {
                        switch (j) {
                        case 0:
                            SoundBufferLeft[i / step] = FSample;
                            if (nCannels >= 2) {
                                break;
                            }
                        case 1:
                            SoundBufferRight[i / step] = FSample;
                            break;
                        default:
                            break;
                        }
                    // }
                }
            }
            SampleRate = iSamplesPerSec;
            beatLength = SoundBufferLength / defaultBeats / beatOverlapMultiple;
            beatOverlap = beatLength * beatOverlapKoef;

        }

        // TODO: analyze containers second time if ACID container is before Data
        if (isAcidContainer(Buffer + iCursor) && foundDataContainer) {
            if (isLoop(Buffer + iCursor)) {
                ACIDbeats = GetAcidBeats(Buffer + iCursor);
                beatLength = SoundBufferLength / ACIDbeats / beatOverlapMultiple;
                beatOverlap = beatLength * beatOverlapKoef;
                Loop = true;
                Sync = true;
            } else if (isOneShoot(Buffer + iCursor)) {
                ACIDbeats = GetAcidBeats(Buffer + iCursor);
                beatLength = SoundBufferLength / ACIDbeats / beatOverlapMultiple;
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
                ResourceName);
        return false;
    }
    return true;
}

// bool SampleEntry::LoadFromResource(void * hInstance,
// 	const char * ResourceName) {
// 	bool result = true;
//           BYTE * Header = (BYTE*)LoadResource(/*(HMODULE)*/hInstance, FindResource(/*(HMODULE)*/hInstance, ResourceName, 1/*RT_RCDATA*/));
// 	if (Header) {
// 		unsigned iRiffSize = AnalyseWavHeader(Header);
// 		if (iRiffSize > 0) {
// 			BYTE * Buffer = Header + 8;

// 			result = AnalyseContainers(Buffer, iRiffSize, ResourceName);
// 			if (result) {
// 				SampleFile = ResourceName;
// 			}
// 		}
// 		else {
// 			fprintf(stderr,
// 				"[SampeEntry] Error: Wrong resource format (not RIFF file in %s)",
// 				ResourceName);
// 			result = false;
// 		}
// 	}
// 	else {
// 		fprintf(stderr,
// 			"[SampeEntry] Error: Resource not found or not access(%s)",
// 			ResourceName);
// 		result = false;
// 	}
// 	return result;
// }

bool SampleEntry::LoadFromFile(const char * FileName) {
    bool result = true;
    ClearBuffer();
    BYTE *Buffer;
    BYTE Header[9];
    // String tmp(FileName);
    FILE * iFileHandle = fopen(FileName, "rb");
    if (iFileHandle) {
        //////////Reading 8Byte Header
        size_t iBytesRead = fread(Header, 1, 8, iFileHandle);
        if (iBytesRead == 8) {
            size_t iRiffSize = AnalyseWavHeader(Header);
            if (iRiffSize > 0) {
                ////////Loading content To Buffer
                Buffer = new BYTE[iRiffSize + 1];
                size_t iBytesRead = fread(Buffer, 1, iRiffSize, iFileHandle);
                fclose(iFileHandle);
                ////////Encodding Content
                if (iBytesRead == iRiffSize) {

                    result = AnalyseContainers(Buffer, iRiffSize, FileName);
                    if (result) {
                        SampleFile = FileName;
                    }

                }
                else {
                    fprintf(stderr,
                            "[SampeEntry] Error: Corrupted file(%s)",
                            FileName);
                    result = false;
                }
                delete[]Buffer;
            }
            else {
                fprintf(stderr,
                        "[SampeEntry] Error: Wrong file format (not RIFF file in %s)",
                        FileName);
                result = false;
            }
        }
        else {
            fprintf(stderr,
                    "[SampeEntry] Error: File empty or not access (%s)",
                    FileName);
            result = false;
        }
    }
    else {
        fprintf(stderr,
                "[SampeEntry] Error: File not found or not access(%s)",
                FileName);
        result = false;

    }
    return result;
}


void SampleEntry::ClearBuffer() {
    // if (SoundBufferLeft != NULL) {
    //     delete[] SoundBufferLeft;
    //     SoundBufferLeft = NULL;
    // }
    // if (SoundBufferRight != NULL) {
    //     delete[] SoundBufferRight;
    //     SoundBufferRight = NULL;
    // }
    //SoundBufferLength = 0;
    // IntegerCursor = 0;
    // FloatCursor = 0;
    SoundBufferLeft.clear();
    SoundBufferRight.clear();
    RealCursor.clear();
    OverlapCursorFirst.clear();
    Loop = false;
    Sync = false;
    Reverse = false;
    Tune = 1.0f;
    Level = 1.0f;

}

void SampleEntry::ResetCursor() {
    // IntegerCursor = 0;
    // FloatCursor = 0;
    RealCursor.clear();
    OverlapCursorFirst.clear();
    BeginLockStrobe();
}

void SampleEntry::SetName(const char * text) {
    // String tmp(text);
    // tmp.copyTo(SampleName, 0, 127);
    SampleName = text;
}

void SampleEntry::SetFileName(const char * text) {
    // String tmp(text);
    // tmp.copyTo(SampleFile, 0, 127);
    SampleFile = text;
}

const char * SampleEntry::GetName() {
    return SampleName;
}

const char * SampleEntry::GetFileName() {
    return SampleFile;
}

bool SampleEntry:: operator == (const SampleEntry & other) const {
    return (other.SoundBufferLeft == SoundBufferLeft) && (other.SoundBufferRight == SoundBufferRight);
}

bool SampleEntry:: operator != (const SampleEntry & other) const {
    return (other.SoundBufferLeft != SoundBufferLeft) || (other.SoundBufferRight != SoundBufferRight);
}

Sample32* SampleEntry::GetBuffer(channel channelname) {
    switch (channelname) {
    case kBufferLeftChannel:
        return SoundBufferLeft.data();
    case kBufferRightChannel:
        return SoundBufferRight.data();
    default:
        return 0;
    }
}

Sample32 SampleEntry::GetSample(unsigned position, channel channelname)
{
    if (position < SoundBufferRight.size()) {
        switch (channelname) {
        case kBufferLeftChannel:
            return SoundBufferLeft[position];
        case kBufferRightChannel:
            return SoundBufferRight[position];
        default:
            return 0;
        }

    }
    return 0;
}

Sample32 SampleEntry::GetAvgSample(unsigned position) {
    if ((size_t)position < SoundBufferLeft.size()) {
        return (SoundBufferLeft[position] +
                SoundBufferRight[position]) / 2.0;
    }
    return 0;
}

Sample32 SampleEntry::GetPeakSample(unsigned from_position,
                                    unsigned to_position) {
    Sample32 peak = 0;
    if (from_position > SoundBufferLeft.size()) {
        return 0;
    }
    if (to_position > SoundBufferLeft.size()) {
        to_position = SoundBufferLeft.size();
    }
    if (from_position > to_position) {
        unsigned tmp = to_position;
        to_position = from_position;
        from_position = tmp;
    }

    for (int i = (int)from_position; i < (int)to_position; i++) {
        if (peak < fabs(GetAvgSample(i))) {
            peak = fabs(GetAvgSample(i));

        }
    }
    return peak;
}

unsigned SampleEntry::GetBufferLength() {
    return SoundBufferLeft.size();
}

Sample64 SampleEntry::GetNoteLength(Sample64 _note, Sample64 _Tempo) {
    if ((Sync) && (ACIDbeats > 0)) {
        return SoundBufferLeft.size() / ACIDbeats * _note;
    }
    else if (_Tempo > 0) {
        return (Sample64)SampleRate / _Tempo * 60.0 * _note;
    }
    return 0;

}

Sample64 SampleEntry::GetNoteLengthEnv(Sample64 _note, Sample64 _Tempo, Sample64 _SampleRate) {
    return SampleRate>0?GetNoteLength(_note,_Tempo)*(_SampleRate/(Sample64)SampleRate):0;

}

Sample64 SampleEntry::RecalcSpeed(Sample64 _Speed, Sample64 _Tempo, Sample64 _SampleRate)
{

    return Sync?CalcTempoSpeed(_Speed,_Tempo,_SampleRate):CalcRealSpeed(_Speed,_SampleRate);
}

Sample64 SampleEntry::CalcRealSpeed(Sample64 _Speed, Sample64 _SampleRate)
{
    Sample64 dir = 1;
    if (Reverse) dir = -1;

    if (_SampleRate > 0) {
        return  _Speed * Tune * ((Sample64)SampleRate / (Sample64)_SampleRate) * dir;
    } else return 0;
}

Sample64 SampleEntry::CalcTempoSpeed(Sample64 _Speed, Sample64 _Tempo, Sample64 _SampleRate)
{
    Sample64 dir = Sign(_Speed);
    if (Reverse) dir = -Sign(_Speed);
    if ((ACIDbeats > 0) && (_SampleRate > 0)) {

        return dir * SoundBufferLeft.size() * _Tempo / 60.0 / _SampleRate / ACIDbeats;
    }else
        if (_SampleRate > 0) {
            return dir * SoundBufferLeft.size() * _Tempo / 60.0 / _SampleRate / defaultBeats;
        }
    return CalcRealSpeed(_Speed,_SampleRate);
}


void SampleEntry::PlayStereoSample(Sample32 * Left, Sample32 * Right,
                                   Sample64 _Speed, Sample64 _Tempo, Sample64 _SampleRate,
                                   bool _changeCursors) {

    //if (holdSize!=0) {

    //Sample64 newSpeed = RecalcSpeed(_Speed, _Tempo, _SampleRate);
    if ((Sync)&&(ACIDbeats>0)) {
        PlayStereoSampleTempo(Left, Right, _Speed * ((Sample64)SampleRate / (Sample64)_SampleRate) ,fabs(_Tempo * _Speed),_SampleRate,_changeCursors);
    }else{
        PlayStereoSample(Left, Right, CalcRealSpeed(_Speed,_SampleRate), _changeCursors);
    }


}

void SampleEntry::PlayStereoSampleTempo(Sample32 * Left, Sample32 * Right,
                                        Sample64 _Speed, Sample64 _Tempo, Sample64 _SampleRate,
                                        bool _changeCursors) {
    //bool syncroStrob;
    Sample64 newSpeed = CalcRealSpeed(_Speed, _SampleRate);
    Sample64 newTempoSpeed = CalcTempoSpeed(_Speed, _Tempo, _SampleRate);
    //bool plus = false;
    //bool plusp = false;
    //bool minus = false;
    //bool minusp = false;
    CuePoint PushCue = CalcNewCursor(newTempoSpeed);
    CuePoint PushCue2 = RealCursor;
    //CalcNewCursor(newTempoSpeed, PushCue);
    //PushCue += newTempoSpeed;
    //NormalizeCue(PushCue);
    //if (Reverse) {
    //	newSpeed = -newSpeed;
    //	newTempoSpeed = -newTempoSpeed;
    //}

    if ((newSpeed / newTempoSpeed) <= 1.3) {
        if (CheckOverlapEvent(PushCue, newTempoSpeed) && (SmoothOverlap <= 0.00005)) {
            OverlapStrobe(newTempoSpeed,fabs(newSpeed));
            // plus = true;
        }
        //else if (CheckSyncroEvent(PushCue,newTempoSpeed)) {SyncStrobe();minus = true;}
        else  {
            RealCursor = OverlapCursorFirst;
        }
    } else if ((newSpeed / newTempoSpeed) > 1.3) {
        if (CheckStratchEvent(PushCue, newSpeed, newTempoSpeed) && (SmoothOverlap <= 0.00005)) {
            StratchStrobe(PushCue);
            /*plusp = true;*/
        }
        RealCursor = OverlapCursorFirst;
    }

    PlayStereoSample(Left, Right, newSpeed, true);
    OverlapCursorFirst = RealCursor;


    if (SmoothOverlap > 0.0) {
        RealCursor = OverlapCursorSecond;
        Sample32 OverlapLeft;
        Sample32 OverlapRight;
        PlayStereoSample(&OverlapLeft, &OverlapRight, newSpeed, true);
        OverlapCursorSecond = RealCursor;
        Sample32 CorrectorCoef = (1. - cos(SmoothOverlap * 2. * Pi)) / 10.;

        *Left = *Left * (SmoothOverlap + CorrectorCoef) + OverlapLeft * (1. - SmoothOverlap + CorrectorCoef);
        *Right = *Right * (SmoothOverlap + CorrectorCoef) + OverlapRight * (1. - SmoothOverlap + CorrectorCoef);
        OverlapCursorSecond = RealCursor;
        //if (SmoothOverlap<=0.0000001) {
        //		SmoothOverlap = 0;
        //}else{
        //SmoothOverlap -= 0.01;
        if ((newSpeed / newTempoSpeed) <= 1.3) {
            SmoothOverlap -= (fabs(newTempoSpeed / (double(beatOverlap) * newSpeed)));
            if (SmoothOverlap < 0.00001) {
                OverlapCursorFirst = OverlapCursorSecond;
                SmoothOverlap = -1;
            }
        } else {
            SmoothOverlap -= (2.3 / double(beatOverlap));
            if (SmoothOverlap < 0.00001) {
                //OverlapCursorFirst = OverlapCursorSecond;
                SmoothOverlap = 0.00001;
            }
            //FaderProcess = false;
        }
        //SmoothOverlap = 99.0 * SmoothOverlap/100.0;
        //}
    }
    //else {
    //	SmoothOverlap = 0;
    //	OverlapCursorSecond = OverlapCursorFirst;
    //}
    /*if (plus) {
				   *Left = 0.98;
				}
				if (minus) {
				   *Left = -0.98;
				}
				if (plusp) {
				   *Left = -0.8;
				}
				if (minusp) {
				   *Right = 0.8;
				}*/

    RealCursor = _changeCursors ? PushCue : PushCue2;
}

void SampleEntry::SyncStrobe() {

    OverlapCursorFirst = OverlapCursorSecond;
    RealCursor = OverlapCursorSecond;
    SmoothOverlap = -1;
}

void SampleEntry::BeginLockStrobe() {
    OverlapCursorFirst = RealCursor;
    SmoothOverlap = -1;
}

void SampleEntry::OverlapStrobe(double speed, double tune) {
    if (speed < 0) {
        OverlapCursorSecond.set(beatLength * NormalizeStretchBeat(long(RealCursor.integerPart() / beatLength)) - beatOverlap * tune / speed, 0);
    } else {
        OverlapCursorSecond.set((long)beatLength * (long)NormalizeStretchBeat(long(RealCursor.integerPart() / beatLength) + 1) - beatOverlap * tune / speed, 0);
    }

    RealCursor = NormalizeCue(OverlapCursorFirst);
    SmoothOverlap = 1;
}

void SampleEntry::StratchStrobe(const CuePoint &cue) {
    OverlapCursorFirst = OverlapCursorSecond;
    OverlapCursorSecond = cue;
    SmoothOverlap = 1;
}


bool SampleEntry::CheckStratchEvent(CuePoint &cue, Sample64 speed,Sample64 speedtempo){
    if ((speedtempo > 0) && (cue > OverlapCursorSecond)) {
        if (fabs(OverlapCursorSecond.integerPart() + SoundBufferLeft.size() - cue.integerPart()) > fabs((double)beatLength/2.0)) {
            return true;
        }
    } else if ((speedtempo < 0) && (cue < OverlapCursorSecond)) {

        if (fabs(OverlapCursorSecond.integerPart() - cue.integerPart() + SoundBufferLeft.size()) > fabs((double)beatLength/2.0)) {
            return true;
        }

    } else {
        if (fabs(OverlapCursorSecond.integerPart() - cue.integerPart()) > fabs(double(beatLength) / 2.0)) {
            return true;
        }
    }
    return false;
}

/*bool SampleEntry::CheckStratchEvent(CuePoint &_cue,Sample64 _offset){
			bool result = false;
			CuePoint _cueNew(_cue);
			_cueNew += _offset;
			NormalizeCue(_cueNew);
			if (((_offset>0)&&(_cue > _cueNew))||((_offset<0)&&(_cue < _cueNew))) {
				   result = false;
			}else
			if (((long)((_cueNew.IntegerPart  + beatLength/2) / beatLength))!=((long)((_cue.IntegerPart + beatLength/2) / beatLength))) {
					result = true;
			}
			return result;
		}*/

bool SampleEntry::CheckRestratchEvent(const CuePoint &cue, Sample64 offset){

    CuePoint cueNew = NormalizeCue(cue + offset);
    // cueNew += offset;
    // NormalizeCue(cueNew);
    if ((long(cueNew.integerPart() / beatLength)) != (long(cue.integerPart() / beatLength))) {
        return true;
    }
    if (((offset > 0) && (cue > cueNew)) || ((offset < 0) && (cue < cueNew))) {
        return true;
    }
    return false;
}

bool SampleEntry::CheckOverlapEvent(CuePoint &cue, Sample64 offset){

    if (((offset > 0) && (cue < RealCursor))||((offset < 0) && (cue > RealCursor))) {
        return false;
    }
    if ((long((RealCursor.integerPart()  + beatOverlap) / beatLength)) != (long((cue.integerPart() + beatOverlap) / beatLength))) {
        return true;
    }
    return false;
}

bool SampleEntry::CheckSyncroEvent(CuePoint &cue,Sample64 offset){

    if ((long(RealCursor.integerPart() / beatLength)) != (long(cue.integerPart() / beatLength))) {
        return true;
    }
    if (((offset > 0) && (cue < RealCursor)) || ((offset < 0) && (cue > RealCursor))) {
        return true;
    }
    return false;
}

void SampleEntry::MoveCursorEnv(Sample64 offset, Sample64 tempo, Sample64 sampleRate) {
    MoveCursor(RecalcSpeed(offset, tempo, sampleRate));
}

SampleEntry::CuePoint SampleEntry::CalcNewCursor(Sample64 _offset)
{

    CuePoint _newCursor(RealCursor);

    Sample64 CurrentSpeed;
    //if (Reverse)
    //	CurrentSpeed = -_offset;
    //else
    CurrentSpeed = _offset;

    if (!Loop) {
        if ((_newCursor.integerPart() == SoundBufferLeft.size() - 1) && (CurrentSpeed > 0)) {
            return _newCursor;
        }
        if ((_newCursor.integerPart() == 0) && (CurrentSpeed < 0)) {
            return _newCursor;
        }
    }

    _newCursor += CurrentSpeed;
    return NormalizeCue(_newCursor);
}

SampleEntry::CuePoint& SampleEntry::NormalizeCue(CuePoint& cue){

    if (Loop 
        || ((cue.integerPart() >= 0) && (cue.integerPart() < int64_t(SoundBufferLeft.size())))) {

        if (cue.integerPart() >= int64_t(SoundBufferLeft.size())) {
            cue.set(cue.integerPart() % SoundBufferLeft.size(), cue.floatPart());
        }
        if (cue.integerPart() < 0) {
            cue.set(SoundBufferLeft.size() - (cue.integerPart() % SoundBufferLeft.size()), cue.floatPart());
        }

    } else if (cue.integerPart() >= int64_t(SoundBufferLeft.size())) {
        cue.set(SoundBufferLeft.size() - 1, 0.);
    } else if (cue.integerPart() < 0) {
        cue.clear();
    }
    return cue;
}

SampleEntry::CuePoint SampleEntry::NormalizeCue(const CuePoint& cue) {
    CuePoint ret(cue);
    if (Loop 
        || ((ret.integerPart() >= 0) && (ret.integerPart() < int64_t(SoundBufferLeft.size())))) {

        if (ret.integerPart() >= int64_t(SoundBufferLeft.size())) {
            ret.set(ret.integerPart() % SoundBufferLeft.size(), cue.floatPart());
        }
        if (ret.integerPart() < 0) {
            ret.set(SoundBufferLeft.size() - (ret.integerPart() % SoundBufferLeft.size()), cue.floatPart());
        }

    } else if (ret.integerPart() >= int64_t(SoundBufferLeft.size())) {
        ret.set(SoundBufferLeft.size() - 1, 0.);
    } else if (ret.integerPart() < 0) {
        ret.clear();
    }
    return ret;
}


bool SampleEntry::MoveCursor(Sample64 offset) {
    if (SoundBufferLeft.size() >= 4) {
        RealCursor = CalcNewCursor(offset);
        return true;
    }
    return false;

}

void SampleEntry::PlayStereoSample(Sample32 * Left, Sample32 * Right,
                                   Sample64 _offset, bool _changeCursors) {

    if (SoundBufferLeft.size() >= 4) {
        /// need 3 point minimum for hermite interpolation
        CuePoint NewCursor = CalcNewCursor(_offset);

        ////////////////////////Play loop//////////////////////////
        Sample64 Point0 = 0;
        Sample64 Point1 = SoundBufferLeft[NewCursor.integerPart()];
        Sample64 Point2 = 0;
        Sample64 Point3 = 0;
        if (NewCursor.integerPart() > 0) {
            Point0 = SoundBufferLeft[NewCursor.integerPart() - 1];
        }
        if (NewCursor.integerPart() < int64_t(SoundBufferLeft.size()) - 2) {
            Point2 = SoundBufferLeft[NewCursor.integerPart() + 1];
        }
        if (NewCursor.integerPart() < int64_t(SoundBufferLeft.size()) - 3) {
            Point3 = SoundBufferLeft[NewCursor.integerPart() + 2];
        }
        *Left = Level * hermite(NewCursor.floatPart(), Point0, Point1, Point2, Point3);

        Point0 = 0;
        Point1 = SoundBufferRight[NewCursor.integerPart()];
        Point2 = 0;
        Point3 = 0;
        if (NewCursor.integerPart() > 0) {
            Point0 = SoundBufferRight[NewCursor.integerPart() - 1];
        }
        if (NewCursor.integerPart() < int64_t(SoundBufferRight.size()) - 2) {
            Point2 = SoundBufferRight[NewCursor.integerPart() + 1];
        }
        if (NewCursor.integerPart() < int64_t(SoundBufferRight.size()) - 3) {
            Point3 = SoundBufferRight[NewCursor.integerPart() + 2];
        }
        *Right = Level * hermite(NewCursor.floatPart(), Point0, Point1,
                                 Point2, Point3);
        ///////////////////////////////////////////////////////////

        if (_changeCursors) {
            RealCursor = NewCursor;
        }

    } else {
        *Left = 0;
        *Right = 0;
    }
}

/*void SampleEntry::Hold(long iterationCount){
			if (iterationCount>=0) {
				holdSize = iterationCount;
				holdCue = Cursor;
				holdIterationCount = 0;
			}else if (iterationCount<0) {
                     holdSize = -1;
				  }

		}

		void SampleEntry::Resume(){
			holdSize = -1;
		}*/

void SampleEntry::setCursorOnBeat(unsigned beatNumber){
    //RealCursor.set(RealCursor.integerPart(), 0.);
    if (ACIDbeats == 0) {
        RealCursor.clear();
        currentBeat = 0;
    } else {
        if (beatNumber > ACIDbeats) {
            beatNumber = ACIDbeats - 1;
        }
        RealCursor.set((SoundBufferLeft.size() / ACIDbeats) * beatNumber, 0.);
        currentBeat = beatNumber;
    }
}

unsigned SampleEntry::setNextBeat(short direction){
    int new_beat = 0;
    if (ACIDbeats > 0) {

        if (direction > 0) {
            new_beat = int(currentBeat) + 1;
        }
        if (direction < 0) {
            new_beat = int(currentBeat) - 1;
        }
        if (new_beat < 0) {
            new_beat = Loop ? (int(ACIDbeats) - 1) : 0;
        }
        if (new_beat >= (int)ACIDbeats) {
            new_beat = Loop ? 0 : (int(ACIDbeats) - 1);
        }

    }
    setCursorOnBeat(new_beat);
    return currentBeat;

}
void SampleEntry::setFirstBeat(void){
    setCursorOnBeat(0);
}

void SampleEntry::setLastBeat(void){
    if (ACIDbeats > 0) {
        setCursorOnBeat(ACIDbeats - 1);
    } else {
        setCursorOnBeat(0);
    }
}

unsigned SampleEntry::NormalizeBeat(long _Beat){
    if (ACIDbeats>0) {
        if (Loop) {
            return (_Beat >= ACIDbeats)?(_Beat % ACIDbeats):((_Beat < 0)?(ACIDbeats + (_Beat % ACIDbeats)):((unsigned) _Beat));
        }else{
            return (_Beat >= ACIDbeats)?ACIDbeats-1:0;
        }
    }else
        if (defaultBeats>0) {
            if (Loop) {
                return (_Beat >= defaultBeats)?(_Beat % (int)defaultBeats):((_Beat < 0)?(defaultBeats + (_Beat % (int)defaultBeats)):((unsigned) _Beat));
            }else{
                return (_Beat >= defaultBeats)?defaultBeats-1:0;
            }
        }else  return 0;
}

unsigned SampleEntry::NormalizeStretchBeat(long beat){
    if (ACIDbeats > 0) {
        if (Loop) {
            return (beat >= int64_t(ACIDbeats * beatOverlapMultiple)) ? (beat % (ACIDbeats * beatOverlapMultiple)) : ((beat < 0) ? ((ACIDbeats * beatOverlapMultiple) + (beat % (ACIDbeats * beatOverlapMultiple))) : unsigned(beat));
        } else {
            return (beat >= int64_t(ACIDbeats * beatOverlapMultiple)) ? ACIDbeats * beatOverlapMultiple - 1 : 0;
        }
    }
    if (defaultBeats > 0) {
        if (Loop) {
            return (beat >= int64_t(defaultBeats * beatOverlapMultiple)) ? (beat % int(defaultBeats * beatOverlapMultiple)) : ((beat < 0) ? (defaultBeats * beatOverlapMultiple + (beat % int(defaultBeats * beatOverlapMultiple))) : unsigned(beat));
        } else {
            return (beat >= int64_t(defaultBeats * beatOverlapMultiple)) ? defaultBeats * beatOverlapMultiple - 1 : 0;
        }
    }
    return 0;
}

void fastsinetransform(Sample32 *a, int tnn) {
    int jj;
    int j;
    int tm;
    int n2;
    Sample32 sum;
    Sample32 y1;
    Sample32 y2;
    Sample32 theta;
    Sample32 wi;
    Sample32 wr;
    Sample32 wpi;
    Sample32 wpr;
    Sample32 wtemp;
    Sample32 twr;
    Sample32 twi;
    Sample32 twpr;
    Sample32 twpi;
    Sample32 twtemp;
    Sample32 ttheta;
    int i;
    int i1;
    int i2;
    int i3;
    int i4;
    Sample32 c1;
    Sample32 c2;
    Sample32 h1r;
    Sample32 h1i;
    Sample32 h2r;
    Sample32 h2i;
    Sample32 wrs;
    Sample32 wis;
    int nn;
    int ii;
    int n;
    int mmax;
    int m;
    int istep;
    int isign;
    Sample32 tempr;
    Sample32 tempi;

    if (tnn == 1) {
        a[0] = 0;
        return;
    }
    theta = Pi / tnn;
    wr = 1.0;
    wi = 0.0;
    wpr = -2.0 * sqr(sin(0.5 * theta));
    wpi = sin(theta);
    a[0] = 0.0;
    tm = tnn / 2;
    n2 = tnn + 2;
    for (j = 2; j <= tm + 1; j++) {
        wtemp = wr;
        wr = wr * wpr - wi * wpi + wr;
        wi = wi * wpr + wtemp * wpi + wi;
        y1 = wi * (a[j - 1] + a[n2 - j - 1]);
        y2 = 0.5 * (a[j - 1] - a[n2 - j - 1]);
        a[j - 1] = y1 + y2;
        a[n2 - j - 1] = y1 - y2;
    }
    ttheta = 2 * Pi / tnn;
    c1 = 0.5;
    c2 = -0.5;
    isign = 1;
    n = tnn;
    nn = tnn / 2;
    j = 1;
    for (ii = 1; ii <= nn; ii++) {
        i = 2 * ii - 1;
        if (j > i) {
            tempr = a[j - 1];
            tempi = a[j];
            a[j - 1] = a[i - 1];
            a[j] = a[i];
            a[i - 1] = tempr;
            a[i] = tempi;
        }
        m = n / 2;
        while (m >= 2 && j > m) {
            j = j - m;
            m = m / 2;
        }
        j = j + m;
    }
    mmax = 2;
    while (n > mmax) {
        istep = 2 * mmax;
        theta = 2 * Pi / (isign * mmax);
        wpr = -2.0 * sqr(sin(0.5 * theta));
        wpi = sin(theta);
        wr = 1.0;
        wi = 0.0;
        for (ii = 1; ii <= mmax / 2; ii++) {
            m = 2 * ii - 1;
            for (jj = 0; jj <= (n - m) / istep; jj++) {
                i = m + jj * istep;
                j = i + mmax;
                tempr = wr * a[j - 1] - wi * a[j];
                tempi = wr * a[j] + wi * a[j - 1];
                a[j - 1] = a[i - 1] - tempr;
                a[j] = a[i] - tempi;
                a[i - 1] = a[i - 1] + tempr;
                a[i] = a[i] + tempi;
            }
            wtemp = wr;
            wr = wr * wpr - wi * wpi + wr;
            wi = wi * wpr + wtemp * wpi + wi;
        }
        mmax = istep;
    }
    twpr = -2.0 * sqr(sin(0.5 * ttheta));
    twpi = sin(ttheta);
    twr = 1.0 + twpr;
    twi = twpi;
    for (i = 2; i <= tnn / 4 + 1; i++) {
        i1 = i + i - 2;
        i2 = i1 + 1;
        i3 = tnn + 1 - i2;
        i4 = i3 + 1;
        wrs = twr;
        wis = twi;
        h1r = c1 * (a[i1] + a[i3]);
        h1i = c1 * (a[i2] - a[i4]);
        h2r = -c2 * (a[i2] + a[i4]);
        h2i = c2 * (a[i1] - a[i3]);
        a[i1] = h1r + wrs * h2r - wis * h2i;
        a[i2] = h1i + wrs * h2i + wis * h2r;
        a[i3] = h1r - wrs * h2r + wis * h2i;
        a[i4] = -h1i + wrs * h2i + wis * h2r;
        twtemp = twr;
        twr = twr * twpr - twi * twpi + twr;
        twi = twi * twpr + twtemp * twpi + twi;
    }
    h1r = a[0];
    a[0] = h1r + a[1];
    a[1] = h1r - a[1];
    sum = 0.0;
    a[0] = 0.5 * a[0];
    a[1] = 0.0;
    for (jj = 0; jj <= tm - 1; jj++) {
        j = 2 * jj + 1;
        sum = sum + a[j - 1];
        a[j - 1] = a[j];
        a[j] = sum;
    }

}

int bitNumber(int _bitSet)
{
    int testBit = 1;
    if (_bitSet == 0) {
        return -1;
    }
    for (int i = 0; i < 32; i++) {
        if (_bitSet & testBit) {
            return i;
        }
        testBit = testBit<<1;
    }
    return -1;
}


/*void SampleEntry::PlayBeatNumberEnv(Sample32 *Left, Sample32 *Right,
				Sample64 _Speed,Sample64 _Tempo, Sample64 _SampleRate,bool _changeCursors){
			if (Cursor<beatCueEnd) {
				PlayStereoSampleEnv(Left,Right,_Speed,_Tempo,_SampleRate,_changeCursors);
			}else{
				Left = 0;
                Right = 0;
            }

		}*/


}
} // namespaces
