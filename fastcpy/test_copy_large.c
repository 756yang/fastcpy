#include <stddef.h>
#include <stdint.h>
#include <immintrin.h>


#ifndef xsun_likely
# if defined(__GNUC__) && defined(__has_builtin) && __has_builtin(__builtin_expect)
#   define xsun_likely(x) __builtin_expect(!!(x), 1)
# elif defined(__has_cpp_attribute) && __has_cpp_attribute(likely)
#   define xsun_likely(x) \
    ([](long a) -> long { if(a) [[likely]] return 1; return 0; })(x)
# else
#   define xsun_likely(x) (!!(x))
# endif
#endif

#ifndef xsun_unlikely
# define xsun_unlikely(x) (!xsun_likely(!(x)))
#endif

#include "x86asm_def.h"

#define PAGE_SIZE 4096
#define CACHELINE_SIZE 64


// 4M ~ 16M
// 64M ~ 128M

/*

一次循环复制的向量数量不太影响性能, 瓶颈在内存带宽

1.  循环复制 4 向量
2.  循环复制 8 向量
3.  循环复制 16 向量
4.  循环复制 8 向量于两个页面 这种方式有性能提升
5.  循环复制 16 向量于四个页面

预取措施：(不使用预取措施, 有硬件预取的情况下, 软件预取只会降低性能)

1.  PREFETCHT0
2.  PREFETCHNTA

避免预取超出内存范围的地址

*/

#define PREFETCH_STEP 4

#if VEC_SIZE != 32
# error Unsupported VEC_SIZE!
#endif

// size >= 1024*1024

// 结论：软件预取不会提升 memcpy 性能

__attribute__((optimize("align-functions=4096")))
void* copy_large_NO(void *dst_, const void *src_, size_t size) {
	VMM_TYPE() v0, v1, v2, v3, v4, v5;
	uint8_t *dst = (uint8_t*)dst_, *src = (uint8_t*)src_;
	size_t padding = 64 - ((size_t)dst & 63);// 对齐缓存行
	const uint8_t *sra = src + padding;// 对齐 src
	uint8_t *dsa = dst + padding;// 对齐 dst
	v0 = SIMD_OPT(loadu_ps)((float*)src);
	v1 = SIMD_OPT(loadu_ps)((float*)(src + VEC_SIZE));
	v4 = SIMD_OPT(loadu_ps)((float*)(src + size - VEC_SIZE));
	v5 = SIMD_OPT(loadu_ps)((float*)(src + size - 2*VEC_SIZE));
	SIMD_OPT(storeu_ps)((float*)dst, v0);
	SIMD_OPT(storeu_ps)((float*)(dst + VEC_SIZE), v1);
	uint8_t *dsz = dst + size - 4*VEC_SIZE;
	do {
		v0 = SIMD_OPT(loadu_ps)((float*)sra);
		v1 = SIMD_OPT(loadu_ps)((float*)(sra + VEC_SIZE));
		v2 = SIMD_OPT(loadu_ps)((float*)(sra + 2 * VEC_SIZE));
		v3 = SIMD_OPT(loadu_ps)((float*)(sra + 3 * VEC_SIZE));
		sra += 4 * VEC_SIZE;
		SIMD_OPT(stream_ps)((float*)dsa, v0);
		SIMD_OPT(stream_ps)((float*)(dsa + VEC_SIZE), v1);
		SIMD_OPT(stream_ps)((float*)(dsa + 2 * VEC_SIZE), v2);
		SIMD_OPT(stream_ps)((float*)(dsa + 3 * VEC_SIZE), v3);
		dsa += 4 * VEC_SIZE;
	} while(dsa < dsz);
	_mm_sfence();
	SIMD_OPT(storeu_ps)((float*)(dsz + 3 * VEC_SIZE), v4);
	SIMD_OPT(storeu_ps)((float*)(dsz + 2 * VEC_SIZE), v5);
	if(xsun_likely(dsa - dsz < 2 * VEC_SIZE)) {
		v0 = SIMD_OPT(loadu_ps)((float*)sra);
		v1 = SIMD_OPT(loadu_ps)((float*)(sra + VEC_SIZE));
		STORE_ASM1(movaps, v0, 0, dsa);
		STORE_ASM1(movaps, v1, VEC_SIZE, dsa);
	}
	VZEROUPPER_ASM;
	return dst;
}


