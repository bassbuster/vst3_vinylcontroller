#pragma once

#include "sampleentry.h"
//---------------------------------------------------------------------------
namespace Steinberg {
	namespace Vst {
		class SamplePlayer {
		private:
			//double NoteLength;
		public:
			double SampleRate;
			double Tempo;

			SamplePlayer(double _tempo,double _samplerate);
			~SamplePlayer();

			void ChangeDefaultTempo(double _tempo) {Tempo = _tempo;}
			void ChangeDefaultSampleRate(double _samplerate) {SampleRate = _samplerate;}

			void VSTPlayStereoSample(SampleEntry * _sample, float * _left, float *_right, double _speed);

		};

	}
}
