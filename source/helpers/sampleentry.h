#pragma once

#include "base/source/fstring.h"
#include "pluginterfaces/vst/vsttypes.h"

#include <math.h>

// ---------------------------------------------------------------------------
namespace Steinberg {
	namespace Vst {

		typedef char BYTE;
		typedef unsigned int DWORD;
		const Sample32 Pi = 3.14159265358979323846264338327950288;
		const Sample32 defaultBeats = 8.;
		const Sample32 beatOverlapKoef = 1./2.;
		const unsigned beatOverlapMultiple = 8;

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

		class CuePoint {
		public:
			CuePoint() {IntegerPart = 0;FloatPart=0;}
			CuePoint(long _integer,double _float) {IntegerPart = _integer;FloatPart=_float;}
			void SetCue(long _integer,double _float) {IntegerPart = _integer;FloatPart=_float;}
			void SetCue(CuePoint _cue) {IntegerPart = _cue.IntegerPart;FloatPart=_cue.FloatPart;}

			CuePoint& operator = (CuePoint& _cue){IntegerPart = _cue.IntegerPart;FloatPart=_cue.FloatPart; return *this;}
			CuePoint& operator = (double _offset){IntegerPart = (long) _offset;FloatPart=_offset - (double)IntegerPart; return *this;}
			CuePoint& operator = (long _offset){IntegerPart = _offset;FloatPart=0; return *this;}
			bool operator > (CuePoint& _cue){if ((IntegerPart > _cue.IntegerPart)||((IntegerPart == _cue.IntegerPart)&&(FloatPart > _cue.FloatPart))) return true; return false;}
			bool operator < (CuePoint& _cue){if ((IntegerPart < _cue.IntegerPart)||((IntegerPart == _cue.IntegerPart)&&(FloatPart < _cue.FloatPart))) return true; return false;}
			bool operator == (CuePoint& _cue){if ((IntegerPart == _cue.IntegerPart)&&(FloatPart == _cue.FloatPart)) return true; return false;}
			bool operator >= (CuePoint& _cue){if ((*this > _cue)||(*this == _cue)) return true; return false;}
			bool operator <= (CuePoint& _cue){if ((*this < _cue)||(*this == _cue)) return true; return false;}
			CuePoint& operator += (double _offset){FloatPart+=_offset;Normalize();return *this;}
			CuePoint& operator -= (double _offset){FloatPart-=_offset;Normalize();return *this;}
			void Normalize() {if (FloatPart>1.0) {
				   IntegerPart += (long)FloatPart;
				   FloatPart = FloatPart - (long)FloatPart;
				}
				if (FloatPart<0) {
				   IntegerPart += ((long)FloatPart - 1);
				   FloatPart = 1 + (FloatPart - (long)FloatPart);
				}}
			long IntegerPart;
			Sample64 FloatPart;
			Sample64 GetPointDouble(){return (Sample64)IntegerPart + FloatPart;}
			void Clear() {IntegerPart = 0;FloatPart=0;}
			//CuePoint GetOffsetFrom(CuePoint _cue);
		};

		class SampleEntry {
		private:
			Sample32* SoundBufferLeft;
			Sample32* SoundBufferRight;
			long SoundBufferLength;
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
		protected:
			unsigned AnalyseWavHeader(BYTE * Header);
			unsigned AnalyseWavForm(BYTE * Form);
			bool AnalyseContainers(BYTE * Buffer,unsigned _size,const char * ResourceName);
			bool AnalysePCMCodec(BYTE * Form,unsigned & nCannels,unsigned & iSamplesPerSec,unsigned & iBitsPerSample);
			unsigned GetContainerSize(BYTE * Container);
			bool isDataContainer(BYTE * Container);
			bool isAcidContainer(BYTE * Container);
			bool isLoop(BYTE * Container);
			bool isOneShoot(BYTE * Container);
			BYTE GetAcidBeats(BYTE * Container);
			DWORD GetChannelData(BYTE * Buffer,unsigned channel,unsigned BitsPerSample);
			Sample32 ConvertToSample(DWORD CannelData,BYTE Type,unsigned BitsPerSample);
			bool CheckSyncroEvent(CuePoint &_cue,Sample64 _offset);
			bool CheckOverlapEvent(CuePoint &_cue,Sample64 _offset);
			bool CheckStratchEvent(CuePoint &_cue,Sample64 _speed,Sample64 _speedtempo);
			bool CheckRestratchEvent(CuePoint &_cue,Sample64 _offset);
			void OverlapStrobe(double _Speed,double _Tune);
			void StratchStrobe(CuePoint &_cue);
			void SyncStrobe();
			unsigned NormalizeStretchBeat(long _Beat);
			//void RestratchStrobe();
		public:
			enum channel{
				kBufferLeftChannel = 0,
				kBufferRightChannel
			};
            //SampleEntry();
            //SampleEntry(const char * Name);
            explicit SampleEntry(const char *  Name = nullptr, const char *  FileName = nullptr);
            //SampleEntry(void * hInstance, const char *  ResourceName);
			~SampleEntry();

			void SetName(const char *  text);
			void SetFileName(const char *  text);
			const char * GetName();
			const char * GetFileName();
			bool LoadFromFile(const char *  FileName);
            //bool LoadFromResource(void * hInstance, const char *  ResourceName);
			void ResetCursor();
			bool MoveCursor(Sample64 _offset);
			void SetCue(CuePoint _cue) {RealCursor=_cue;NormalizeCue(RealCursor);OverlapCursorFirst = RealCursor;}
			void NormalizeCue(CuePoint &_cue);
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
			void CalcNewCursor(Sample64 _offset,CuePoint& _newCursor);
			void MoveCursorEnv(Sample64 _offset, Sample64 _Tempo, Sample64 _SampleRate);
			Sample64 GetNoteLength(Sample64 _note, Sample64 _Tempo);
			Sample64 GetNoteLengthEnv(Sample64 _note, Sample64 _Tempo, Sample64 _SampleRate);
			Sample32* GetBuffer(channel channelname);
			Sample32 GetSample(unsigned position,channel channelname);
			Sample32 GetAvgSample(unsigned position);
			Sample32 GetPeakSample(unsigned from_position,unsigned to_position);
			Sample64 GetTempo() {return ACIDbeats>0?60.0/((Sample64)SoundBufferLength/(Sample64)SampleRate) * ACIDbeats:60.0/((Sample64)SoundBufferLength/(Sample64)SampleRate)*defaultBeats;}
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
		};

		inline bool between(long int MinValue, long int Value,
			long int MaxValue) {
			if (MinValue < MaxValue) {
				if ((MinValue <= Value) && (Value < MaxValue)) {
					return true;
				}
				else {
					return false;
				}
			}
			else {
				if ((MinValue >= Value) && (Value > MaxValue)) {
					return true;
				}
				else {
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

        inline Sample64 noteLengthInSamples(Sample64 _note, Sample64 _Tempo, Sample64 _SampleRate)
		{
			if ((_Tempo>0)&&(_note>0)) {
				return  _SampleRate / _Tempo * 60.0 * _note;
			}
			return 0;

		}


	}
}