__attribute__((optimize("align-functions=4096")))
void* copy_large_P0(void *dst_, const void *src_, size_t size) {
	VMM_TYPE() v0, v1, v2, v3, v4, v5;
	uint8_t *dst = (uint8_t*)dst_, *src = (uint8_t*)src_;
	size_t padding = 64 - ((size_t)dst & 63);// 对齐缓存行
	uint8_t *sra = src + padding;// 对齐 src
	uint8_t *dsa = dst + padding;// 对齐 dst
	v0 = SIMD_OPT(loadu_ps)((float*)src);
	v1 = SIMD_OPT(loadu_ps)((float*)(src + VEC_SIZE));
	v4 = SIMD_OPT(loadu_ps)((float*)(src + size - VEC_SIZE));
	v5 = SIMD_OPT(loadu_ps)((float*)(src + size - 2*VEC_SIZE));
	SIMD_OPT(storeu_ps)((float*)dst, v0);
	SIMD_OPT(storeu_ps)((float*)(dst + VEC_SIZE), v1);
	uint8_t *dsz = dst + size - 4*VEC_SIZE;
	do {
		v0 = SIMD_OPT(loadu_ps)((float*)sra);
		v1 = SIMD_OPT(loadu_ps)((float*)(sra + VEC_SIZE));
		v2 = SIMD_OPT(loadu_ps)((float*)(sra + 2 * VEC_SIZE));
		v3 = SIMD_OPT(loadu_ps)((float*)(sra + 3 * VEC_SIZE));
		_mm_prefetch(sra + (4*VEC_SIZE), _MM_HINT_T0);
		sra += 4 * VEC_SIZE;
		SIMD_OPT(stream_ps)((float*)dsa, v0);
		SIMD_OPT(stream_ps)((float*)(dsa + VEC_SIZE), v1);
		SIMD_OPT(stream_ps)((float*)(dsa + 2 * VEC_SIZE), v2);
		SIMD_OPT(stream_ps)((float*)(dsa + 3 * VEC_SIZE), v3);
		dsa += 4 * VEC_SIZE;
	} while(dsa < dsz);
	_mm_sfence();
	SIMD_OPT(storeu_ps)((float*)(dsz + 3 * VEC_SIZE), v4);
	SIMD_OPT(storeu_ps)((float*)(dsz + 2 * VEC_SIZE), v5);
	if(xsun_likely(dsa - dsz < 2 * VEC_SIZE)) {
		v0 = SIMD_OPT(loadu_ps)((float*)sra);
		v1 = SIMD_OPT(loadu_ps)((float*)(sra + VEC_SIZE));
		STORE_ASM1(movaps, v0, 0, dsa);
		STORE_ASM1(movaps, v1, VEC_SIZE, dsa);
	}
	VZEROUPPER_ASM;
	return dst;
}

/* large_memcpy 内部循环的每次字节数  */
#if VEC_SIZE == 64
# define LARGE_LOAD_SIZE (VEC_SIZE * 2)
#else
# define LARGE_LOAD_SIZE (VEC_SIZE * 4)
#endif

#ifndef PREFETCH
# define PREFETCH(addr) _mm_prefetch(addr, _MM_HINT_T0)
#endif

/* 假设预取大小为 64 字节  */
#ifndef PREFETCH_SIZE
# define PREFETCH_SIZE 64
#endif

#define PREFETCHED_LOAD_SIZE LARGE_LOAD_SIZE

#if PREFETCH_SIZE == 64 || PREFETCH_SIZE == 128
# if PREFETCHED_LOAD_SIZE == PREFETCH_SIZE
#  define PREFETCH_ONE_SET(dir, base, offset) \
	PREFETCH ((char*)(base) + (offset))
# elif PREFETCHED_LOAD_SIZE == 2 * PREFETCH_SIZE
#  define PREFETCH_ONE_SET(dir, base, offset) \
	PREFETCH ((char*)(base) + (offset)); \
	PREFETCH ((char*)(base) + (offset) + (dir) * PREFETCH_SIZE)
# elif PREFETCHED_LOAD_SIZE == 4 * PREFETCH_SIZE
#  define PREFETCH_ONE_SET(dir, base, offset) \
	PREFETCH ((char*)(base) + (offset)); \
	PREFETCH ((char*)(base) + (offset) + (dir) * PREFETCH_SIZE); \
	PREFETCH ((char*)(base) + (offset) + (dir) * PREFETCH_SIZE * 2); \
	PREFETCH ((char*)(base) + (offset) + (dir) * PREFETCH_SIZE * 3)
# else
#   error Unsupported PREFETCHED_LOAD_SIZE!
# endif
#else
# error Unsupported PREFETCH_SIZE!
#endif

#if LARGE_LOAD_SIZE == (VEC_SIZE * 2)
# define LOAD_ONE_SET(base, offset, vec0, vec1, ...) \
	vec0 = SIMD_OPT(loadu_ps)((float*)((char*)(base) + (offset))); \
	vec1 = SIMD_OPT(loadu_ps)((float*)((char*)(base) + (offset) + VEC_SIZE))
# define STORE_ONE_SET(base, offset, vec0, vec1, ...) \
	SIMD_OPT(stream_ps)((float*)((char*)(base) + (offset)), vec0); \
	SIMD_OPT(stream_ps)((float*)((char*)(base) + (offset) + VEC_SIZE), vec1)
#elif LARGE_LOAD_SIZE == (VEC_SIZE * 4)
# define LOAD_ONE_SET(base, offset, vec0, vec1, vec2, vec3) \
	vec0 = SIMD_OPT(loadu_ps)((float*)((char*)(base) + (offset))); \
	vec1 = SIMD_OPT(loadu_ps)((float*)((char*)(base) + (offset) + VEC_SIZE)); \
	vec2 = SIMD_OPT(loadu_ps)((float*)((char*)(base) + (offset) + 2 * VEC_SIZE)); \
	vec3 = SIMD_OPT(loadu_ps)((float*)((char*)(base) + (offset) + 3 * VEC_SIZE))
# define STORE_ONE_SET(base, offset, vec0, vec1, vec2, vec3) \
	SIMD_OPT(stream_ps)((float*)((char*)(base) + (offset)), vec0); \
	SIMD_OPT(stream_ps)((float*)((char*)(base) + (offset) + VEC_SIZE), vec1); \
	SIMD_OPT(stream_ps)((float*)((char*)(base) + (offset) + 2 * VEC_SIZE), vec2); \
	SIMD_OPT(stream_ps)((float*)((char*)(base) + (offset) + 3 * VEC_SIZE), vec3)
#else
# error Invalid LARGE_LOAD_SIZE
#endif


