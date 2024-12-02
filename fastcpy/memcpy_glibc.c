#include "memcpy_glibc.h"
#include <stddef.h>
#include <stdint.h>
#include <immintrin.h>

/* memmove-vec-unaligned-erms.S 汇编移植到 C 语言, rep movsb 未移植 */

#ifndef INLINE
# if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
#   define INLINE __inline__ __attribute__((always_inline))
# elif defined(_MSC_VER)
#   define INLINE __forceinline
# else
#   define INLINE inline
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

struct memcpy_glibc_config __memcpy_glibc_config = {
	.__x86_shared_non_temporal_threshold = 4 * 1024 * 1024,
};


INLINE static uint8_t* memcpy_less_vec(uint8_t *dst, const uint8_t *src, size_t size) {
	uint64_t r9, r10;
	uint32_t r9d, r10d;
	uint16_t r9w, r10w;
#if VEC_SIZE != 16 && VEC_SIZE != 32 && VEC_SIZE != 64
# error Unsupported VEC_SIZE!
#endif
#if VEC_SIZE > 32
	if(xsun_unlikely((uint32_t)size >= 32)) {
		register __m256 vmm0 asm("ymm0");
		register __m256 vmm1 asm("ymm1");
		vmm0 = _mm256_loadu_ps((float*)src);
		vmm1 = _mm256_loadu_ps((float*)(src + size - 32));
		_mm256_storeu_ps((float*)dst, vmm0);
		_mm256_storeu_ps((float*)(dst + size - 32), vmm1);
		VZEROUPPER_ASM;
		return dst;
	}
#endif
#if VEC_SIZE > 16
	if(xsun_unlikely((uint32_t)size >= 16)) {
		register __m128 vmm0 asm("xmm0");
		register __m128 vmm1 asm("xmm1");
		vmm0 = _mm_loadu_ps((float*)src);
		vmm1 = _mm_loadu_ps((float*)(src + size - 16));
		_mm_storeu_ps((float*)dst, vmm0);
		_mm_storeu_ps((float*)(dst + size - 16), vmm1);
		return dst;
	}
#endif
	if(xsun_unlikely((uint32_t)size >= 8)) {
		r9 = *(uint64_t*)src;
		r10 = *(uint64_t*)(src + size - 8);
		*(uint64_t*)dst = r9;
		*(uint64_t*)(dst + size - 8) = r10;
		return dst;
	}
	if(xsun_unlikely((uint32_t)size >= 4)) {
		r9d = *(uint32_t*)src;
		r10d = *(uint32_t*)(src + size - 4);
		*(uint32_t*)dst = r9d;
		*(uint32_t*)(dst + size - 4) = r10d;
		return dst;
	}
	if(xsun_unlikely((uint32_t)size >= 2)) {
		r9w = *(uint16_t*)src;
		r10w = *(uint16_t*)(src + size - 2);
		*(uint16_t*)dst = r9w;
		*(uint16_t*)(dst + size - 2) = r10w;
		return dst;
	}
	if(xsun_likely((uint32_t)size >= 1)) *dst = *src;
	return dst;
}

