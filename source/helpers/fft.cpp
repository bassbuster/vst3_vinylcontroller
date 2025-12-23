#include "fft.h"

#include <emmintrin.h>	// sse2

namespace Steinberg::Vst {


constexpr float Pi = 3.1415926535897932384626433832;


void fft_simd(Complex<double> *X, size_t N)
{
    // Notes	: the length of fft must be a power of 2,and it is  a in-place algorithm
    // ref		: https://en.wikipedia.org/wiki/Cooley%E2%80%93Tukey_FFT_algorithm

    auto separate = [](Complex<double>* a, size_t n) {
        Complex<double>* b = new Complex<double>[n / 2];				// get temp heap storage
        for (size_t i = 0; i < n / 2; i++)	{	// copy all odd elements to heap storage
            b[i] = a[i * 2 + 1];
        }
        for (size_t i = 0; i < n / 2; i++)	{	// copy all even elements to lower-half of a[]
            a[i] = a[i * 2];
        }
        for (size_t i = 0; i < n / 2; i++)	{	// copy all odd (from heap) to upper-half of a[]
            a[i + n / 2] = b[i];
        }
        delete[] b;							// delete heap storage
    };

    if (N < 2) {
        // bottom of recursion.
        // Do nothing here, because already X[0] = x[0]
    } else {
        separate(X, N);							// all evens to lower half, all odds to upper half
        fft_simd(X, N / 2);					// recurse even items
        fft_simd(X + N / 2, N / 2);			// recurse odd  items
            // combine results of two half recursions
        for (int k = 0; k < N / 2; k++) {

            //__m128d e = _mm_load_pd( (double *)&X[k] );   // even
            __m128d o = _mm_load_pd((double *)&X[k + N/2]);   // odd
            double cc = cos(-2. * Pi * k / N);
            double ss = sin(-2. * Pi * k / N);
            // Thanks for the improvement from @htltdco. Now we can use fewer instructions. \
            // reference from: https://github.com/jagger2048/fft_simd/issues/1
            __m128d wr = _mm_load1_pd(&cc);			//__m128d wr =  _mm_set_pd( cc,cc );		// cc
            __m128d wi = _mm_set_pd(ss, -ss);		// -d | d	, note that it is reverse order
            // compute the w*o
            wr = _mm_mul_pd(o, wr);					// ac|bc
            __m128d n1 = _mm_shuffle_pd(o, o, _MM_SHUFFLE2(0, 1)); // invert
            wi = _mm_mul_pd(n1, wi);				// -bd|ad
            n1 = _mm_add_pd(wr, wi);				// ac-bd|bc+ad

            o = _mm_load_pd((double *)&X[k]);		// load even part
            wr = _mm_add_pd(o, n1);					// compute even part, X_e + w * X_o;
            wi = _mm_sub_pd(o, n1);					// compute odd part,  X_e - w * X_o;
            _mm_store_pd((double *)&X[k], wr);
            _mm_store_pd((double *)&X[k + N / 2], wi);
        }
    }
}
}