__attribute__((optimize("align-functions=4096")))
void* copy_large_2x(void *dst_, const void *src_, size_t size) {
	register VMM_TYPE() vmm0 asm(VMM_NAME "0");// 传入的寄存器参数
	register VMM_TYPE() vmm1 asm(VMM_NAME "1");
	register VMM_TYPE() vmm2 asm(VMM_NAME "2");
	register VMM_TYPE() vmm3 asm(VMM_NAME "3");
	register VMM_TYPE() vmm4 asm(VMM_NAME "4");
	register VMM_TYPE() vmm5 asm(VMM_NAME "5");
	register VMM_TYPE() vmm6 asm(VMM_NAME "6");
	register VMM_TYPE() vmm7 asm(VMM_NAME "7");
	uint8_t *dst = (uint8_t*)dst_, *src = (uint8_t*)src_;
#ifdef XMM_NEED_SAVE
	ADD_STACK_ASM(-0xA0);
	PUSH_XMM_ASM(xmm6, 0);
	PUSH_XMM_ASM(xmm7, 0x10);
#endif
	size_t padding = 64 - ((size_t)dst & 63);// 对齐缓存行
	const uint8_t *sra = src + padding;// 对齐 src
	uint8_t *dsa = dst + padding;// 对齐 dst
	vmm0 = SIMD_OPT(loadu_ps)((float*)src);
	vmm1 = SIMD_OPT(loadu_ps)((float*)(src + VEC_SIZE));
	SIMD_OPT(storeu_ps)((float*)dst, vmm0);
	SIMD_OPT(storeu_ps)((float*)(dst + VEC_SIZE), vmm1);
	size_t szp, sza = size - padding;// 对齐 size
	uint32_t szq;// 剩余字节数
	szq = sza & (2 * PAGE_SIZE - 1);
	szp = sza / (2 * PAGE_SIZE);
	do {
		uint32_t count = PAGE_SIZE / LARGE_LOAD_SIZE;
		do {
			LOAD_ONE_SET(sra, 0, vmm0, vmm1, vmm2, vmm3);
			LOAD_ONE_SET(sra, PAGE_SIZE, vmm4, vmm5, vmm6, vmm7);
			PREFETCH_ONE_SET(1, sra, LARGE_LOAD_SIZE);
			PREFETCH_ONE_SET(1, sra, PAGE_SIZE + LARGE_LOAD_SIZE);
			sra += LARGE_LOAD_SIZE;
			STORE_ONE_SET(dsa, 0, vmm0, vmm1, vmm2, vmm3);
			STORE_ONE_SET(dsa, PAGE_SIZE, vmm4, vmm5, vmm6, vmm7);
			dsa += LARGE_LOAD_SIZE;
		} while(--count != 0);
		dsa += PAGE_SIZE;
		sra += PAGE_SIZE;
	} while(--szp != 0);
	_mm_sfence();
#ifdef XMM_NEED_SAVE
	POP_XMM_ASM(xmm7, 0x10);
	POP_XMM_ASM(xmm6, 0);
	ADD_STACK_ASM(0xA0);
#endif
	while(xsun_likely(szq > 4 * VEC_SIZE)) {
		vmm0 = SIMD_OPT(loadu_ps)((float*)sra);
		vmm1 = SIMD_OPT(loadu_ps)((float*)(sra + VEC_SIZE));
		vmm2 = SIMD_OPT(loadu_ps)((float*)(sra + 2 * VEC_SIZE));
		vmm3 = SIMD_OPT(loadu_ps)((float*)(sra + 3 * VEC_SIZE));
		PREFETCH_ONE_SET(1, sra, 4 * VEC_SIZE);
		PREFETCH_ONE_SET(1, dsa, 4 * VEC_SIZE);
		sra += 4 * VEC_SIZE;
		szq -= 4 * VEC_SIZE;
		SIMD_OPT(store_ps)((float*)dsa, vmm0);
		SIMD_OPT(store_ps)((float*)(dsa + VEC_SIZE), vmm1);
		SIMD_OPT(store_ps)((float*)(dsa + 2 * VEC_SIZE), vmm2);
		SIMD_OPT(store_ps)((float*)(dsa + 3 * VEC_SIZE), vmm3);
		dsa += 4 * VEC_SIZE;
	}
	vmm0 = SIMD_OPT(loadu_ps)((float*)(sra + szq - 4 * VEC_SIZE));
	vmm1 = SIMD_OPT(loadu_ps)((float*)(sra + szq - 3 * VEC_SIZE));
	vmm2 = SIMD_OPT(loadu_ps)((float*)(sra + szq - 2 * VEC_SIZE));
	vmm3 = SIMD_OPT(loadu_ps)((float*)(sra + szq - VEC_SIZE));
	SIMD_OPT(storeu_ps)((float*)(dsa + szq - 4 * VEC_SIZE), vmm0);
	SIMD_OPT(storeu_ps)((float*)(dsa + szq - 3 * VEC_SIZE), vmm1);
	SIMD_OPT(storeu_ps)((float*)(dsa + szq - 2 * VEC_SIZE), vmm2);
	SIMD_OPT(storeu_ps)((float*)(dsa + szq - VEC_SIZE), vmm3);
	VZEROUPPER_ASM;
	return dst;
}