INLINE static uint8_t* memcpy_more_8x_vec_forward(uint8_t *dst, const uint8_t *src, size_t size, VMM_TYPE() vmm0) {
	register VMM_TYPE() vmm1 asm(VMM_NAME "1");
	register VMM_TYPE() vmm2 asm(VMM_NAME "2");
	register VMM_TYPE() vmm3 asm(VMM_NAME "3");
	register VMM_TYPE() vmm4 asm(VMM_NAME "4");
	register VMM_TYPE() vmm5 asm(VMM_NAME "5");
	register VMM_TYPE() vmm6 asm(VMM_NAME "6");
	register VMM_TYPE() vmm7 asm(VMM_NAME "7");
	register VMM_TYPE() vmm8 asm(VMM_NAME "8");
#ifdef XMM_NEED_SAVE
	ADD_STACK_ASM(-0x30);
	PUSH_XMM_ASM(xmm6, 0);
	PUSH_XMM_ASM(xmm7, 0x10);
	PUSH_XMM_ASM(xmm8, 0x20);
#endif
	vmm5 = SIMD_OPT(loadu_ps)((float*)(src + size - VEC_SIZE));
	vmm6 = SIMD_OPT(loadu_ps)((float*)(src + size - 2 * VEC_SIZE));
	uint8_t *dsa = (uint8_t*)((size_t)dst | VEC_SIZE - 1) + 1;// 对齐 dst
	vmm7 = SIMD_OPT(loadu_ps)((float*)(src + size - 3 * VEC_SIZE));
	vmm8 = SIMD_OPT(loadu_ps)((float*)(src + size - 4 * VEC_SIZE));
	const uint8_t *sra = src + (dsa - dst);// 对齐 src
	uint8_t *dsz = dst + size - 4 * VEC_SIZE;// dst 末尾
	do {
		vmm1 = SIMD_OPT(loadu_ps)((float*)sra);
		vmm2 = SIMD_OPT(loadu_ps)((float*)(sra + VEC_SIZE));
		vmm3 = SIMD_OPT(loadu_ps)((float*)(sra + 2 * VEC_SIZE));
		vmm4 = SIMD_OPT(loadu_ps)((float*)(sra + 3 * VEC_SIZE));
		sra += 4 * VEC_SIZE;
		SIMD_OPT(store_ps)((float*)dsa, vmm1);
		SIMD_OPT(store_ps)((float*)(dsa + VEC_SIZE), vmm2);
		SIMD_OPT(store_ps)((float*)(dsa + 2 * VEC_SIZE), vmm3);
		SIMD_OPT(store_ps)((float*)(dsa + 3 * VEC_SIZE), vmm4);
		dsa += 4 * VEC_SIZE;
	} while(dsz > dsa);
	SIMD_OPT(storeu_ps)((float*)(dsz + 3 * VEC_SIZE), vmm5);
	SIMD_OPT(storeu_ps)((float*)(dsz + 2 * VEC_SIZE), vmm6);
	SIMD_OPT(storeu_ps)((float*)(dsz + VEC_SIZE), vmm7);
	SIMD_OPT(storeu_ps)((float*)dsz, vmm8);
	SIMD_OPT(storeu_ps)((float*)dst, vmm0);
#ifdef XMM_NEED_SAVE
	POP_XMM_ASM(xmm8, 0x20);
	POP_XMM_ASM(xmm7, 0x10);
	POP_XMM_ASM(xmm6, 0);
	ADD_STACK_ASM(0x30);
#endif
	VZEROUPPER_ASM;
	return dst;
}

INLINE static uint8_t* memcpy_more_8x_vec_backward(uint8_t *dst, const uint8_t *src, size_t size, VMM_TYPE() vmm0) {
	register VMM_TYPE() vmm1 asm(VMM_NAME "1");
	register VMM_TYPE() vmm2 asm(VMM_NAME "2");
	register VMM_TYPE() vmm3 asm(VMM_NAME "3");
	register VMM_TYPE() vmm4 asm(VMM_NAME "4");
	register VMM_TYPE() vmm5 asm(VMM_NAME "5");
	register VMM_TYPE() vmm6 asm(VMM_NAME "6");
	register VMM_TYPE() vmm7 asm(VMM_NAME "7");
	register VMM_TYPE() vmm8 asm(VMM_NAME "8");
#ifdef XMM_NEED_SAVE
	ADD_STACK_ASM(-0x30);
	PUSH_XMM_ASM(xmm6, 0);
	PUSH_XMM_ASM(xmm7, 0x10);
	PUSH_XMM_ASM(xmm8, 0x20);
#endif
	vmm5 = SIMD_OPT(loadu_ps)((float*)(src + VEC_SIZE));
	vmm6 = SIMD_OPT(loadu_ps)((float*)(src + 2 * VEC_SIZE));
	uint8_t *dsa = dst + size - 4 * VEC_SIZE - 1;// 预计对齐 dst
	vmm7 = SIMD_OPT(loadu_ps)((float*)(src + 3 * VEC_SIZE));
	vmm8 = SIMD_OPT(loadu_ps)((float*)(src + size - VEC_SIZE));
	dsa = (uint8_t*)((size_t)dsa & -VEC_SIZE);
	const uint8_t *sra = src + (dsa - dst);// 对齐 src
	do {
		vmm1 = SIMD_OPT(loadu_ps)((float*)(sra + 3 * VEC_SIZE));
		vmm2 = SIMD_OPT(loadu_ps)((float*)(sra + 2 * VEC_SIZE));
		vmm3 = SIMD_OPT(loadu_ps)((float*)(sra + VEC_SIZE));
		vmm4 = SIMD_OPT(loadu_ps)((float*)sra);
		sra -= 4 * VEC_SIZE;
		SIMD_OPT(store_ps)((float*)(dsa + 3 * VEC_SIZE), vmm1);
		SIMD_OPT(store_ps)((float*)(dsa + 2 * VEC_SIZE), vmm2);
		SIMD_OPT(store_ps)((float*)(dsa + VEC_SIZE), vmm3);
		SIMD_OPT(store_ps)((float*)dsa, vmm4);
		dsa -= 4 * VEC_SIZE;
	} while(dst < dsa);
	SIMD_OPT(storeu_ps)((float*)dst, vmm0);
	SIMD_OPT(storeu_ps)((float*)(dst + VEC_SIZE), vmm5);
	SIMD_OPT(storeu_ps)((float*)(dst + 2 * VEC_SIZE), vmm6);
	SIMD_OPT(storeu_ps)((float*)(dst + 3 * VEC_SIZE), vmm7);
	SIMD_OPT(storeu_ps)((float*)(dst + size - VEC_SIZE), vmm8);
#ifdef XMM_NEED_SAVE
	POP_XMM_ASM(xmm8, 0x20);
	POP_XMM_ASM(xmm7, 0x10);
	POP_XMM_ASM(xmm6, 0);
	ADD_STACK_ASM(0x30);
#endif
	VZEROUPPER_ASM;
	return dst;
}


