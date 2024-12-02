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


/* branch_copy 更快, 避免使用跳表!!! */

/* 跳表最快的版本 size <= 32 */
__attribute__((optimize("align-functions=4096")))
void* switch_copy(void *dst, const void *src, size_t size) {
	uint64_t r9, r10;
	uint32_t r9d, r10d;
	uint16_t r9w, r10w;
	uint8_t r9b, r10b;
	uint8_t *rdi = (uint8_t*)dst, *rsi = (uint8_t*)src;
	__m128 xmm3, xmm4;
	switch(size) {
	case 16:
	case 15:// L8
	case 14:
	case 13:
	case 12:
	case 11:
	case 10:
	case 9:
	case 8:
		r9 = *(uint64_t*)rsi;
		r10 = *(uint64_t*)(rsi + size - 8);
		*(uint64_t*)rdi = r9;
		*(uint64_t*)(rdi + size - 8) = r10;
		return dst;
	case 0: return dst;
	case 7:// L4
	case 6:
	case 5:
	case 4:
		r9d = *(uint32_t*)rsi;
		r10d = *(uint32_t*)(rsi + size - 4);
		*(uint32_t*)rdi = r9d;
		*(uint32_t*)(rdi + size - 4) = r10d;
		return dst;
	case 3:
	case 2:
		r9w = *(uint16_t*)rsi;
		r10w = *(uint8_t*)(rsi + size - 2);
		*(uint16_t*)rdi = r9w;
		*(uint16_t*)(rdi + size - 2) = r10w;
		return dst;
	case 1:
		*(uint8_t*)dst = *(uint8_t*)src;
		return dst;
	default:// L16r
		if(xsun_likely(size <= 32)) {
			xmm3 = _mm_loadu_ps((float*)rsi);
			xmm4 = _mm_loadu_ps((float*)(rsi + size - 16));
			_mm_storeu_ps((float*)rdi, xmm3);
			_mm_storeu_ps((float*)(rdi + size - 16), xmm4);
		}
		return dst;
	}
}

/* 分支最快的版本 size <= 32 */
__attribute__((optimize("align-functions=4096")))
void* branch_copy(void *dst, const void *src, size_t size) {
	uint64_t r9, r10;
	uint32_t r9d, r10d;
	uint16_t r9w, r10w;
	uint8_t *rdi = (uint8_t*)dst, *rsi = (uint8_t*)src;
	__m128 xmm3, xmm4;
	if(xsun_likely((uint32_t)size <= 32)) {
		if(xsun_likely((uint32_t)size >= 16)) {
			xmm3 = _mm_loadu_ps((float*)rsi);
			xmm4 = _mm_loadu_ps((float*)(rsi + size - 16));
			_mm_storeu_ps((float*)rdi, xmm3);
			_mm_storeu_ps((float*)(rdi + size - 16), xmm4);
		}
		else if(xsun_unlikely((uint32_t)size >= 8)) {
			r9 = *(uint64_t*)rsi;
			r10 = *(uint64_t*)(rsi + size - 8);
			*(uint64_t*)rdi = r9;
			*(uint64_t*)(rdi + size - 8) = r10;
		}
		else if(xsun_unlikely((uint32_t)size >= 4)) {
			r9d = *(uint32_t*)rsi;
			r10d = *(uint32_t*)(rsi + size - 4);
			*(uint32_t*)rdi = r9d;
			*(uint32_t*)(rdi + size - 4) = r10d;
		}
		else if(xsun_unlikely((uint32_t)size >= 2)) {
			r9w = *(uint16_t*)rsi;
			r10w = *(uint16_t*)(rsi + size - 2);
			*(uint16_t*)rdi = r9w;
			*(uint16_t*)(rdi + size - 2) = r10w;
		}
		else if(xsun_likely((uint32_t)size >= 1)) {
			*rdi = *rsi;
		}
		return dst;
	}
	return dst;
}

#if VEC_SIZE != 16 && VEC_SIZE != 32 && VEC_SIZE != 64
# error Unsupported VEC_SIZE!
#endif