__attribute__((optimize("align-functions=4096")))
void* copy_large_4x(void *dst_, const void *src_, size_t size) {
	register VMM_TYPE() vmm0 asm(VMM_NAME "0");// 传入的寄存器参数
	register VMM_TYPE() vmm1 asm(VMM_NAME "1");
	register VMM_TYPE() vmm2 asm(VMM_NAME "2");
	register VMM_TYPE() vmm3 asm(VMM_NAME "3");
	register VMM_TYPE() vmm4 asm(VMM_NAME "4");
	register VMM_TYPE() vmm5 asm(VMM_NAME "5");
	register VMM_TYPE() vmm6 asm(VMM_NAME "6");
	register VMM_TYPE() vmm7 asm(VMM_NAME "7");
	register VMM_TYPE() vmm8 asm(VMM_NAME "8");
	register VMM_TYPE() vmm9 asm(VMM_NAME "9");
	register VMM_TYPE() vmm10 asm(VMM_NAME "10");
	register VMM_TYPE() vmm11 asm(VMM_NAME "11");
	register VMM_TYPE() vmm12 asm(VMM_NAME "12");
	register VMM_TYPE() vmm13 asm(VMM_NAME "13");
	register VMM_TYPE() vmm14 asm(VMM_NAME "14");
	register VMM_TYPE() vmm15 asm(VMM_NAME "15");
	uint8_t *dst = (uint8_t*)dst_, *src = (uint8_t*)src_;
#ifdef XMM_NEED_SAVE
	ADD_STACK_ASM(-0xA0);
	PUSH_XMM_ASM(xmm6, 0);
	PUSH_XMM_ASM(xmm7, 0x10);
	PUSH_XMM_ASM(xmm8, 0x20);
	PUSH_XMM_ASM(xmm9, 0x30);
	PUSH_XMM_ASM(xmm10, 0x40);
	PUSH_XMM_ASM(xmm11, 0x50);
	PUSH_XMM_ASM(xmm12, 0x60);
	PUSH_XMM_ASM(xmm13, 0x70);
	PUSH_XMM_ASM(xmm14, 0x80);
	PUSH_XMM_ASM(xmm15, 0x90);
#endif
	size_t padding = 64 - ((size_t)dst & 63);// 对齐缓存行
	const uint8_t *sra = src + padding;// 对齐 src
	uint8_t *dsa = dst + padding;// 对齐 dst
	vmm0 = SIMD_OPT(loadu_ps)((float*)src);
	vmm1 = SIMD_OPT(loadu_ps)((float*)(src + VEC_SIZE));
	SIMD_OPT(storeu_ps)((float*)dst, vmm0);
	SIMD_OPT(storeu_ps)((float*)(dst + VEC_SIZE), vmm1);
	size_t szp, sza = size - padding;// 对齐 size
	uint32_t szq;// 剩余字节数
	szq = sza & (4 * PAGE_SIZE - 1);
	szp = sza / (4 * PAGE_SIZE);
	do {
		uint32_t count = PAGE_SIZE / LARGE_LOAD_SIZE;
		do {
			LOAD_ONE_SET(sra, 0, vmm0, vmm1, vmm2, vmm3);
			LOAD_ONE_SET(sra, PAGE_SIZE, vmm4, vmm5, vmm6, vmm7);
			LOAD_ONE_SET(sra, 2 * PAGE_SIZE, vmm8, vmm9, vmm10, vmm11);
			LOAD_ONE_SET(sra, 3 * PAGE_SIZE, vmm12, vmm13, vmm14, vmm15);
			PREFETCH_ONE_SET(1, sra, LARGE_LOAD_SIZE);
			PREFETCH_ONE_SET(1, sra, PAGE_SIZE + LARGE_LOAD_SIZE);
			PREFETCH_ONE_SET(1, sra, 2 * PAGE_SIZE + LARGE_LOAD_SIZE);
			PREFETCH_ONE_SET(1, sra, 3 * PAGE_SIZE + LARGE_LOAD_SIZE);
			sra += LARGE_LOAD_SIZE;
			STORE_ONE_SET(dsa, 0, vmm0, vmm1, vmm2, vmm3);
			STORE_ONE_SET(dsa, PAGE_SIZE, vmm4, vmm5, vmm6, vmm7);
			STORE_ONE_SET(dsa, 2 * PAGE_SIZE, vmm8, vmm9, vmm10, vmm11);
			STORE_ONE_SET(dsa, 3 * PAGE_SIZE, vmm12, vmm13, vmm14, vmm15);
			dsa += LARGE_LOAD_SIZE;
		} while(--count != 0);
		dsa += 3 * PAGE_SIZE;
		sra += 3 * PAGE_SIZE;
	} while(--szp != 0);
	_mm_sfence();
#ifdef XMM_NEED_SAVE
	POP_XMM_ASM(xmm15, 0x90);
	POP_XMM_ASM(xmm14, 0x80);
	POP_XMM_ASM(xmm13, 0x70);
	POP_XMM_ASM(xmm12, 0x60);
	POP_XMM_ASM(xmm11, 0x50);
	POP_XMM_ASM(xmm10, 0x40);
	POP_XMM_ASM(xmm9, 0x30);
	POP_XMM_ASM(xmm8, 0x20);
	POP_XMM_ASM(xmm7, 0x10);
	POP_XMM_ASM(xmm6, 0);
	ADD_STACK_ASM(0xA0);
#endif
	while(xsun_likely(szq > 4 * VEC_SIZE)) {
		vmm0 = SIMD_OPT(loadu_ps)((float*)sra);
		vmm1 = SIMD_OPT(loadu_ps)((float*)(sra + VEC_SIZE));
		vmm2 = SIMD_OPT(loadu_ps)((float*)(sra + 2 * VEC_SIZE));
		vmm3 = SIMD_OPT(loadu_ps)((float*)(sra + 3 * VEC_SIZE));
		PREFETCH_ONE_SET(1, sra, 4 * VEC_SIZE);
		PREFETCH_ONE_SET(1, dsa, 4 * VEC_SIZE);
		sra += 4 * VEC_SIZE;
		szq -= 4 * VEC_SIZE;
		SIMD_OPT(store_ps)((float*)dsa, vmm0);
		SIMD_OPT(store_ps)((float*)(dsa + VEC_SIZE), vmm1);
		SIMD_OPT(store_ps)((float*)(dsa + 2 * VEC_SIZE), vmm2);
		SIMD_OPT(store_ps)((float*)(dsa + 3 * VEC_SIZE), vmm3);
		dsa += 4 * VEC_SIZE;
	}
	vmm0 = SIMD_OPT(loadu_ps)((float*)(sra + szq - 4 * VEC_SIZE));
	vmm1 = SIMD_OPT(loadu_ps)((float*)(sra + szq - 3 * VEC_SIZE));
	vmm2 = SIMD_OPT(loadu_ps)((float*)(sra + szq - 2 * VEC_SIZE));
	vmm3 = SIMD_OPT(loadu_ps)((float*)(sra + szq - VEC_SIZE));
	SIMD_OPT(storeu_ps)((float*)(dsa + szq - 4 * VEC_SIZE), vmm0);
	SIMD_OPT(storeu_ps)((float*)(dsa + szq - 3 * VEC_SIZE), vmm1);
	SIMD_OPT(storeu_ps)((float*)(dsa + szq - 2 * VEC_SIZE), vmm2);
	SIMD_OPT(storeu_ps)((float*)(dsa + szq - VEC_SIZE), vmm3);
	VZEROUPPER_ASM;
	return dst;
}

