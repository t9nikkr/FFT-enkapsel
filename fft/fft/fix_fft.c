//
//  fix_fft.c
//  fixedTest
//
//  Created by Adam on 2018-01-28.
//  Copyright © 2018 Adam. All rights reserved.
//
#if 1
//#include "fix_fft.h"

/* fix_fft.c - Fixed-point in-place Fast Fourier Transform  */
/*
 All data are fixed-point short integers, in which -32768
 to +32768 represent -1.0 to +1.0 respectively. Integer
 arithmetic is used for speed, instead of the more natural
 floating-point.
 For the forward FFT (time -> freq), fixed scaling is
 performed to prevent arithmetic overflow, and to map a 0dB
 sine/cosine wave (i.e. amplitude = 32767) to two -6dB freq
 coefficients. The return value is always 0.
 For the inverse FFT (freq -> time), fixed scaling cannot be
 done, as two 0dB coefficients would sum to a peak amplitude
 of 64K, overflowing the 32k range of the fixed-point integers.
 Thus, the fix_fft() routine performs variable scaling, and
 returns a value which is the number of bits LEFT by which
 the output must be shifted to get the actual amplitude
 (i.e. if fix_fft() returns 3, each value of fr[] and fi[]
 must be multiplied by 8 (2**3) for proper scaling.
 Clearly, this cannot be done within fixed-point short
 integers. In practice, if the result is to be used as a
 filter, the scale_shift can usually be ignored, as the
 result will be approximately correctly normalized as is.
 Written by:  Tom Roberts  11/8/89
 Made portable:  Malcolm Slaney 12/15/94 malcolm@interval.com
 Enhanced:  Dimitrios P. Bouras  14 Jun 2006 dbouras@ieee.org
 */

#define N_WAVE      256    /* full length of Sinewave[] */
#define LOG2_N_WAVE 8      /* log2(N_WAVE) */

#include <stdint.h>
/*
 Henceforth "short" implies 16-bit word. If this is not
 the case in your architecture, please replace "short"
 with a type definition which *is* a 16-bit word.
 */

/*
 Since we only use 3/4 of N_WAVE, we define only
 this many samples, in order to conserve data space.
 */
int8_t Sinewave[N_WAVE-N_WAVE/4] = {
//int8_t Sinewave[N_WAVE] = {
    0,    3,    6,    9,    12,   15,   18,   21,
    24,   28,   31,   34,   37,   40,   43,   46,
    48,   51,   54,   57,   60,   63,   65,   68,
    71,   73,   76,   78,   81,   83,   85,   88,
    90,   92,   94,   96,   98,   100,  102,  104,
    106,  108,  109,  111,  112,  114,  115,  117,
    118,  119,  120,  121,  122,  123,  124,  124,
    125,  126,  126,  127,  127,  127,  127,  127,
    127,  127,  127,  127,  127,  127,  126,  126,
    125,  124,  124,  123,  122,  121,  120,  119,
    118,  117,  115,  114,  112,  111,  109,  108,
    106,  104,  102,  100,  98,   96,   94,   92,
    90,   88,   85,   83,   81,   78,   76,   73,
    71,   68,   65,   63,   60,   57,   54,   51,
    48,   46,   43,   40,   37,   34,   31,   28,
    24,   21,   18,   15,   12,   9,    6,    3,
    0,    -3,   -6,   -9,   -12,  -15,  -18,  -21,
    -24,  -28,  -31,  -34,  -37,  -40,  -43,  -46,
    -48,  -51,  -54,  -57,  -60,  -63,  -65,  -68,
    -71,  -73,  -76,  -78,  -81,  -83,  -85,  -88,
    -90,  -92,  -94,  -96,  -98,  -100, -102, -104,
    -106, -108, -109, -111, -112, -114, -115, -117,
    -118, -119, -120, -121, -122, -123, -124, -124,
    -125, -126, -126, -127, -127, -127, -127, -127,
        /*
        -127, -127, -127, -127, -127, -127, -126, -126,
         -125, -124, -124, -123, -122, -121, -120, -119,
         -118, -117, -115, -114, -112, -111, -109, -108,
         -106, -104, -102, -100, -98, -96, -94, -92, 
         -90, -88, -85, -83, -81, -78, -76, -73, 
         -71, -68, -65, -63, -60, -57, -54, -51, 
         -48, -46, -43, -40, -37, -34, -31, -28, 
         -24, -21, -18, -15, -12, -9, -6, -3, */
};
/*
 FIX_MPY() - fixed-point multiplication & scaling.
 Substitute inline assembly for hardware-specific
 optimization suited to a particluar DSP processor.
 Scaling ensures that result remains 16-bit.
 */
