#ifndef MEMCPY_BEHCH
#define MEMCPY_BEHCH 0x7ffffff
/* 此文件仅用于 benchmark 宏定义基准测试, 可多次从 main.cpp 包含 */

#define MEMCPY_GLIBC                         0x1
#define MEMCPY                               0x2
#define FASTCPY                              0x4
#define MEMCPY_ERMS                          0x8
#define MEMCPY_TRIVIAL                      0x10
#define MEMCPY_KERNEL                       0x20
#define MEMCPYSSE2                          0x40//
#define MEMCPYSSE2UNROLLED2                 0x80
#define MEMCPYSSE2UNROLLED4                0x100
#define MEMCPYSSE2UNROLLED8                0x200
#define MEMCPY_MY                          0x400
#define MEMCPY_JART                        0x800
#define MEMCPY_FAST_SSE                   0x1000//
#define RTE_MEMCPY_SSE                    0x2000
#define AVX_MEMCPY                        0x4000
#define MEMCPYAVX2                        0x8000//
#define MEMCPYAVXUNROLLED2               0x10000
#define MEMCPYAVXUNROLLED4               0x20000
#define MEMCPYAVXUNROLLED8               0x40000
#define MEMCPY_MY2                       0x80000
#define MEMCPY_JART1                    0x100000
#define MEMCPY_FAST_AVX                 0x200000//
#define CUSTOM_MEMCPY                   0x400000
#define RTE_MEMCPY_AVX                  0x800000
#define BRANCH_COPY                    0x1000000
#define SWITCH_COPY                    0x2000000
#define TINY_COPY                      0x4000000
#define UNROLLED_COPY                  0x8000000
#define COPY_LARGE                    0x10000000
#define MEMCPY_GLIBC4                 0x20000000
#define MEMCPY_GLIBC5                 0x40000000
#define MEMCPY_GLIBC6                 0x80000000


#ifdef __cplusplus
extern "C"
{
#endif

void* branch_copy(void *dst, const void *src, size_t size);
void* switch_copy(void *dst, const void *src, size_t size);
void* tiny_copy(void *dst_, const void *src_, size_t size);
void* copy_forward(void *dst_, const void *src_, size_t size);
void* copy_unroll4(void *dst_, const void *src_, size_t size);
void* copy_unroll6(void *dst_, const void *src_, size_t size);
void* copy_unroll8(void *dst_, const void *src_, size_t size);
void* copy_unroll9(void *dst_, const void *src_, size_t size);
void* copy_large_NO(void *dst_, const void *src_, size_t size);
void* copy_large_NTA(void *dst_, const void *src_, size_t size);
void* copy_large_T0(void *dst_, const void *src_, size_t size);
void* copy_large_P0(void *dst_, const void *src_, size_t size);
void* copy_large_U4(void *dst_, const void *src_, size_t size);
void* copy_large_U6(void *dst, const void *src, size_t size);
void* copy_large_U8(void *dst, const void *src, size_t size);
void* copy_large_2x(void *dst_, const void *src_, size_t size);
void* copy_large_4x(void *dst_, const void *src_, size_t size);

#ifdef __cplusplus
}
#endif

#else

//#define BM_ARG1
//#define BM_ARG2

#if MEMCPY_BEHCH & MEMCPY_GLIBC
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, memcpy_glibc);
#endif

#if MEMCPY_BEHCH & MEMCPY
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, memcpy);
#endif

#if MEMCPY_BEHCH & FASTCPY
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, fastcpy);
#endif

#if MEMCPY_BEHCH & MEMCPY_ERMS// 在合适区间比较优秀
BENCHMARK_INLINE_MEMCPY(MEMCPY_ERMS, BM_ARG1, BM_ARG2);
#endif

#if MEMCPY_BEHCH & MEMCPY_TRIVIAL// 这是最慢方法
BENCHMARK_INLINE_MEMCPY(MEMCPY_TRIVIAL, BM_ARG1, BM_ARG2);
#endif

#if MEMCPY_BEHCH & MEMCPY_KERNEL// 这是使用普通寄存器的最快方法
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, memcpy_kernel);
#endif

#if MEMCPY_BEHCH & MEMCPYSSE2// 没必要比较
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, memcpySSE2);
#endif

#if MEMCPY_BEHCH & MEMCPYSSE2UNROLLED2// 没必要比较
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, memcpySSE2Unrolled2);
#endif

#if MEMCPY_BEHCH & MEMCPYSSE2UNROLLED4
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, memcpySSE2Unrolled4);
#endif

#if MEMCPY_BEHCH & MEMCPYSSE2UNROLLED8
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, memcpySSE2Unrolled8);
#endif

#if MEMCPY_BEHCH & MEMCPY_MY
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, memcpy_my);
#endif

#if MEMCPY_BEHCH & MEMCPY_JART
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, memcpy_jart);
#endif

#if MEMCPY_BEHCH & MEMCPY_FAST_SSE// 跳转表过大导致综合性能不佳
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, memcpy_fast_sse);
#endif

#if MEMCPY_BEHCH & RTE_MEMCPY_SSE
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, rte_memcpy_sse);
#endif



#if MEMCPY_BEHCH & AVX_MEMCPY// 小尺寸复制性能较差
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, AVX_memcpy);
#endif

#if MEMCPY_BEHCH & MEMCPYAVX2// 没必要比较
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, memcpyAVX2);
#endif

#if MEMCPY_BEHCH & MEMCPYAVXUNROLLED2// 没必要比较
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, memcpyAVXUnrolled2);
#endif

#if MEMCPY_BEHCH & MEMCPYAVXUNROLLED4
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, memcpyAVXUnrolled4);
#endif

#if MEMCPY_BEHCH & MEMCPYAVXUNROLLED8
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, memcpyAVXUnrolled8);
#endif

#if MEMCPY_BEHCH & MEMCPY_MY2
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, memcpy_my2);
#endif

#if MEMCPY_BEHCH & MEMCPY_JART1
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, memcpy_jart1);
#endif

#if MEMCPY_BEHCH & MEMCPY_FAST_AVX// 跳转表过大导致综合性能不佳
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, memcpy_fast_avx);
#endif

#if MEMCPY_BEHCH & CUSTOM_MEMCPY// 此实现性能不佳
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, custom_memcpy);
#endif

#if MEMCPY_BEHCH & RTE_MEMCPY_AVX
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, rte_memcpy_avx);
#endif


#if (MEMCPY_BEHCH & BRANCH_COPY) && (BM_ARG2 != 0 && BM_ARG2 <= 32)
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, branch_copy);
#endif

#if (MEMCPY_BEHCH & SWITCH_COPY) && (BM_ARG2 != 0 && BM_ARG2 <= 32)
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, switch_copy);
#endif

#if (MEMCPY_BEHCH & TINY_COPY) && (BM_ARG2 != 0 && BM_ARG2 <= 256)
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, tiny_copy);
#endif

#if (MEMCPY_BEHCH & UNROLLED_COPY) && (BM_ARG1 == 0 || BM_ARG1 >= 256)
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, copy_forward);
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, copy_unroll9);
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, copy_unroll8);
//BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, copy_unroll6);
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, copy_unroll4);
#endif

#if MEMCPY_BEHCH & COPY_LARGE
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, copy_large_NO);
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, copy_large_P0);
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, copy_large_2x);
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, copy_large_4x);
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, copy_large_NTA);
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, copy_large_T0);
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, copy_large_U4);
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, copy_large_U6);
BENCHMARK_MEMCPY(BM_ARG1, BM_ARG2, copy_large_U8);
#endif


#endif