__attribute__((optimize("align-functions=4096")))
void* copy_large_NTA(void *dst_, const void *src_, size_t size) {
	VMM_TYPE() v0, v1, v2, v3, v4, v5;
	uint8_t *dst = (uint8_t*)dst_, *src = (uint8_t*)src_;
	size_t padding = 64 - ((size_t)dst & 63);// 对齐缓存行
	const uint8_t *sra = src + padding;// 对齐 src
	uint8_t *dsa = dst + padding;// 对齐 dst
	v0 = SIMD_OPT(loadu_ps)((float*)src);
	v1 = SIMD_OPT(loadu_ps)((float*)(src + VEC_SIZE));
	v4 = SIMD_OPT(loadu_ps)((float*)(src + size - VEC_SIZE));
	v5 = SIMD_OPT(loadu_ps)((float*)(src + size - 2*VEC_SIZE));
	SIMD_OPT(storeu_ps)((float*)dst, v0);
	SIMD_OPT(storeu_ps)((float*)(dst + VEC_SIZE), v1);
	uint8_t *dsz = dst + size - 4*VEC_SIZE;
	uint8_t *dsh = dsz - PREFETCH_STEP*(4*VEC_SIZE);
DO_PREFETCH_NTA:
	_mm_prefetch(sra + PREFETCH_STEP*(4*VEC_SIZE), _MM_HINT_NTA);
	_mm_prefetch(sra + PREFETCH_STEP*(4*VEC_SIZE) + 64, _MM_HINT_NTA);
	do {
		v0 = SIMD_OPT(loadu_ps)((float*)sra);
		v1 = SIMD_OPT(loadu_ps)((float*)(sra + VEC_SIZE));
		v2 = SIMD_OPT(loadu_ps)((float*)(sra + 2 * VEC_SIZE));
		v3 = SIMD_OPT(loadu_ps)((float*)(sra + 3 * VEC_SIZE));
		sra += 4 * VEC_SIZE;
		SIMD_OPT(stream_ps)((float*)dsa, v0);
		SIMD_OPT(stream_ps)((float*)(dsa + VEC_SIZE), v1);
		SIMD_OPT(stream_ps)((float*)(dsa + 2 * VEC_SIZE), v2);
		SIMD_OPT(stream_ps)((float*)(dsa + 3 * VEC_SIZE), v3);
		dsa += 4 * VEC_SIZE;
		if(dsa < dsh) goto DO_PREFETCH_NTA;
	} while(dsa < dsz);
	_mm_sfence();
	SIMD_OPT(storeu_ps)((float*)(dsz + 3 * VEC_SIZE), v4);
	SIMD_OPT(storeu_ps)((float*)(dsz + 2 * VEC_SIZE), v5);
	if(xsun_likely(dsa - dsz < 2 * VEC_SIZE)) {
		v0 = SIMD_OPT(loadu_ps)((float*)sra);
		v1 = SIMD_OPT(loadu_ps)((float*)(sra + VEC_SIZE));
		STORE_ASM1(movaps, v0, 0, dsa);
		STORE_ASM1(movaps, v1, VEC_SIZE, dsa);
	}
	VZEROUPPER_ASM;
	return dst;
}


