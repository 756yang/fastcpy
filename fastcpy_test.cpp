
#include "fastcpy/fastcpy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <immintrin.h>

#ifndef INLINE
# if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
#   define INLINE __inline__ __attribute__((always_inline))
# elif defined(_MSC_VER)
#   define INLINE __forceinline
# else
#   define INLINE inline
# endif
#endif

#ifndef NOINLINE
# if defined(__clang__) || defined(__GNUC__) || defined(__GNUG__)
#   define NOINLINE __attribute__((__noinline__))
# elif defined(_MSC_VER)
#   define NOINLINE __noinline
# else
#   define NOINLINE
# endif
#endif

/* 简单的 64 位随机数生成 */
INLINE static uint64_t xorshift64(uint64_t x) {
	x ^= x << 7;
	x ^= x >> 9;
	return x;
}

/**
 * 缓冲区填充随机数
 * @buf     缓冲区地址
 * @len     需填充的长度
 * @state   随机数状态
 * @return  返回随机数状态
 */
NOINLINE static uint64_t rand_fill(void *buf, size_t len, uint64_t state) {
	while(1) {
		state = xorshift64(state);
		switch(len) {
		case 8:
			*(uint64_t*)buf = state;
			[[fallthrough]];
		case 0:
			return state;
		case 3:
			*(uint16_t*)buf = (uint16_t)state;
			*((uint8_t*)buf + 2) = (uint8_t)(state = state << 32 | state >> 32);
			return state;
		case 5:
			*(uint32_t*)((uint8_t*)buf + 1) = (uint32_t)state;
			state = state << 32 | state >> 32;
			[[fallthrough]];
		case 1:
			*(uint8_t*)buf = (uint8_t)state;
			return state;
		case 6:
			*(uint32_t*)((uint8_t*)buf + 2) = (uint32_t)state;
			state = state << 32 | state >> 32;
			[[fallthrough]];
		case 2:
			*(uint16_t*)buf = (uint16_t)state;
			return state;
		case 7:
			*(uint32_t*)((uint8_t*)buf + 3) = (uint32_t)state;
			state = state << 32 | state >> 32;
			[[fallthrough]];
		case 4:
			*(uint32_t*)buf = (uint32_t)state;
			return state;
		default:
			while(len > sizeof(uint64_t)) {
				*(uint64_t*)buf = state;
				buf = (uint8_t*)buf + sizeof(uint64_t);
				state = xorshift64(state);
				len -= sizeof(uint64_t);
			}
		}
	}
}


uint32_t simd_crc32(uint32_t crc,void *buf,size_t n){
	switch(n){
	case 0: return crc;
	case 4:
		return crc=_mm_crc32_u32(crc,*(uint32_t*)buf);
	case 1:
		return crc=_mm_crc32_u8(crc,*(uint8_t*)buf);
	case 2:
		return crc=_mm_crc32_u16(crc,*(uint16_t*)buf);
	case 8:
		return crc=_mm_crc32_u64(crc,*(uint64_t*)buf);
	case 3:
		crc=_mm_crc32_u16(crc,*(uint16_t*)buf);
		return crc=_mm_crc32_u8(crc,((uint8_t*)buf)[2]);
	default:
		if((n&(ptrdiff_t)(-0x8))==0){
			crc = simd_crc32(crc,buf,4);
			return simd_crc32(crc,(uint8_t*)buf+4,n-4);
		}
		do{
			crc=_mm_crc32_u64(crc,*(uint64_t*)buf);
			buf=(uint8_t*)buf+8; n-=8;
		}while(n>8);
		return simd_crc32(crc,buf,n);
	}
}



int main() {
	uint64_t seed = 46541;
	uint32_t i, j, k, crc_src, crc_dst, mlen = 4096*6;
	uint8_t *dst, *src;
	dst = (uint8_t*)malloc(mlen);
	src = (uint8_t*)malloc(mlen);
	for(i = 0; i <= 4096*2; ++i) {
		memset(dst, -1, mlen);
		memset(src, -1, mlen);
		seed = xorshift64(seed);
		j = 2048 + seed % (mlen - 4096 - i + 1);
		seed = xorshift64(seed);
		k = 2048 + seed % (mlen - 4096 - i + 1);
		seed = rand_fill(src + j, i, seed);
		crc_src = simd_crc32(0, src + j - 2048, i + 4096);
		if((uint8_t*)fastcpy(dst + k, src + j, i) != dst + k)
			printf("fastcpy return address is not equal origin dest!\n");
		crc_dst = simd_crc32(0, dst + k - 2048, i + 4096);
		if(crc_dst != crc_src) printf("fastcpy dest memory copy error!\n");
		crc_src = simd_crc32(0, src + j - 2048, i + 4096);
		if(crc_dst != crc_src) printf("fastcpy src@dest memory copy error!\n");
	}
	free(dst);
	free(src);
	uint32_t MB = 1024*1024;
	dst = (uint8_t*)malloc(96*MB);
	src = (uint8_t*)malloc(96*MB);
	for(i = 0; i < 1024; ++i) {
		memset(dst, -1, 96*MB);
		memset(src, -1, 96*MB);
		seed = xorshift64(seed);
		j = MB + seed % (96*MB - 6*MB + 1);
		seed = xorshift64(seed);
		k = MB + seed % (j - MB + 1);
		seed = rand_fill(src + j, 96*MB - MB - j, seed);
		crc_src = simd_crc32(0, src + j - MB, 96*MB - MB - j + 2*MB);
		if((uint8_t*)fastcpy(dst + k, src + j, 96*MB - MB - j) != dst + k)
			printf("large_memcpy return address is not equal origin dest!\n");
		crc_dst = simd_crc32(0, dst + k - MB, 96*MB - MB - j + 2*MB);
		if(crc_dst != crc_src) printf("large_memcpy dest memory copy error!\n");
		crc_src = simd_crc32(0, src + j - MB, 96*MB - MB - j + 2*MB);
		if(crc_dst != crc_src) printf("large_memcpy src@dest memory copy error!\n");
	}
	free(dst);
	free(src);
	printf("finished!\n");
	return 0;
}