/* 最快的小尺寸拷贝 size <= 8*VEC_SIZE */
__attribute__((optimize("align-functions=4096")))
void* tiny_copy(void *dst_, const void *src_, size_t size) {
	uint8_t *dst = (uint8_t*)dst_, *src = (uint8_t*)src_;
	if(xsun_unlikely(size < 32)) {
#if VEC_SIZE > 16
		if(xsun_likely((uint32_t)size >= 16)) {
			__m128 xmm0 = _mm_loadu_ps((float*)src);
			__m128 xmm1 = _mm_loadu_ps((float*)(src + size - 16));
			_mm_storeu_ps((float*)dst, xmm0);
			_mm_storeu_ps((float*)(dst + size - 16), xmm1);
			return dst;
		}
#endif
		if(xsun_unlikely((uint32_t)size >= 8)) {
			uint64_t r9 = *(uint64_t*)src;
			uint64_t r10 = *(uint64_t*)(src + size - 8);
			*(uint64_t*)dst = r9;
			*(uint64_t*)(dst + size - 8) = r10;
		}
		else if(xsun_unlikely((uint32_t)size >= 4)) {
			uint32_t r9d = *(uint32_t*)src;
			uint32_t r10d = *(uint32_t*)(src + size - 4);
			*(uint32_t*)dst = r9d;
			*(uint32_t*)(dst + size - 4) = r10d;
		}
		else if(xsun_unlikely((uint32_t)size >= 2)) {
			uint16_t r9w = *(uint16_t*)src;
			uint16_t r10w = *(uint16_t*)(src + size - 2);
			*(uint16_t*)dst = r9w;
			*(uint16_t*)(dst + size - 2) = r10w;
		}
		else if(xsun_likely((uint32_t)size >= 1)) *dst = *src;
		return dst;
	}
	if(xsun_unlikely(size > 8 * VEC_SIZE)) return dst;// 远跳转
#if VEC_SIZE == 64
	if(xsun_unlikely((uint32_t)size < 64)) {
		__m256 ymm0 = _mm256_loadu_ps((float*)src);
		__m256 ymm1 = _mm256_loadu_ps((float*)(src + size - 32));
		_mm256_storeu_ps((float*)dst, ymm0);
		_mm256_storeu_ps((float*)(dst + size - 32), ymm1);
		VZEROUPPER_ASM;
		return dst;
	}
#endif
	VMM_TYPE() vmm0, vmm1, vmm2, vmm3;
	vmm0 = SIMD_OPT(loadu_ps)((float*)src);
	vmm1 = SIMD_OPT(loadu_ps)((float*)(src + size - VEC_SIZE));
	SIMD_OPT(storeu_ps)((float*)dst, vmm0);
	SIMD_OPT(storeu_ps)((float*)(dst + size - VEC_SIZE), vmm1);
	if(xsun_likely((uint32_t)size > 2 * VEC_SIZE)) {
		vmm0 = SIMD_OPT(loadu_ps)((float*)(src + VEC_SIZE));
		vmm1 = SIMD_OPT(loadu_ps)((float*)(src + size - 2 * VEC_SIZE));
		SIMD_OPT(storeu_ps)((float*)(dst + VEC_SIZE), vmm0);
		SIMD_OPT(storeu_ps)((float*)(dst + size - 2 * VEC_SIZE), vmm1);
#if VEC_SIZE == 32
		asm volatile(ASM_OPT(
			add %[i], %[size];// 四字节指令而不是七字节
			jle 0f) : [size]"+r"(size) : [i]"i"(-4*VEC_SIZE)
		);// SIMD 首指令地址需对齐 8 字节, 否则会效率低下
		vmm0 = SIMD_OPT(loadu_ps)((float*)(src + 2 * VEC_SIZE));
		vmm1 = SIMD_OPT(loadu_ps)((float*)(src + 3 * VEC_SIZE));
		vmm2 = SIMD_OPT(loadu_ps)((float*)(src + size));
		vmm3 = SIMD_OPT(loadu_ps)((float*)(src + size + VEC_SIZE));
		SIMD_OPT(storeu_ps)((float*)(dst + 2 * VEC_SIZE), vmm0);
		SIMD_OPT(storeu_ps)((float*)(dst + 3 * VEC_SIZE), vmm1);
		SIMD_OPT(storeu_ps)((float*)(dst + size), vmm2);
		SIMD_OPT(storeu_ps)((float*)(dst + size + VEC_SIZE), vmm3);
		asm volatile("0:"::);
#else
		if(xsun_likely((uint32_t)size > 4 * VEC_SIZE)) {
			vmm0 = SIMD_OPT(loadu_ps)((float*)(src + size - 3 * VEC_SIZE));
			vmm1 = SIMD_OPT(loadu_ps)((float*)(src + size - 4 * VEC_SIZE));
			vmm2 = SIMD_OPT(loadu_ps)((float*)(src + 2 * VEC_SIZE));
			vmm3 = SIMD_OPT(loadu_ps)((float*)(src + 3 * VEC_SIZE));
			SIMD_OPT(storeu_ps)((float*)(dst + size - 3 * VEC_SIZE), vmm0);
			SIMD_OPT(storeu_ps)((float*)(dst + size - 4 * VEC_SIZE), vmm1);
			SIMD_OPT(storeu_ps)((float*)(dst + 2 * VEC_SIZE), vmm2);
			SIMD_OPT(storeu_ps)((float*)(dst + 3 * VEC_SIZE), vmm3);
		}
#endif
	}
	VZEROUPPER_ASM;
	return dst;
}