__attribute__((optimize("align-functions=4096")))
void* copy_large_T0(void *dst_, const void *src_, size_t size) {
	VMM_TYPE() v0, v1, v2, v3, v4, v5;
	uint8_t *dst = (uint8_t*)dst_, *src = (uint8_t*)src_;
	size_t padding = 64 - ((size_t)dst & 63);// 对齐缓存行
	const uint8_t *sra = src + padding;// 对齐 src
	uint8_t *dsa = dst + padding;// 对齐 dst
	v0 = SIMD_OPT(loadu_ps)((float*)src);
	v1 = SIMD_OPT(loadu_ps)((float*)(src + VEC_SIZE));
	v4 = SIMD_OPT(loadu_ps)((float*)(src + size - VEC_SIZE));
	v5 = SIMD_OPT(loadu_ps)((float*)(src + size - 2*VEC_SIZE));
	SIMD_OPT(storeu_ps)((float*)dst, v0);
	SIMD_OPT(storeu_ps)((float*)(dst + VEC_SIZE), v1);
	uint8_t *dsz = dst + size - 4*VEC_SIZE;
	uint8_t *dsh = dsz - PREFETCH_STEP*(4*VEC_SIZE);
DO_PREFETCH_T0:
	_mm_prefetch(sra + PREFETCH_STEP*(4*VEC_SIZE), _MM_HINT_T0);
	_mm_prefetch(sra + PREFETCH_STEP*(4*VEC_SIZE) + 64, _MM_HINT_T0);
	do {
		v0 = SIMD_OPT(loadu_ps)((float*)sra);
		v1 = SIMD_OPT(loadu_ps)((float*)(sra + VEC_SIZE));
		v2 = SIMD_OPT(loadu_ps)((float*)(sra + 2 * VEC_SIZE));
		v3 = SIMD_OPT(loadu_ps)((float*)(sra + 3 * VEC_SIZE));
		sra += 4 * VEC_SIZE;
		SIMD_OPT(stream_ps)((float*)dsa, v0);
		SIMD_OPT(stream_ps)((float*)(dsa + VEC_SIZE), v1);
		SIMD_OPT(stream_ps)((float*)(dsa + 2 * VEC_SIZE), v2);
		SIMD_OPT(stream_ps)((float*)(dsa + 3 * VEC_SIZE), v3);
		dsa += 4 * VEC_SIZE;
		if(dsa < dsh) goto DO_PREFETCH_T0;
	} while(dsa < dsz);
	_mm_sfence();
	SIMD_OPT(storeu_ps)((float*)(dsz + 3 * VEC_SIZE), v4);
	SIMD_OPT(storeu_ps)((float*)(dsz + 2 * VEC_SIZE), v5);
	if(xsun_likely(dsa - dsz < 2 * VEC_SIZE)) {
		v0 = SIMD_OPT(loadu_ps)((float*)sra);
		v1 = SIMD_OPT(loadu_ps)((float*)(sra + VEC_SIZE));
		STORE_ASM1(movaps, v0, 0, dsa);
		STORE_ASM1(movaps, v1, VEC_SIZE, dsa);
	}
	VZEROUPPER_ASM;
	return dst;
}

__attribute__((optimize("align-functions=4096")))
void* copy_large_U4(void *dst_, const void *src_, size_t size) {
	uint8_t *dst = (uint8_t*)dst_, *src = (uint8_t*)src_;
	VMM_TYPE() vmm0, vmm1, vmm2, vmm3, vmm4, vmm5;
	// 仅六个 SIMD 寄存器可直接使用
	size_t padding = VEC_SIZE;
	vmm0 = SIMD_OPT(loadu_ps)((float*)src);
	vmm4 = SIMD_OPT(loadu_ps)((float*)(src + size - VEC_SIZE));
	vmm5 = SIMD_OPT(loadu_ps)((float*)(src + size - 2 * VEC_SIZE));
	padding -= (size_t)dst & VEC_SIZE - 1;
	SIMD_OPT(storeu_ps)((float*)dst, vmm0);
	const uint8_t *sra = src + padding;
	uint8_t *dsa = dst + padding, *dsz = dst + size - 4 * VEC_SIZE;
	do {
		vmm0 = SIMD_OPT(loadu_ps)((float*)sra);
		vmm1 = SIMD_OPT(loadu_ps)((float*)(sra + VEC_SIZE));
		vmm2 = SIMD_OPT(loadu_ps)((float*)(sra + 2 * VEC_SIZE));
		vmm3 = SIMD_OPT(loadu_ps)((float*)(sra + 3 * VEC_SIZE));
		// 避免 gcc 错误优化降低性能
		asm volatile("sub %[i], %[p]" :[p]"+r"(dsa) :[i]"i"(-4*VEC_SIZE));
		asm volatile("sub %[i], %[p]" :[p]"+r"(sra) :[i]"i"(-4*VEC_SIZE));
		// store_ps 可能生成 movups 指令, 这在旧平台会降低性能
		STORE_ASM1(movntps, vmm0, -4*VEC_SIZE, dsa);
		STORE_ASM1(movntps, vmm1, -3*VEC_SIZE, dsa);
		STORE_ASM1(movntps, vmm2, -2*VEC_SIZE, dsa);
		STORE_ASM1(movntps, vmm3, -VEC_SIZE, dsa);
	} while(dsz > dsa);
	_mm_sfence();
	SIMD_OPT(storeu_ps)((float*)(dsz + 3 * VEC_SIZE), vmm4);
	SIMD_OPT(storeu_ps)((float*)(dsz + 2 * VEC_SIZE), vmm5);
	if(xsun_likely(dsa - dsz < 2 * VEC_SIZE)) {
		vmm0 = SIMD_OPT(loadu_ps)((float*)sra);
		vmm1 = SIMD_OPT(loadu_ps)((float*)(sra + VEC_SIZE));
		STORE_ASM1(movaps, vmm0, 0, dsa);
		STORE_ASM1(movaps, vmm1, VEC_SIZE, dsa);
	}
	VZEROUPPER_ASM;
	return dst;
}