inline int8_t fixmul(int8_t a, int8_t b)
{
    /* shift right one less bit (i.e. 15-1) */
    int16_t c = ((int16_t)a * (int16_t)b) >> 6;
    /* last bit shifted out = rounding-bit */
    b = c & 0x01;
    /* last shift + rounding bit */
    a = (c >> 1) + b;
    return a;
	//return c;
}

/*
 fix_fft() - perform forward/inverse fast Fourier transform.
 fr[n],fi[n] are real and imaginary arrays, both INPUT AND
 RESULT (in-place FFT), with 0 <= n < 2**m; set inverse to
 0 for forward transform (FFT), or 1 for iFFT.
 */
uint16_t fix_fft(int8_t fr[], int8_t fi[], uint8_t m, uint8_t inverse)
{
    int16_t mr, nn, i, j, l, k, istep, n, scale, shift;
    int8_t qr, qi, tr, ti, wr, wi;
    
    n = 1 << m;
    
    /* max FFT size = N_WAVE */
    if (n > N_WAVE)
        return -1;
    
    mr = 0;
    nn = n - 1;
    scale = 0;
    
    /* decimation in time - re-order data */
    for (m=1; m<=nn; ++m) {
        l = n;
        do {
            l >>= 1;
        } while (mr+l > nn);
        mr = (mr & (l-1)) + l;
        
        if (mr <= m)
            continue;
        tr = fr[m];
        fr[m] = fr[mr];
        fr[mr] = tr;
        ti = fi[m];
        fi[m] = fi[mr];
        fi[mr] = ti;
    }
    
    l = 1;
    k = LOG2_N_WAVE-1;
    while (l < n) {
        if (inverse) {
            /* variable scaling, depending upon data */
            shift = 0;
            for (i=0; i<n; ++i) {
                j = fr[i];
                if (j < 0)
                    j = -j;
                m = fi[i];
                if (m < 0)
                    m = -m;
                if (j > 63 || m > 63) {
                    shift = 1;
                    break;
                }
            }
            if (shift)
                ++scale;
        } else {
            /*
             fixed scaling, for proper normalization --
             there will be log2(n) passes, so this results
             in an overall factor of 1/n, distributed to
             maximize arithmetic accuracy.
             */
            shift = 1;
        }
        /*
         it may not be obvious, but the shift will be
         performed on each data point exactly once,
         during this pass.
         */
        istep = l << 1;
        for (m=0; m<l; ++m) {
            j = m << k;
            /* 0 <= j < N_WAVE/2 */
            wr =  Sinewave[j+N_WAVE/4];
            wi = -Sinewave[j];
            if (inverse)
                wi = -wi;
            if (shift) {
                wr >>= 1;
                wi >>= 1;
            }
            for (i=m; i<n; i+=istep) {
                j = i + l;
                tr = fixmul(wr,fr[j]) - fixmul(wi,fi[j]);
                ti = fixmul(wr,fi[j]) + fixmul(wi,fr[j]);
                qr = fr[i];
                qi = fi[i];
                if (shift) {
                    qr >>= 1;
                    qi >>= 1;
                }
                fr[j] = qr - tr;
                fi[j] = qi - ti;
                fr[i] = qr + tr;
                fi[i] = qi + ti;
            }
        }
        --k;
        l = istep;
    }
    return scale;
}

/*
 fix_fftr() - forward/inverse FFT on array of real numbers.
 Real FFT/iFFT using half-size complex FFT by distributing
 even/odd samples into real/imaginary arrays respectively.
 In order to save data space (i.e. to avoid two arrays, one
 for real, one for imaginary samples), we proceed in the
 following two steps: a) samples are rearranged in the real
 array so that all even samples are in places 0-(N/2-1) and
 all imaginary samples in places (N/2)-(N-1), and b) fix_fft
 is called with fr and fi pointing to index 0 and index N/2
 respectively in the original array. The above guarantees
 that fix_fft "sees" consecutive real samples as alternating
 real and imaginary samples in the complex array.
 */
