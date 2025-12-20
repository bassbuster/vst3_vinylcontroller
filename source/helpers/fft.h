#pragma once

#include <cmath>


namespace Steinberg::Vst {

template<typename T>
struct Complex {
    T real {0};
    T imaginary {0};

    friend Complex operator * (const Complex &A, const Complex &B) noexcept {
        return {A.real * B.real - A.imaginary * B.imaginary, A.real * B.imaginary + A.imaginary * B.real};
    }

    friend Complex operator + (const Complex &A, const Complex &B) noexcept {
        return {A.real + B.real, A.imaginary + B.imaginary};
    }

    friend Complex operator - (const Complex &A, const Complex &B) noexcept {
        return {A.real - B.real, A.imaginary - B.imaginary};
    }

};

void fft_simd(Complex<double> * X, size_t N);


template<typename T>
void fft(Complex<T> *a, size_t n) {

    constexpr T Pi = 3.1415926535897932384626433832;

    for (size_t i=1, j = 0; i < n; i++) {
        size_t bit = (n >> 1);
        while ( j >= bit) {
            j -= bit;
            bit >>= 1;
        }
        j += bit;
        if (i < j) {
            std::swap(a[i], a[j]);
        }
    }

    for (size_t len = 2; len <= n; len<<=1) {
        Complex<T> w {cos(2. * Pi / len), sin(2. * Pi / len)};
        for (size_t i = 0; i < n; i += len) {
            Complex<T> cur_w = {1., 0.};
            for (size_t j = 0; j < len / 2; j++) {
                Complex<T> u = a[i + j];
                Complex<T> v = a[i + j + len / 2] * cur_w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                cur_w = cur_w * w;
            }
        }
    }
}


template<typename T>
inline T sqr(T x) {
    return x * x;
}

template<typename T>
void fastsine(T *a, size_t len) {

    constexpr T Pi = 3.1415926535897932384626433832;

    if (len == 1) {
        a[0] = 0;
        return;
    }

    T theta = Pi / len;
    T ttheta = 2. * Pi / len;
    T wr = 1.;
    T wi = 0.;
    T wpr = -2. * sqr(sin(0.5 * theta));
    T wpi = sin(theta);
    a[0] = 0.0;
    size_t halfLen = len / 2;
    size_t n2 = len + 2;

    for (size_t j = 2; j <= halfLen + 1; j++) {
        T wtemp = wr;
        wr = wr * wpr - wi * wpi + wr;
        wi = wi * wpr + wtemp * wpi + wi;
        T y1 = wi * (a[j - 1] + a[n2 - j - 1]);
        T y2 = 0.5 * (a[j - 1] - a[n2 - j - 1]);
        a[j - 1] = y1 + y2;
        a[n2 - j - 1] = y1 - y2;
    }

    const T c1 = 0.5;
    const T c2 = -0.5;
    //const T isign = 1.;

    size_t j = 1;
    for (size_t ii = 1; ii <= halfLen; ii++) {
        size_t i = 2 * ii - 1;
        if (j > i) {
            T tempr = a[j - 1];
            T tempi = a[j];
            a[j - 1] = a[i - 1];
            a[j] = a[i];
            a[i - 1] = tempr;
            a[i] = tempi;
        }
        size_t m = halfLen;
        while (m >= 2 && j > m) {
            j = j - m;
            m = m / 2;
        }
        j = j + m;
    }

    for (size_t mmax = 2; len > mmax;) {
        size_t istep = 2 * mmax;
        theta = 2. * Pi / (/*isign **/ mmax);
        T wpr = -2. * sqr(sin(0.5 * theta));
        T wpi = sin(theta);
        T wr = 1.;
        T wi = 0.;
        for (size_t ii = 1; ii <= mmax / 2; ii++) {
            size_t m = 2 * ii - 1;
            for (size_t jj = 0; jj <= (len - m) / istep; jj++) {
                size_t i = m + jj * istep;
                size_t j = i + mmax;
                T tempr = wr * a[j - 1] - wi * a[j];
                T tempi = wr * a[j] + wi * a[j - 1];
                a[j - 1] = a[i - 1] - tempr;
                a[j] = a[i] - tempi;
                a[i - 1] = a[i - 1] + tempr;
                a[i] = a[i] + tempi;
            }
            T wtemp = wr;
            wr = wr * wpr - wi * wpi + wr;
            wi = wi * wpr + wtemp * wpi + wi;
        }
        mmax = istep;
    }
    T twpr = -2. * sqr(sin(0.5 * ttheta));
    T twpi = sin(ttheta);
    T twr = 1. + twpr;
    T twi = twpi;
    for (size_t i = 2; i <= len / 4 + 1; i++) {
        size_t i1 = i + i - 2;
        size_t i2 = i1 + 1;
        size_t i3 = len + 1 - i2;
        size_t i4 = i3 + 1;
        T wrs = twr;
        T wis = twi;
        T h1r = c1 * (a[i1] + a[i3]);
        T h1i = c1 * (a[i2] - a[i4]);
        T h2r = -c2 * (a[i2] + a[i4]);
        T h2i = c2 * (a[i1] - a[i3]);
        a[i1] = h1r + wrs * h2r - wis * h2i;
        a[i2] = h1i + wrs * h2i + wis * h2r;
        a[i3] = h1r - wrs * h2r + wis * h2i;
        a[i4] = -h1i + wrs * h2i + wis * h2r;
        T twtemp = twr;
        twr = twr * twpr - twi * twpi + twr;
        twi = twi * twpr + twtemp * twpi + twi;
    }
    T h1r = a[0];
    a[0] = h1r + a[1];
    a[1] = h1r - a[1];
    T sum = 0.;
    a[0] = 0.5 * a[0];
    a[1] = 0.;
    for (size_t jj = 0; jj <= halfLen - 1; jj++) {
        size_t j = 2 * jj + 1;
        sum = sum + a[j - 1];
        a[j - 1] = a[j];
        a[j] = sum;
    }
}

}
