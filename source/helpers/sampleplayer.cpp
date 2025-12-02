//---------------------------------------------------------------------------

#pragma hdrstop

#include "sampleplayer.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
namespace Steinberg {
	namespace Vst {

		SamplePlayer::SamplePlayer(double _tempo,double _samplerate):
			SampleRate(_samplerate)
			,Tempo(_tempo)
			{
            //do nothing
		}

		SamplePlayer::~SamplePlayer() {
			//do nothing
		}

		void SamplePlayer::VSTPlayStereoSample(SampleEntry * _sample, float * _left, float *_right, double _speed){

			_sample->PlayStereoSample(_left,_right, _speed, Tempo, SampleRate,true);

		}


	}
}