#ifdef __cplusplus
extern "C"
#endif/* memcpy_large_memcpy 有一个参数以寄存器 xmm0 传递!!! */
uint8_t* memcpy_large_memcpy(uint8_t *dst, const uint8_t *src, size_t size/*, VMM_TYPE() vmm0*/);

INLINE static uint8_t* memcpy_more_8x_vec(uint8_t *dst, const uint8_t *src, size_t size, VMM_TYPE() vmm0) {
	size_t diff = dst - src;
	if(xsun_unlikely(diff < size)) {
		if(xsun_unlikely(diff == 0)) return dst;
		return memcpy_more_8x_vec_backward(dst, src, size, vmm0);
	}
	if(xsun_unlikely(size > __memcpy_glibc_config.__x86_shared_non_temporal_threshold)) {
		if(xsun_unlikely(size > -diff))
			return memcpy_more_8x_vec_forward(dst, src, size, vmm0);
		else return memcpy_large_memcpy(dst, src, size/*, vmm0*/);
	}
	uint32_t r8 = (diff + size ^ diff) >> 63;// r8 值 1 必须前向复制
	// 4k 伪混叠可以後向复制消除
	//if(xsun_unlikely((diff & 0xf00) + r8 == 0))
	// 0xf00 这个值应测试一下, 理论应是 8 * VEC_SIZE 的整数倍
	if(xsun_unlikely(((uint32_t)diff & PAGE_SIZE - 16 * VEC_SIZE) + r8 == 0))
		return memcpy_more_8x_vec_backward(dst, src, size, vmm0);
	return memcpy_more_8x_vec_forward(dst, src, size, vmm0);
}

