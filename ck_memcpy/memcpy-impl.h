#ifndef MEMCPY_IMPL_H
#define MEMCPY_IMPL_H

#include <stddef.h>


//---------------------------------------------------------------------
// force inline for compilers
//---------------------------------------------------------------------
#ifndef INLINE
#ifdef __GNUC__
#if (__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ >= 1))
#define INLINE __inline__ __attribute__((always_inline))
#else
#define INLINE __inline__
#endif
#elif defined(_MSC_VER)
#define INLINE __forceinline
#elif (defined(__BORLANDC__) || defined(__WATCOMC__))
#define INLINE __inline
#else
#define INLINE
#endif
#endif

/* 各种 memcpy 实现, 都已优化到极致 */

#ifdef __cplusplus
extern "C"
{
#endif

// rep movsb
INLINE void* memcpy_erms(void* __restrict dst, const void* __restrict src, size_t size) {
	void *ret = dst;
	asm volatile("rep movsb"
	: "+D"(dst), "+S"(src), "+c"(size)
	:
	: "memory");
	return ret;
}

// for(i = 0; i < size; ++i) dst[i] = src[i]
INLINE void* memcpy_trivial(void* __restrict dst_, const void* __restrict src_, size_t size) {
	asm volatile(
		"subq $1,%0\n\t"
		"jb 1f\n\t"
		"0:"
		"movzbl (%0,%2),%%r9d\n\t"
		"movb %%r9b,(%0,%1)\n\t"
		"subq $1,%0\n\t"
		"jnb 0b\n\t"
		"1:"
		:"+r"(size)
		:"r"(dst_), "r"(src_)
		: "r9","memory"
	);
	return dst_;
}

// 使用一个 xmm 寄存器复制数据
void* memcpySSE2(void* __restrict destination, const void* __restrict source, size_t size);

// 使用一个 ymm 寄存器复制数据
void* memcpyAVX2(void* __restrict destination, const void* __restrict source, size_t size);

// 使用两个 xmm 寄存器复制数据
void* memcpySSE2Unrolled2(void* __restrict destination, const void* __restrict source, size_t size);

// 使用四个 xmm 寄存器复制数据
void* memcpySSE2Unrolled4(void* __restrict destination, const void* __restrict source, size_t size);

// 使用八个 xmm 寄存器复制数据
void* memcpySSE2Unrolled8(void* __restrict destination, const void* __restrict source, size_t size);

// 使用两个 ymm 寄存器复制数据
void* memcpyAVXUnrolled2(void* __restrict destination, const void* __restrict source, size_t size);

// 使用四个 ymm 寄存器复制数据
void* memcpyAVXUnrolled4(void* __restrict destination, const void* __restrict source, size_t size);

// 使用八个 ymm 寄存器复制数据
void* memcpyAVXUnrolled8(void* __restrict destination, const void* __restrict source, size_t size);

// 原版 clickhouse memcpy (SSE版)
void* memcpy_my(void* __restrict destination, const void* __restrict source, size_t size);

// 原版 clickhouse memcpy (AVX版)
void* memcpy_my2(void* __restrict destination, const void* __restrict source, size_t size);

// Linux kernel 的 memcpy 实现
void* memcpy_kernel(void *dest, const void *src, size_t size);

#ifdef __cplusplus
}
#endif

#endif

