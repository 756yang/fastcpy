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


__attribute__((optimize("align-functions=4096"),target("arch=x86-64-v2")))
void* memcpy_jart(void *dest, const void *src, size_t size) {
	uint64_t r9, r10;
	uint32_t r9d, r10d;
	uint16_t r9w, r10w;
	uint8_t r9b, r10b;
	uint8_t *rdi = (uint8_t*)dest, *rsi = (uint8_t*)src;
	__m128 xmm3, xmm4;
	switch(size) {
	/*case 31:// L16
	case 30:
	case 29:
	case 28:
	case 27:
	case 26:
	case 25:
	case 24:
	case 23:
	case 22:
	case 21:
	case 20:
	case 19:
	case 18:
	case 17:
	case 16:
L16:
		xmm4 = _mm_loadu_ps((float*)(rsi + size - 16));
		for(r9 = 16; ; ) {
			r9 += 16;
			xmm3 = _mm_loadu_ps((float*)(rsi + r9 - 32));
			_mm_storeu_ps((float*)(rdi + r9 - 32), xmm3);
			if(size <= r9) break;
		}
		_mm_storeu_ps((float*)(rdi + size - 16), xmm4);
		return dest;*/
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
		return dest;
	case 0:
		return dest;
	case 7:// L4
	case 6:
	case 5:
	case 4:
		r9d = *(uint32_t*)rsi;
		r10d = *(uint32_t*)(rsi + size - 4);
		*(uint32_t*)rdi = r9d;
		*(uint32_t*)(rdi + size - 4) = r10d;
		return dest;
	case 3:
	case 2:
		r9w = *(uint16_t*)rsi;
		r10w = *(uint8_t*)(rsi + size - 2);
		*(uint16_t*)rdi = r9w;
		*(uint16_t*)(rdi + size - 2) = r10w;
		return dest;
	case 1:
		*(uint8_t*)dest = *(uint8_t*)src;
		return dest;
	default:// L16r
		if(xsun_likely((size <= 4*1024*1024))) {// 最优值为共享缓存大小除以核心数
			xmm4 = _mm_loadu_ps((float*)(rsi + size - 16));
			for(r9 = 16; ; ) {
				r9 += 16;
				xmm3 = _mm_loadu_ps((float*)(rsi + r9 - 32));
				_mm_storeu_ps((float*)(rdi + r9 - 32), xmm3);
				if(size <= r9) break;
			}
			_mm_storeu_ps((float*)(rdi + size - 16), xmm4);
		}
		/*else if(xsun_likely(size <= 4*1024*1024)) {
			asm volatile("rep movsb"
			: "+D"(rdi), "+S"(rsi), "+c"(size)
			:
			: "memory");
		}*/
		else {
			xmm3 = _mm_loadu_ps((float*)rsi);
			_mm_storeu_ps((float*)rdi, xmm3);
			r9 = 16 - ((size_t)rdi & 15);
			rdi += r9;
			rsi += r9;
			size -= r9;
			for(r9 = 16; ; ) {
				r9 += 16;
				xmm3 = _mm_loadu_ps((float*)(rsi + r9 - 32));
				_mm_stream_ps((float*)(rdi + r9 - 32), xmm3);
				if(size <= r9) break;
			}
			_mm_sfence();
			xmm3 = _mm_loadu_ps((float*)(rsi + size - 16));
			_mm_storeu_ps((float*)(rdi + size - 16), xmm3);
		}
		return dest;
	}
}

/* 这个是最优算法, 最小代码尺寸的 memcpy 可基于此编写汇编 */
__attribute__((optimize("align-functions=4096"),target("arch=x86-64-v3")))
void* memcpy_jart1(void *dest, const void *src, size_t size) {
	uint64_t r9, r10;
	uint32_t r9d, r10d;
	uint16_t r9w, r10w;
	uint8_t r9b, r10b;
	uint8_t *rdi = (uint8_t*)dest, *rsi = (uint8_t*)src;
	__m128 xmm3, xmm4;
	__m256 ymm3, ymm4;
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
		return dest;
	case 0: return dest;
	case 7:// L4
	case 6:
	case 5:
	case 4:
		r9d = *(uint32_t*)rsi;
		r10d = *(uint32_t*)(rsi + size - 4);
		*(uint32_t*)rdi = r9d;
		*(uint32_t*)(rdi + size - 4) = r10d;
		return dest;
	case 3:
	case 2:
		r9w = *(uint16_t*)rsi;
		r10w = *(uint8_t*)(rsi + size - 2);
		*(uint16_t*)rdi = r9w;
		*(uint16_t*)(rdi + size - 2) = r10w;
		return dest;
	case 1:
		*(uint8_t*)dest = *(uint8_t*)src;
		return dest;
	default:// L16r
		if(xsun_likely(size <= 32)) {
			xmm3 = _mm_loadu_ps((float*)rsi);
			xmm4 = _mm_loadu_ps((float*)(rsi + size - 16));
			_mm_storeu_ps((float*)rdi, xmm3);
			_mm_storeu_ps((float*)(rdi + size - 16), xmm4);
		}
		else if(xsun_likely((size <= 4*1024*1024))) {
			ymm4 = _mm256_loadu_ps((float*)(rsi + size - 32));
			for(r9 = 32; ; ) {
				r9 += 32;
				ymm3 = _mm256_loadu_ps((float*)(rsi + r9 - 64));
				_mm256_storeu_ps((float*)(rdi + r9 - 64), ymm3);
				if(size <= r9) break;
			}
			_mm256_storeu_ps((float*)(rdi + size - 32), ymm4);
		}
		else {
			ymm3 = _mm256_loadu_ps((float*)rsi);
			_mm256_storeu_ps((float*)rdi, ymm3);
			r9 = 32 - ((size_t)rdi & 31);
			rdi += r9;
			rsi += r9;
			size -= r9;
			for(r9 = 32; ; ) {
				r9 += 32;
				ymm3 = _mm256_loadu_ps((float*)(rsi + r9 - 64));
				_mm256_stream_ps((float*)(rdi + r9 - 64), ymm3);
				if(size <= r9) break;
			}
			_mm_sfence();
			ymm3 = _mm256_loadu_ps((float*)(rsi + size - 32));
			_mm256_storeu_ps((float*)(rdi + size - 32), ymm3);
		}
		return dest;
	}
}


