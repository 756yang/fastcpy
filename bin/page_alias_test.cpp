//sum.cpp
#include <x86intrin.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define TIMER_TYPE CLOCK_REALTIME

double time_diff(timespec start, timespec end)
{
    timespec temp;
    if ((end.tv_nsec-start.tv_nsec)<0) {
        temp.tv_sec = end.tv_sec-start.tv_sec-1;
        temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec-start.tv_sec;
        temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }
    return (double)temp.tv_sec +  (double)temp.tv_nsec*1E-9;
}

void sum(float * __restrict x, float * __restrict y, float * __restrict z, const int n) {
    #if defined(__GNUC__)
    x = (float*)__builtin_assume_aligned (x, 64);
    y = (float*)__builtin_assume_aligned (y, 64);
    z = (float*)__builtin_assume_aligned (z, 64);
    #endif
    for(int i=0; i<n; i++) {
        z[i] = x[i] + y[i];
    }
}

#if (defined(__AVX__))
void sum_avx(float *x, float *y, float *z, const int n) {
    float *x1 = x;
    float *y1 = y;
    float *z1 = z;
    for(int i=0; i<n/64; i++) { //unroll eight times
        _mm256_store_ps(z1+64*i+  0,_mm256_add_ps(_mm256_load_ps(x1+64*i+ 0), _mm256_load_ps(y1+64*i+  0)));
        _mm256_store_ps(z1+64*i+  8,_mm256_add_ps(_mm256_load_ps(x1+64*i+ 8), _mm256_load_ps(y1+64*i+  8)));
        _mm256_store_ps(z1+64*i+ 16,_mm256_add_ps(_mm256_load_ps(x1+64*i+16), _mm256_load_ps(y1+64*i+ 16)));
        _mm256_store_ps(z1+64*i+ 24,_mm256_add_ps(_mm256_load_ps(x1+64*i+24), _mm256_load_ps(y1+64*i+ 24)));
        _mm256_store_ps(z1+64*i+ 32,_mm256_add_ps(_mm256_load_ps(x1+64*i+32), _mm256_load_ps(y1+64*i+ 32)));
        _mm256_store_ps(z1+64*i+ 40,_mm256_add_ps(_mm256_load_ps(x1+64*i+40), _mm256_load_ps(y1+64*i+ 40)));
        _mm256_store_ps(z1+64*i+ 48,_mm256_add_ps(_mm256_load_ps(x1+64*i+48), _mm256_load_ps(y1+64*i+ 48)));
        _mm256_store_ps(z1+64*i+ 56,_mm256_add_ps(_mm256_load_ps(x1+64*i+56), _mm256_load_ps(y1+64*i+ 56)));
    }
}
#else
void sum_sse(float *x, float *y, float *z, const int n) {
    float *x1 = x;
    float *y1 = y;
    float *z1 = z;
    for(int i=0; i<n/32; i++) { //unroll eight times
        _mm_store_ps(z1+32*i+  0,_mm_add_ps(_mm_load_ps(x1+32*i+ 0), _mm_load_ps(y1+32*i+  0)));
        _mm_store_ps(z1+32*i+  4,_mm_add_ps(_mm_load_ps(x1+32*i+ 4), _mm_load_ps(y1+32*i+  4)));
        _mm_store_ps(z1+32*i+  8,_mm_add_ps(_mm_load_ps(x1+32*i+ 8), _mm_load_ps(y1+32*i+  8)));
        _mm_store_ps(z1+32*i+ 12,_mm_add_ps(_mm_load_ps(x1+32*i+12), _mm_load_ps(y1+32*i+ 12)));
        _mm_store_ps(z1+32*i+ 16,_mm_add_ps(_mm_load_ps(x1+32*i+16), _mm_load_ps(y1+32*i+ 16)));
        _mm_store_ps(z1+32*i+ 20,_mm_add_ps(_mm_load_ps(x1+32*i+20), _mm_load_ps(y1+32*i+ 20)));
        _mm_store_ps(z1+32*i+ 24,_mm_add_ps(_mm_load_ps(x1+32*i+24), _mm_load_ps(y1+32*i+ 24)));
        _mm_store_ps(z1+32*i+ 28,_mm_add_ps(_mm_load_ps(x1+32*i+28), _mm_load_ps(y1+32*i+ 28)));
    }
}
#endif

int main(int argc, char **argv) {
    const int n = 2048;
    const int k = atoi(argv[1]);
    float *z2 = (float*)_mm_malloc(sizeof(float)*n, 64);

    char *mem = (char*)_mm_malloc(1<<18,4096);
    char *a = mem;
    char *b = mem;
    char *c = b+n*sizeof(float)+k*64;

    float *x = (float*)a;
    float *y = (float*)b;
    float *z = (float*)c;
    printf("x %p, y %p, z %p, y-x %d, z-y %d\n", a, b, c, b-a, c-b);

    for(int i=0; i<n; i++) {
        x[i] = (1.0f*i+1.0f);
        y[i] = (1.0f*i+1.0f);
        z[i] = 0;
    }
    int repeat = 10000000;
    timespec time1, time2;

    sum(x,y,z,n);
    #if (defined(__AVX__))
    sum_avx(x,y,z2,n);
    #else
    sum_sse(x,y,z2,n);
    #endif
    printf("error: %d\n", memcmp(z,z2,sizeof(float)*n));

    while(1) {
        clock_gettime(TIMER_TYPE, &time1);
        #if (defined(__AVX__))
        for(int r=0; r<repeat; r++) sum_avx(x,y,z,n);
        #else
        for(int r=0; r<repeat; r++) sum_sse(x,y,z,n);
        #endif
        clock_gettime(TIMER_TYPE, &time2);

        double dtime = time_diff(time1,time2);
        double peak = 4.0*128;
        //double peak = 1.3*96; //haswell @1.3GHz
        //double peak = 3.6*48; //Ivy Bridge @ 3.6Ghz
        //double peak = 2.4*24; // Westmere @ 2.4GHz
        double rate = 3.0*1E-9*sizeof(float)*n*repeat/dtime;
        printf("dtime %f, %f GB/s, peak, %f, efficiency %f%%\n", dtime, rate, peak, 100*rate/peak);
    }
}
