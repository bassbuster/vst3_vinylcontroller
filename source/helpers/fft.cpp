#include "fft.h"

#include <cmath>

namespace Steinberg::Vst {

constexpr float Pi = 3.141592653;

// inline float sqr(int x) {
//     return x * x;
// }

inline float sqr(float x) {
    return x * x;
}

// inline double sqr(double x) {
//     return x * x;
// }

void fastsinetransform(float *a, int tnn) {
    int jj;
    int j;
    int tm;
    int n2;
    float sum;
    float y1;
    float y2;
    float theta;
    float wi;
    float wr;
    float wpi;
    float wpr;
    float wtemp;
    float twr;
    float twi;
    float twpr;
    float twpi;
    float twtemp;
    float ttheta;
    int i;
    int i1;
    int i2;
    int i3;
    int i4;
    float c1;
    float c2;
    float h1r;
    float h1i;
    float h2r;
    float h2i;
    float wrs;
    float wis;
    int nn;
    int ii;
    int n;
    int mmax;
    int m;
    int istep;
    int isign;
    float tempr;
    float tempi;

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

}