__attribute__((optimize("align-functions=4096")))
void* copy_large_U6(void *dst_, const void *src_, size_t size) {
	VMM_TYPE() vmm0, vmm1, vmm2, vmm3, vmm4, vmm5;
#ifdef XMM_NEED_SAVE/* ms_abi */
	register uint8_t *dsa asm("rcx") = (uint8_t*)dst_;
	register uint8_t *sra asm("rdx") = (uint8_t*)src_;
	register uint8_t *dsz asm("r8") = (uint8_t*)size;
	register size_t load_size asm("rax");
	register uint8_t *dst;//asm("r9");
#else/* sysv_abi */
	register uint8_t *dsa asm("rdi") = (uint8_t*)dst_;
	register uint8_t *sra asm("rsi") = (uint8_t*)src_;
	register uint8_t *dsz asm("rdx") = (uint8_t*)size;
	register size_t load_size asm("rcx");
	register uint8_t *dst asm("rax");
#endif
	asm volatile(ASM_OPT(/* 这个更快, 因为减少了一些加法 */
		VEX_PRE(movups) (%[sa]), %[v1]\n\t
		VEX_PRE(movups) -%c[i](%[sa],%[dz]), %[v0]\n\t
		movl $(6*%c[i]), %k[lz]\n\t
		movq %[da], %[dd]\n\t
		leaq -%c[i](%[da],%[dz]), %[dz]\n\t
		VEX_PRE(movups) %[v1], (%[da])\n\t
		orq $(%c[i]-1), %[da]\n\t
		lea (1-%c[i])(%[da],%[lz]), %[da]\n\t
		VEX_PRE(movups) %[v0], (%[dz])\n\t
		subq %[dd], %[sa]\n\t
		addq %[da], %[sa]\n\t
		): [sa]"+r"(sra), [da]"+r"(dsa), [dz]"+r"(dsz), [dd]"=r"(dst), [lz]"=r"(load_size), [v0]"=x"(vmm0), [v1]"=x"(vmm1)
		: [i]"i"(VEC_SIZE) : "memory"
	);// 这段指令大小 0x30 bytes
	asm volatile(".p2align 4\n\t""0:\n\t"::);/* 指令字节优化 */
	LOAD_ASM1(movups, vmm0, -5*VEC_SIZE, sra);
	LOAD_ASM1(movups, vmm1, -4*VEC_SIZE, sra);
	LOAD_ASM1(movups, vmm2, -3*VEC_SIZE, sra);
	LOAD_ASM1(movups, vmm3, -2*VEC_SIZE, sra);
	LOAD_ASM1(movups, vmm4, -1*VEC_SIZE, sra);
	LOAD_ASM1(movups, vmm5, 0*VEC_SIZE, sra);
	// 避免 gcc 错误优化降低性能
	asm volatile("add %[lz], %[sa]" :[sa]"+r"(sra) :[lz]"r"(load_size));
	// store_ps 可能生成 movups 指令, 这在旧平台会降低性能
	STORE_ASM1(movntps, vmm0, -5*VEC_SIZE, dsa);
	STORE_ASM1(movntps, vmm1, -4*VEC_SIZE, dsa);
	STORE_ASM1(movntps, vmm2, -3*VEC_SIZE, dsa);
	STORE_ASM1(movntps, vmm3, -2*VEC_SIZE, dsa);
	STORE_ASM1(movntps, vmm4, -1*VEC_SIZE, dsa);
	STORE_ASM1(movntps, vmm5, 0*VEC_SIZE, dsa);
	asm volatile("add %[lz], %[da]" :[da]"+r"(dsa) :[lz]"r"(load_size));
	asm volatile("cmp %[da], %[dz]\n\t""ja 0b\n\t" ::[da]"r"(dsa), [dz]"r"(dsz));
	_mm_sfence();
	/* 使用循环和分支一样快 */
	asm volatile(ASM_OPT(
		sub $(5*%c[i]), %[da]\n\t
		sub $(5*%c[i]), %[sa]\n\t
		cmp %[dz], %[da]\n\t
		jnb 2f\n\t
		1: .p2align 3\n\t
		VEX_PRE(movups) (%[sa]), %[v]\n\t
		VEX_PRE(movaps) %[v], (%[da])\n\t
		add %[i], %[da]\n\t
		add %[i], %[sa]\n\t
		cmp %[dz], %[da]\n\t
		jb 1b\n\t
		2:\n\t
		): [da]"+r"(dsa), [sa]"+r"(sra), [dz]"+r"(dsz), [v]"=x"(vmm0)
		: [i]"i"(VEC_SIZE) :"memory"
	);
	VZEROUPPER_ASM;
	return dst;
}