/* no_callee_saved_registers 函数允许破坏所有寄存器 */
__attribute__((optimize("align-functions=4096")))
X64_CALL void* memcpy_glibc(void *dst_, const void *src_, size_t size) {
	uint8_t *dst = (uint8_t*)dst_, *src = (uint8_t*)src_;
	if(xsun_unlikely(size < VEC_SIZE)) return memcpy_less_vec(dst, src, size);
	register VMM_TYPE() vmm0 asm(VMM_NAME "0");// 固定寄存器位置
	register VMM_TYPE() vmm1 asm(VMM_NAME "1");
	register VMM_TYPE() vmm2 asm(VMM_NAME "2");
	register VMM_TYPE() vmm3 asm(VMM_NAME "3");
	register VMM_TYPE() vmm4 asm(VMM_NAME "4");
	register VMM_TYPE() vmm5 asm(VMM_NAME "5");
	register VMM_TYPE() vmm6 asm(VMM_NAME "6");
	register VMM_TYPE() vmm7 asm(VMM_NAME "7");
#if 0/* 两者都可以 */
	vmm0 = SIMD_OPT(loadu_ps)((float*)src);
#else
	asm volatile(
#if VEC_SIZE == 64
		"vmovups (%[s]), %%zmm0\n\t"
		:"=v"(vmm0)
#elif defined(__AVX__)
		"vmovups (%[s]), %%ymm0\n\t"
		:"=x"(vmm0)
#elif defined(__SSE__) && VEC_SIZE == 16
		"movups (%[s]), %%xmm0\n\t"
		:"=x"(vmm0)
#else
#error Unsupported VEC_SIZE!
#endif
		:[s]"r"(src):
	);
#endif
	if(xsun_likely(size <= 2 * VEC_SIZE)) {
		vmm1 = SIMD_OPT(loadu_ps)((float*)(src + size - VEC_SIZE));
		SIMD_OPT(storeu_ps)((float*)dst, vmm0);
		SIMD_OPT(storeu_ps)((float*)(dst + size - VEC_SIZE), vmm1);
		VZEROUPPER_ASM;
		return dst;
	}
	if(xsun_unlikely(size > 8 * VEC_SIZE))
		return memcpy_more_8x_vec(dst, src, size, vmm0);
	vmm1 = SIMD_OPT(loadu_ps)((float*)(src + VEC_SIZE));
	if(xsun_likely(size > 4 * VEC_SIZE)) {
#ifdef XMM_NEED_SAVE
		ADD_STACK_ASM(-0x20);
		PUSH_XMM_ASM(xmm6, 0);
		PUSH_XMM_ASM(xmm7, 0x10);
#endif
		vmm2 = SIMD_OPT(loadu_ps)((float*)(src + 2 * VEC_SIZE));
		vmm3 = SIMD_OPT(loadu_ps)((float*)(src + 3 * VEC_SIZE));
		vmm4 = SIMD_OPT(loadu_ps)((float*)(src + size - VEC_SIZE));
		vmm5 = SIMD_OPT(loadu_ps)((float*)(src + size - 2 * VEC_SIZE));
		vmm6 = SIMD_OPT(loadu_ps)((float*)(src + size - 3 * VEC_SIZE));
		vmm7 = SIMD_OPT(loadu_ps)((float*)(src + size - 4 * VEC_SIZE));
		SIMD_OPT(storeu_ps)((float*)dst, vmm0);
		SIMD_OPT(storeu_ps)((float*)(dst + VEC_SIZE), vmm1);
		SIMD_OPT(storeu_ps)((float*)(dst + 2 * VEC_SIZE), vmm2);
		SIMD_OPT(storeu_ps)((float*)(dst + 3 * VEC_SIZE), vmm3);
		SIMD_OPT(storeu_ps)((float*)(dst + size - VEC_SIZE), vmm4);
		SIMD_OPT(storeu_ps)((float*)(dst + size - 2 * VEC_SIZE), vmm5);
		SIMD_OPT(storeu_ps)((float*)(dst + size - 3 * VEC_SIZE), vmm6);
		SIMD_OPT(storeu_ps)((float*)(dst + size - 4 * VEC_SIZE), vmm7);
#ifdef XMM_NEED_SAVE
		POP_XMM_ASM(xmm7, 0x10);
		POP_XMM_ASM(xmm6, 0);
		ADD_STACK_ASM(0x20);
#endif
	}
	else {
		vmm4 = SIMD_OPT(loadu_ps)((float*)(src + size - VEC_SIZE));
		vmm5 = SIMD_OPT(loadu_ps)((float*)(src + size - 2 * VEC_SIZE));
		SIMD_OPT(storeu_ps)((float*)dst, vmm0);
		SIMD_OPT(storeu_ps)((float*)(dst + VEC_SIZE), vmm1);
		SIMD_OPT(storeu_ps)((float*)(dst + size - VEC_SIZE), vmm4);
		SIMD_OPT(storeu_ps)((float*)(dst + size - 2 * VEC_SIZE), vmm5);
	}
	VZEROUPPER_ASM;
	return dst;
}










