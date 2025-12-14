#pragma once

#include <emmintrin.h>	// sse2


namespace Steinberg::Vst {


struct Complex {
    double real =0;
    double imaginary = 0;
};

void fft2_simd(Complex* X, int N);

void fastsinetransform(float* a, int tnn);

}