__attribute__((optimize("align-functions=4096")))
void* copy_large_U8(void *dst_, const void *src_, size_t size) {
	register VMM_TYPE() vmm0 asm(VMM_NAME "0");// 固定寄存器位置
	register VMM_TYPE() vmm1 asm(VMM_NAME "1");
	register VMM_TYPE() vmm2 asm(VMM_NAME "2");
	register VMM_TYPE() vmm3 asm(VMM_NAME "3");
	register VMM_TYPE() vmm4 asm(VMM_NAME "4");
	register VMM_TYPE() vmm5 asm(VMM_NAME "5");
	register VMM_TYPE() vmm6 asm(VMM_NAME "6");
	register VMM_TYPE() vmm7 asm(VMM_NAME "7");
	// 仅六个 SIMD 寄存器可直接使用
#ifdef XMM_NEED_SAVE/* ms_abi */
	register uint8_t *dsa asm("rcx") = (uint8_t*)dst_;
	register uint8_t *sra asm("rdx") = (uint8_t*)src_;
	register uint8_t *dsz asm("r8") = (uint8_t*)size;
	register size_t load_size asm("rax");
	register uint8_t *dst;//asm("r9");
#else/* sysv_abi */
	register uint8_t *dsa asm("rdi") = (uint8_t*)dst_;
	register uint8_t *sra asm("rsi") = (uint8_t*)src_;
	register uint8_t *dsz asm("rdx") = (uint8_t*)size;
	register size_t load_size asm("rcx");
	register uint8_t *dst asm("rax");
#endif
	/* 循环准备 */
	asm volatile(ASM_OPT(// 这段代码已经是最优序列了
		VEX_PRE(movups) (%[sa]), %[v1]\n\t
		VEX_PRE(movups) -%c[i](%[sa],%[dz]), %[v0]\n\t
		movl $(8*%c[i]), %k[lz]\n\t
#ifdef XMM_NEED_SAVE
		subq $0x20, %%rsp\n\t
#endif
		VEX_PRE(movups) %[v1], (%[da])\n\t
		movq %[da], %[dd]\n\t
		leaq -4*%c[i](%[da],%[dz]), %[dz]\n\t
#ifdef XMM_NEED_SAVE
		VEX_PRE(movups) %%xmm6, 0(%%rsp)\n\t
		VEX_PRE(movups) %%xmm7, 0x10(%%rsp)\n\t
#endif
		orq $(%c[i]-1), %[da]\n\t
		lea (1-4*%c[i])(%[da],%[lz]), %[da]\n\t
		subq %[dd], %[sa]\n\t
		addq %[da], %[sa]\n\t
		VEX_PRE(movups) %[v0], 3*%c[i](%[dz])\n\t
		): [sa]"+r"(sra), [da]"+r"(dsa), [dz]"+r"(dsz), [dd]"=r"(dst), [lz]"=r"(load_size), [v0]"=x"(vmm5), [v1]"=x"(vmm4)
		: [i]"i"(VEC_SIZE) : "memory"
	); // 这段指令大小 0x40 bytes
	/* 循环复制 */
	asm volatile(".p2align 4\n\t""0:\n\t"::);/* 指令字节优化 */
	LOAD_ASM1(movups, vmm0, -4*VEC_SIZE, sra);
	LOAD_ASM1(movups, vmm1, -3*VEC_SIZE, sra);
	LOAD_ASM1(movups, vmm2, -2*VEC_SIZE, sra);
	LOAD_ASM1(movups, vmm3, -1*VEC_SIZE, sra);
	LOAD_ASM1(movups, vmm4, 0*VEC_SIZE, sra);
	LOAD_ASM1(movups, vmm5, 1*VEC_SIZE, sra);
	LOAD_ASM1(movups, vmm6, 2*VEC_SIZE, sra);
	LOAD_ASM1(movups, vmm7, 3*VEC_SIZE, sra);
	// 避免 gcc 错误优化降低性能
	asm volatile("add %[lz], %[sa]" :[sa]"+r"(sra) :[lz]"r"(load_size));
	// store_ps 可能生成 movups 指令, 这在旧平台会降低性能
	STORE_ASM1(movntps, vmm0, -4*VEC_SIZE, dsa);
	STORE_ASM1(movntps, vmm1, -3*VEC_SIZE, dsa);
	STORE_ASM1(movntps, vmm2, -2*VEC_SIZE, dsa);
	STORE_ASM1(movntps, vmm3, -1*VEC_SIZE, dsa);
	STORE_ASM1(movntps, vmm4, 0*VEC_SIZE, dsa);
	STORE_ASM1(movntps, vmm5, 1*VEC_SIZE, dsa);
	STORE_ASM1(movntps, vmm6, 2*VEC_SIZE, dsa);
	STORE_ASM1(movntps, vmm7, 3*VEC_SIZE, dsa);
	asm volatile("add %[lz], %[da]" :[da]"+r"(dsa) :[lz]"r"(load_size));
	asm volatile("cmp %[da], %[dz]\n\t""ja 0b\n\t" ::[da]"r"(dsa), [dz]"r"(dsz));
	_mm_sfence();
	/* 循环收尾 */
	asm volatile(ASM_OPT(
		add $(3*%c[i]), %[dz]\n\t
		add $-(4*%c[i]), %[da]\n\t
		add $-(4*%c[i]), %[sa]\n\t
#ifdef XMM_NEED_SAVE
		mov %[dd], %[dA]\n\t
#endif
		cmp %[dz], %[da]\n\t
		jnb 2f\n\t
		1:\n\t
		VEX_PRE(movups) (%[sa]), %[v]\n\t
		VEX_PRE(movaps) %[v], (%[da])\n\t
		add %[i], %[da]\n\t
		add %[i], %[sa]\n\t
		cmp %[dz], %[da]\n\t
		jb 1b\n\t
		2:\n\t
		): [da]"+r"(dsa), [sa]"+r"(sra), [dz]"+r"(dsz), [dA]"=a"(dst), [v]"=x"(vmm0)
		: [dd]"r"(dst), [i]"i"(VEC_SIZE) :"memory"
	);
#ifdef XMM_NEED_SAVE
	POP_XMM_ASM(xmm6, 0);
	POP_XMM_ASM(xmm7, 0x10);
	ADD_STACK_ASM(0x20);
#endif
	VZEROUPPER_ASM;
	return dst;
}