/*
int16_t fix_fftr(int8_t f[], uint8_t m, uint8_t inverse)
{
    int16_t i, N = 1<<(m-1), scale = 0;
    int8_t tt, *fr=f, *fi=&f[N];
    
    if (inverse)
        scale = fix_fft(fi, fr, m-1, inverse);
    for (i=1; i<N; i+=2) {
        tt = f[N+i-1];
        f[N+i-1] = f[i];
        f[i] = tt;
    }
    if (! inverse)
        scale = fix_fft(fi, fr, m-1, inverse);
    return scale;
}
*/
int16_t fix_fftr(int8_t f[], uint8_t m, uint8_t inverse) {
    int16_t i, N = 1<<(m-1), scale = 0;
    int8_t tt, *fr=f, *fi=&f[N];

    if (inverse)
        scale = fix_fft(fi, fr, m-1, inverse);
    /*
    for (i=1; i<N; i+=2) {
        tt = f[N+i-1];
        f[N+i-1] = f[i];
        f[i] = tt;
    }
    */
    int8_t frt[N];
    int8_t fit[N];
	
    for(i=0; i<N; i++) {
        /*
        tt = f[i];
        fr[i] = f[2*i];
        fi[i] = f[2*i+1];
         */
        frt[i] = f[2*i];
        fit[i] = f[2*i+1];
    }
	
    for(i=0; i<N; i++) {
        fr[i] = frt[i];
        fi[i] = fit[i];
    }
   
    if (!inverse)
        scale = fix_fft(fi, fr, m-1, inverse);
    return scale;
}

void split(int8_t* X, int8_t* G, uint16_t N) {
    int8_t *Xr=X, *Xi=&X[N/2], *Gr=G, *Gi=&G[N/2];//, Twrp, Twip, Twrn, Twin, evenr, eveni, oddr, oddi, f=N/2;
    for(uint8_t i=0; i<N/2; i++) {
    //for(uint8_t n=N/4, i=0; n>0; n--, i++) {
        /*
        Twrp = Sinewave[2*n+64];
        Twip = Sinewave[2*n];
        Twrn = Sinewave[f-(2*n+64)];
        Twin = Sinewave[f-(2*n)];
        evenr = Xr[n] + Xr[f-n];
        eveni = Xi[n] - Xi[f-n];
        oddr = Xi[n] + Xi[f-n];
        oddi = Xr[f-n] - Xr[n];
        
        Gr[n] = evenr + (((oddr*Twrp)>>7) - ((oddi*Twip)>>7));
        Gi[n] = eveni + (((oddi*Twrp)>>7) + ((oddr*Twip)>>7));
        Gr[f-n] = evenr + (((oddr*Twrn)>>7) + ((oddi*Twin)>>7));
        Gi[f-n] = -eveni + (((-oddi*Twrn)>>7) + ((oddr*Twin)>>7));
        */
		uint8_t v = i << 1;
        int8_t Ai = (-Sinewave[v+N_WAVE/4])>>1;//-Sinewave[i+64];//
        int8_t Ar = (127 - Sinewave[v])>>1;//-Sinewave[i];//
        int8_t Bi = (Sinewave[v+N_WAVE/4])>>1;//Sinewave[i+64];//Sinewave[2*i+64]>>1;
        int8_t Br = (127 + Sinewave[v])>>1;//Sinewave[i];//
        
		/*
        int8_t Ar = (Sinewave[2*i]-127)>>1;
        int8_t Ai = (-Sinewave[2*i+N_WAVE/4])>>1;
        int8_t Br = Sinewave[2*i+N_WAVE/4]>>1;
        int8_t Bi = Sinewave[2*i]>>1;
        */

        Gr[i] = ((Xr[i]*Ar)>>7) - ((Xi[i]*Ai)>>7) + ((Xr[(N/2)-i]*Br)>>7) + ((Xi[(N/2)-i]*Bi)>>7);
        Gi[i] = ((Xi[i]*Ar)>>7) + ((Xr[i]*Ai)>>7)  + ((Xr[(N/2)-i]*Bi)>>7) - ((Xi[(N/2)-i]*Br)>>7);
		
		Gr[N/2-i]=Gr[i];
		Gi[N/2-i]=-Gi[i];
    }

    //Gr[N/2] = Gr[0] + Gi[0];
    //Gi[N/2] = 0;
}

#endif
