#include "memcpy_glibc.h"
#include <stddef.h>
#include <stdint.h>
#include <immintrin.h>


#ifndef NOINLINE
# if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
#   define NOINLINE __attribute__((__noinline__))
# elif defined(_MSC_VER)
#   define NOINLINE __declspec(noinline)
# else
#   define NOINLINE
# endif
#endif

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
#   define PREFETCH_ONE_SET(dir, base, offset) \
	PREFETCH ((char*)(base) + (offset))
# elif PREFETCHED_LOAD_SIZE == 2 * PREFETCH_SIZE
#   define PREFETCH_ONE_SET(dir, base, offset) \
	PREFETCH ((char*)(base) + (offset)); \
	PREFETCH ((char*)(base) + (offset) + (dir) * PREFETCH_SIZE)
# elif PREFETCHED_LOAD_SIZE == 4 * PREFETCH_SIZE
#   define PREFETCH_ONE_SET(dir, base, offset) \
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


#ifdef __cplusplus
extern "C"
#endif
X64_CALL NOINLINE uint8_t* memcpy_large_memcpy(uint8_t *dst, const uint8_t *src, size_t size/*, VMM_TYPE() vmm0*/) {
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
#ifdef XMM_NEED_SAVE
	ADD_STACK_ASM(-0xA0);
	PUSH_XMM_ASM(xmm6, 0);
	PUSH_XMM_ASM(xmm7, 0x10);
#endif
#if VEC_SIZE < 64
	vmm1 = SIMD_OPT(loadu_ps)((float*)(src + VEC_SIZE));
#endif
#if VEC_SIZE < 32
	vmm2 = SIMD_OPT(loadu_ps)((float*)(src + 2 * VEC_SIZE));
	vmm3 = SIMD_OPT(loadu_ps)((float*)(src + 3 * VEC_SIZE));
#endif
	SIMD_OPT(storeu_ps)((float*)dst, vmm0);
#if VEC_SIZE < 64
	SIMD_OPT(storeu_ps)((float*)(dst + VEC_SIZE), vmm1);
#endif
#if VEC_SIZE < 32
	SIMD_OPT(storeu_ps)((float*)(dst + 2 * VEC_SIZE), vmm2);
	SIMD_OPT(storeu_ps)((float*)(dst + 3 * VEC_SIZE), vmm3);
#endif
	size_t r8 = 64 - ((size_t)dst & 63);// padding
	const uint8_t *sra = src + r8;// 对齐 src
	uint8_t *dsa = dst + r8;// 对齐 dst
	size_t szp, sza = size - r8;// 对齐 size
	uint32_t szq;// 剩余字节数
	// ~(src - dst) == dst - src - 1
	if(xsun_unlikely((~(src - dst) & (PAGE_SIZE - 8 * VEC_SIZE)) == 0 ||
		sza >= 16 * __memcpy_glibc_config.__x86_shared_non_temporal_threshold))
	{// large_memcpy_4x
#ifdef XMM_NEED_SAVE
		PUSH_XMM_ASM(xmm8, 0x20);
		PUSH_XMM_ASM(xmm9, 0x30);
		PUSH_XMM_ASM(xmm10, 0x40);
		PUSH_XMM_ASM(xmm11, 0x50);
		PUSH_XMM_ASM(xmm12, 0x60);
		PUSH_XMM_ASM(xmm13, 0x70);
		PUSH_XMM_ASM(xmm14, 0x80);
		PUSH_XMM_ASM(xmm15, 0x90);
#endif
		szq = sza & (4 * PAGE_SIZE - 1);
		szp = sza / (4 * PAGE_SIZE);
		do {
			uint32_t count = PAGE_SIZE / LARGE_LOAD_SIZE;
			do {
				PREFETCH_ONE_SET(1, sra, LARGE_LOAD_SIZE);
				PREFETCH_ONE_SET(1, sra, PAGE_SIZE + LARGE_LOAD_SIZE);
				PREFETCH_ONE_SET(1, sra, 2 * PAGE_SIZE + LARGE_LOAD_SIZE);
				PREFETCH_ONE_SET(1, sra, 3 * PAGE_SIZE + LARGE_LOAD_SIZE);
				asm volatile("");
				LOAD_ONE_SET(sra, 0, vmm0, vmm1, vmm2, vmm3);
				LOAD_ONE_SET(sra, PAGE_SIZE, vmm4, vmm5, vmm6, vmm7);
				LOAD_ONE_SET(sra, 2 * PAGE_SIZE, vmm8, vmm9, vmm10, vmm11);
				LOAD_ONE_SET(sra, 3 * PAGE_SIZE, vmm12, vmm13, vmm14, vmm15);
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
#endif
	}
	else {// large_memcpy_2x
		szq = sza & (2 * PAGE_SIZE - 1);
		szp = sza / (2 * PAGE_SIZE);
		do {
			uint32_t count = PAGE_SIZE / LARGE_LOAD_SIZE;
			do {
				PREFETCH_ONE_SET(1, sra, LARGE_LOAD_SIZE);
				//PREFETCH_ONE_SET(1, sra, LARGE_LOAD_SIZE * 2);
				PREFETCH_ONE_SET(1, sra, PAGE_SIZE + LARGE_LOAD_SIZE);
				//PREFETCH_ONE_SET(1, sra, PAGE_SIZE + LARGE_LOAD_SIZE * 2);
				asm volatile("");
				LOAD_ONE_SET(sra, 0, vmm0, vmm1, vmm2, vmm3);
				LOAD_ONE_SET(sra, PAGE_SIZE, vmm4, vmm5, vmm6, vmm7);
				sra += LARGE_LOAD_SIZE;
				STORE_ONE_SET(dsa, 0, vmm0, vmm1, vmm2, vmm3);
				STORE_ONE_SET(dsa, PAGE_SIZE, vmm4, vmm5, vmm6, vmm7);
				dsa += LARGE_LOAD_SIZE;
			} while(--count != 0);
			dsa += PAGE_SIZE;
			sra += PAGE_SIZE;
		} while(--szp != 0);
		_mm_sfence();
	}
loop_large_memcpy_tail:
#ifdef XMM_NEED_SAVE
	POP_XMM_ASM(xmm7, 0x10);
	POP_XMM_ASM(xmm6, 0);
	ADD_STACK_ASM(0xA0);
#endif
	while(xsun_likely(szq > 4 * VEC_SIZE)) {
		PREFETCH_ONE_SET(1, sra, 4 * VEC_SIZE);
		PREFETCH_ONE_SET(1, dsa, 4 * VEC_SIZE);
		asm volatile("");
		vmm0 = SIMD_OPT(loadu_ps)((float*)sra);
		vmm1 = SIMD_OPT(loadu_ps)((float*)(sra + VEC_SIZE));
		vmm2 = SIMD_OPT(loadu_ps)((float*)(sra + 2 * VEC_SIZE));
		vmm3 = SIMD_OPT(loadu_ps)((float*)(sra + 3 * VEC_SIZE));
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

