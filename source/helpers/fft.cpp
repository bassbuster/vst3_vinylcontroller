#include "fft.h"

#include <emmintrin.h>	// sse2

namespace Steinberg::Vst {


constexpr float Pi = 3.1415926535897932384626433832;

// inline float sqr(int x) {
//     return x * x;
// }

// inline float sqr(float x) {
//     return x * x;
// }

// inline double sqr(double x) {
//     return x * x;
// }

// void fastsinetransform(float *a, size_t len) {

//     if (len == 1) {
//         a[0] = 0;
//         return;
//     }
//     float theta = Pi / len;
//     float ttheta = 2.f * Pi / len;
//     float wr = 1.;
//     float wi = 0.;
//     float wpr = -2.f * sqr(sin(0.5f * theta));
//     float wpi = sin(theta);
//     a[0] = 0.0;
//     size_t halfLen = len / 2;
//     size_t n2 = len + 2;
//     for (size_t j = 2; j <= halfLen + 1; j++) {
//         float wtemp = wr;
//         wr = wr * wpr - wi * wpi + wr;
//         wi = wi * wpr + wtemp * wpi + wi;
//         float y1 = wi * (a[j - 1] + a[n2 - j - 1]);
//         float y2 = 0.5f * (a[j - 1] - a[n2 - j - 1]);
//         a[j - 1] = y1 + y2;
//         a[n2 - j - 1] = y1 - y2;
//     }
//     const float c1 = 0.5;
//     const float c2 = -0.5;
//     const float isign = 1.;
//     //n = len;
//     //nn = len / 2;
//     size_t j = 1;
//     for (size_t ii = 1; ii <= halfLen; ii++) {
//         size_t i = 2 * ii - 1;
//         if (j > i) {
//             float tempr = a[j - 1];
//             float tempi = a[j];
//             a[j - 1] = a[i - 1];
//             a[j] = a[i];
//             a[i - 1] = tempr;
//             a[i] = tempi;
//         }
//         size_t m = halfLen;
//         while (m >= 2 && j > m) {
//             j = j - m;
//             m = m / 2;
//         }
//         j = j + m;
//     }

//     for (size_t mmax = 2; len > mmax;) {
//         size_t istep = 2 * mmax;
//         theta = 2.f * Pi / (isign * mmax);
//         float wpr = -2.f * sqr(sin(0.5f * theta));
//         float wpi = sin(theta);
//         float wr = 1.;
//         float wi = 0.;
//         for (size_t ii = 1; ii <= mmax / 2; ii++) {
//             size_t m = 2 * ii - 1;
//             for (size_t jj = 0; jj <= (len - m) / istep; jj++) {
//                 size_t i = m + jj * istep;
//                 size_t j = i + mmax;
//                 float tempr = wr * a[j - 1] - wi * a[j];
//                 float tempi = wr * a[j] + wi * a[j - 1];
//                 a[j - 1] = a[i - 1] - tempr;
//                 a[j] = a[i] - tempi;
//                 a[i - 1] = a[i - 1] + tempr;
//                 a[i] = a[i] + tempi;
//             }
//             float wtemp = wr;
//             wr = wr * wpr - wi * wpi + wr;
//             wi = wi * wpr + wtemp * wpi + wi;
//         }
//         mmax = istep;
//     }
//     float twpr = -2.f * sqr(sin(0.5f * ttheta));
//     float twpi = sin(ttheta);
//     float twr = 1.f + twpr;
//     float twi = twpi;
//     for (size_t i = 2; i <= len / 4 + 1; i++) {
//         size_t i1 = i + i - 2;
//         size_t i2 = i1 + 1;
//         size_t i3 = len + 1 - i2;
//         size_t i4 = i3 + 1;
//         float wrs = twr;
//         float wis = twi;
//         float h1r = c1 * (a[i1] + a[i3]);
//         float h1i = c1 * (a[i2] - a[i4]);
//         float h2r = -c2 * (a[i2] + a[i4]);
//         float h2i = c2 * (a[i1] - a[i3]);
//         a[i1] = h1r + wrs * h2r - wis * h2i;
//         a[i2] = h1i + wrs * h2i + wis * h2r;
//         a[i3] = h1r - wrs * h2r + wis * h2i;
//         a[i4] = -h1i + wrs * h2i + wis * h2r;
//         float twtemp = twr;
//         twr = twr * twpr - twi * twpi + twr;
//         twi = twi * twpr + twtemp * twpi + twi;
//     }
//     float h1r = a[0];
//     a[0] = h1r + a[1];
//     a[1] = h1r - a[1];
//     float sum = 0.;
//     a[0] = 0.5f * a[0];
//     a[1] = 0.0;
//     for (size_t jj = 0; jj <= halfLen - 1; jj++) {
//         size_t j = 2 * jj + 1;
//         sum = sum + a[j - 1];
//         a[j - 1] = a[j];
//         a[j] = sum;
//     }

// }


void fft_simd(Complex<double> *X, size_t N)
{
    // Notes	: the length of fft must be a power of 2,and it is  a in-place algorithm
    // ref		: https://en.wikipedia.org/wiki/Cooley%E2%80%93Tukey_FFT_algorithm

    auto separate = [](Complex<double>* a, int n) {
        Complex<double>* b = new Complex<double>[n / 2];				// get temp heap storage
        for (int i = 0; i < n / 2; i++)	{	// copy all odd elements to heap storage
            b[i] = a[i * 2 + 1];
        }
        for (int i = 0; i < n / 2; i++)	{	// copy all even elements to lower-half of a[]
            a[i] = a[i * 2];
        }
        for (int i = 0; i < n / 2; i++)	{	// copy all odd (from heap) to upper-half of a[]
            a[i + n / 2] = b[i];
        }
        delete[] b;							// delete heap storage
    };

    auto fft_ = [&](Complex<double> *X, size_t N) {
        if (N < 2) {
            // bottom of recursion.
            // Do nothing here, because already X[0] = x[0]
        } else {
            separate(X, N);							// all evens to lower half, all odds to upper half
            fft2_simd(X, N / 2);					// recurse even items
            fft2_simd(X + N / 2, N / 2);			// recurse odd  items
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
    };

    separate(X, N);
    fft_(X, N / 2);
    //fft_(X + N / 2, N / 2);			// recurse odd  items
}

// void fft(Complex<> *a, size_t n) {

//     for (size_t i=1, j=0; i < n; i++) {
//         size_t bit = (n >> 1);
//         while ( j >= bit) {
//             j -= bit;
//             bit >>= 1;
//         }
//         j += bit;
//         if (i < j) {
//             std::swap(a[i], a[j]);
//         }
//     }

//     for (size_t len = 2; len <= n; len<<=1) { //Строим итеративно БПФ для отрезков длины 2, 4, 8, ... n
//         Complex w {cos(2. * Pi / len), sin(2. * Pi / len)}; //Множитель, умножив на который, мы прокрутим аргумент на один пункт длины 2*pi/len дальше
//         for (size_t i = 0; i < n; i += len) { //Обрабатываем поблочно, i - начало очередного блока
//             Complex cur_w = {1., 0.}; //Аргумент
//             for (size_t j = 0; j < len/2; j++) { //Итерируемся по блоку
//                 Complex u = a[i + j];
//                 Complex v = a[i + j + len / 2] * cur_w; //Так как записываем в эти элементы массива, их нужно скопировать
//                 a[i + j] = u + v;
//                 a[i + j + len / 2] = u - v;
//                 cur_w = cur_w * w; //Прокручиваем аргумент на угол 2*pi/len
//             } //Когда мы вышли из цикла, аргумент cur_w снова равен нулю, так как мы добавили 2*pi/len len раз.
//         }
//     }
// }

// void revfft(vector <comp> &a){
//     int n=a.size();

//     /**< Сортировка по двоичной форме, записанной зеркально */
//     for(int i=1,j=0;i<n;i++){
//         int bit=(n>>1);
//         while(j>=bit){
//             j-=bit;
//             bit>>=1;
//         }
//         j+=bit;
//         if(i<j)
//             swap(a[i],a[j]);
//     }

//     for(int len=2;len<=n;len<<=1){
//         comp w={cos(2*pi/len),-sin(2*pi/len)}; //Значения аргумента отображены
//         for(int i=0;i<n;i+=len){
//             comp cur_w={1,0};
//             for(int j=0;j<len/2;j++){
//                 comp u=a[i+j],v=a[i+j+len/2]*cur_w;
//                 a[i+j]=u+v;
//                 a[i+j+len/2]=u-v;
//                 cur_w=cur_w*w;
//             }
//         }
//     }

//     for(int i=0;i<n;i++) //Мы вычислили nW^{-1}, нужно разделить массив на n
//         a[i]=a[i]/n;
// }

}
