#include "benchmark/benchmark.h"

// .\fastcpy_bench.exe --benchmark_repetitions=5 --benchmark_report_aggregates_only=true --benchmark_out_format=csv --benchmark_out=memcpy_bench.csv

#include "./memcpy_bench.h"
#include "malloc_user.h"
#include "fastcpy/memcpy_glibc.h"
#include "fastcpy/fastcpy.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>


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
#   define NOINLINE __declspec(noinline)
# else
#   define NOINLINE
# endif
#endif


typedef void* (*copy_fn)(void*__restrict, const void*__restrict, size_t);

extern "C" void * AVX_memcpy(void *dest, const void *src, size_t numbytes);

#include "ck_memcpy/memcpy-impl.h"
// memcpy_erms memcpy_trivial memcpySSE2 memcpyAVX2
// memcpySSE2Unrolled2 memcpySSE2Unrolled4 memcpySSE2Unrolled8
// memcpyAVXUnrolled2 memcpyAVXUnrolled4 memcpyAVXUnrolled8
// memcpy_my memcpy_my2 memcpy_kernel

// SSE 版最小 memcpy
extern "C" void* memcpy_jart(void *dest, const void *src, size_t size);
// AVX 版最小 memcpy
extern "C" void* memcpy_jart1(void *dest, const void *src, size_t size);

#include "FastMemcpy/FastMemcpy.h"
// memcpy_fast_sse memcpy_fast_avx

#include "memcpy-amd64/memcpy.h"
// custom_memcpy

#include "rte_memcpy/rte_memcpy.h"
// rte_memcpy_avx rte_memcpy_sse


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
__attribute__((optimize("align-functions=4096")))
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

// 2M 全局内存池
#define MEM_HEAP_SIZE 2*1024*1024

#define MEM_HEAP_BLOCK_SIZE 4096

// 程序运行之前开辟的内存池, 避免 malloc 影响性能测试
//mempool mem_heap(MEM_HEAP_SIZE);

/**
 * 文本拼接场景下 memcpy 的性能测试
 * @max_text_len    一个文本的长度最大值
 * @sum_text_len    所有文本总长度最大值
 * @fn              待测试 memcpy 实现的函数指针
 */
__attribute__((optimize("align-functions=4096")))
NOINLINE static size_t test_text_cat_process(mempool *heap_ptr, size_t max_text_len, size_t sum_text_len, copy_fn fn) {
	assert(max_text_len != 0 && max_text_len <= 4096 && sum_text_len > max_text_len);
	//assert(heap_ptr != &mem_heap || sum_text_len <= MEM_HEAP_SIZE / 3 - MEM_HEAP_BLOCK_SIZE);
	uint64_t seed = 1;
	size_t buflen, text_size = MEM_HEAP_BLOCK_SIZE, text_len = 0, size_sum = 0;
	char *text, *buf, *temp;
	temp = (char*)heap_ptr->memalloc(text_size);
	assert(temp != NULL);
	text = temp;
	do {
		seed = xorshift64(seed);
		buflen = seed % max_text_len + 1;
		temp = (char*)heap_ptr->memalloc(buflen);
		assert(temp != NULL);
		buf = temp;
		seed = rand_fill(buf, buflen, seed);
		if(text_len + buflen <= text_size) {
			fn(text + text_len, buf, buflen);
			size_sum += buflen;
		}
		else {
			text_size += MEM_HEAP_BLOCK_SIZE;
			temp = (char*)heap_ptr->memalloc(text_size);
			assert(temp != NULL);
			temp = (char*)fn(temp, text, text_len);
			heap_ptr->memfree(text);
			fn((text = temp) + text_len, buf, buflen);
			size_sum += text_len + buflen;
		}
		text_len = text_len + buflen;
		heap_ptr->memfree(buf);
	} while(text_len < sum_text_len);
	heap_ptr->memfree(text);
	return size_sum;
}

// 函数地址无法作为模板参数
template <int FN = 0, typename ...ARGS>
__attribute__((optimize("align-functions=4096")))
static void bench_text_cat(benchmark::State& state, size_t mt_len, size_t st_len, ARGS&&... args) {
	mempool mem_heap((st_len + MEM_HEAP_BLOCK_SIZE) * 3 + 2048);
	size_t size_sum;
	if constexpr(FN == 0) {
		size_sum = test_text_cat_process(&mem_heap, mt_len, st_len, std::forward<ARGS>(args)...);
	}
	else if constexpr(FN == MEMCPY_ERMS) {
		size_sum = test_text_cat_process(&mem_heap, mt_len, st_len, memcpy_erms);
	}
	else if constexpr(FN == MEMCPY_TRIVIAL) {
		size_sum = test_text_cat_process(&mem_heap, mt_len, st_len, memcpy_trivial);
	}
#ifdef __AVX__
	asm volatile("vzeroupper\n\t"::);
#endif
	for(auto _ : state) {
		if constexpr(FN == 0) {
			test_text_cat_process(&mem_heap, mt_len, st_len, std::forward<ARGS>(args)...);
		}
		else if constexpr(FN == MEMCPY_ERMS) {
			test_text_cat_process(&mem_heap, mt_len, st_len, memcpy_erms);
		}
		else if constexpr(FN == MEMCPY_TRIVIAL) {
			test_text_cat_process(&mem_heap, mt_len, st_len, memcpy_trivial);
		}
	}
	// 性能基准的单个测试的迭代次数是自动计算的
	// 通过调用测试用例估计合适的迭代次数
	// 以估计的迭代次数运行测试用例并作为结果计算
	// printf("%d\n", state.iterations());
	state.SetBytesProcessed(state.iterations() * size_sum);
}


/**
 * 内存块复制 memcpy 的性能测试
 * @min_len     一个块的最小长度, 必须是 2 的幂次
 * @max_len     一个块的最大长度, 必须是 2 的幂次
 * @fn          待测试 memcpy 实现的函数指针
 * 若不提供 fn 参数, 则使用 fastcpy 内联函数
 */
INLINE void bench_copy_block_inline(benchmark::State& state, size_t min_len, size_t max_len, copy_fn fn) {
	assert(max_len > min_len && (min_len & (min_len - 1)) == 0 && (max_len & (max_len - 1)) == 0);
	uint64_t seed = 1;
	size_t mem_len = MEM_HEAP_SIZE + max_len + 4096 + 64;
	char *src = (char*)malloc(mem_len), *rsi;
	char *dst = (char*)malloc(mem_len), *rdi;
#ifdef __AVX__
	asm volatile("vzeroupper\n\t"::);
#endif
	seed = rand_fill(src, mem_len, seed);
	seed = rand_fill(dst, mem_len, seed);
	size_t size_sum = 0, bsize;
	size_t diff_len = (min_len != 0 ? max_len - min_len : max_len - 1);
	for(auto _ : state) {
		seed = diff_len;
		for(int j = 0; j < 32; ++j) {// 优化测试生成器
			/* 随机访存测试 */
			seed = xorshift64(seed);
			rsi = src + seed % MEM_HEAP_SIZE;
			rdi = dst + (seed >> 32) % MEM_HEAP_SIZE;
			seed = xorshift64(seed);
			for(int i = 0; i < 13; ++i) {
				bsize = min_len + (seed & diff_len);
				size_sum += bsize;
				asm volatile("rorx $5, %0, %0\n\t":"+r"(seed)::);
				fn(rdi, rsi, bsize);
			}
			/* 4K 伪混叠测试, 实测影响性能, 但影响有限 */
			size_sum += max_len + max_len;
			rsi = (char*)((size_t)src | 4095) + 1;
			rdi = (char*)((size_t)dst | 4095) + 1;
			fn(rdi, rsi + 64, max_len);// 後向复制会 4K 混叠
			fn(rdi + 64, rsi, max_len);// 前向复制会 4K 混叠
		}
	}
	state.SetBytesProcessed(size_sum);
	free(src);
	free(dst);
}

template <int FN = 0, typename ...ARGS>
__attribute__((optimize("align-functions=4096")))
void bench_copy_block(benchmark::State& state, size_t min_len, size_t max_len, ARGS&&... args) {
	if constexpr(FN == 0) {
		bench_copy_block_inline(state, min_len, max_len, std::forward<ARGS>(args)...);
	}
	else if constexpr(FN == MEMCPY_ERMS) {
		bench_copy_block_inline(state, min_len, max_len, memcpy_erms);
	}
	else if constexpr(FN == MEMCPY_TRIVIAL) {
		bench_copy_block_inline(state, min_len, max_len, memcpy_trivial);
	}
}

static constexpr uint32_t KB = 1024;
static constexpr uint32_t MB = 1024 * 1024;

#if 0

#undef BENCHMARK_MEMCPY
#define BENCHMARK_MEMCPY(bm_arg1, bm_arg2, memcpy) \
	BENCHMARK_CAPTURE(bench_copy_block, (memcpy/bm_arg1/bm_arg2), bm_arg1, bm_arg2, memcpy)

/* 测试小尺寸复制的性能 */

#undef MEMCPY_BEHCH
#define MEMCPY_BEHCH (FASTCPY | BRANCH_COPY | SWITCH_COPY | TINY_COPY | MEMCPY_GLIBC)

#undef BM_ARG1
#undef BM_ARG2
#define BM_ARG1 1
#define BM_ARG2 32
#include "./memcpy_bench.h"

#undef MEMCPY_BEHCH
#define MEMCPY_BEHCH (FASTCPY | TINY_COPY | MEMCPY_GLIBC | RTE_MEMCPY_AVX)

#undef BM_ARG1
#undef BM_ARG2
#define BM_ARG1 32
#define BM_ARG2 256
#include "./memcpy_bench.h"

/* 测试中尺寸复制的性能 */

#undef MEMCPY_BEHCH
#define MEMCPY_BEHCH (FASTCPY | MEMCPY_GLIBC | UNROLLED_COPY | MEMCPY_MY2 | RTE_MEMCPY_AVX)

#undef BM_ARG1
#undef BM_ARG2
#define BM_ARG1 256
#define BM_ARG2 1*KB
#include "./memcpy_bench.h"

#undef BM_ARG1
#undef BM_ARG2
#define BM_ARG1 256*KB
#define BM_ARG2 1*MB
#include "./memcpy_bench.h"

#undef BM_ARG1
#undef BM_ARG2
#define BM_ARG1 1*MB
#define BM_ARG2 4*MB
#include "./memcpy_bench.h"

/* 测试大尺寸复制的性能 */

#undef MEMCPY_BEHCH
#define MEMCPY_BEHCH (FASTCPY | MEMCPY_GLIBC | COPY_LARGE)

#undef BM_ARG1
#undef BM_ARG2
#define BM_ARG1 4*MB
#define BM_ARG2 16*MB
#include "./memcpy_bench.h"

#undef BM_ARG1
#undef BM_ARG2
#define BM_ARG1 64*MB
#define BM_ARG2 128*MB
#include "./memcpy_bench.h"

/* 三种尺寸测试结束 */
#endif

/* 测试各种 memcpy 实现的性能 */
#if 1
#undef BENCHMARK_MEMCPY
#define BENCHMARK_MEMCPY(bm_arg1, bm_arg2, memcpy) \
	BENCHMARK_CAPTURE(bench_text_cat, (memcpy/bm_arg1/bm_arg2), bm_arg1, bm_arg2, memcpy)
#undef BENCHMARK_INLINE_MEMCPY
#define BENCHMARK_INLINE_MEMCPY(id, bm_arg1, bm_arg2) \
	BENCHMARK_CAPTURE(bench_text_cat<id>, (_##id/bm_arg1/bm_arg2), bm_arg1, bm_arg2)

#undef BM_ARG1
#undef BM_ARG2
#define BM_ARG1 64
#define BM_ARG2 678*KB
#include "./memcpy_bench.h"

#undef BM_ARG1
#undef BM_ARG2
#define BM_ARG1 256
#define BM_ARG2 678*KB
#include "./memcpy_bench.h"

#undef BM_ARG1
#undef BM_ARG2
#define BM_ARG1 4096
#define BM_ARG2 16*MB
#include "./memcpy_bench.h"

#undef MEMCPY_BEHCH
#define MEMCPY_BEHCH 0xfffffff

#undef BENCHMARK_MEMCPY
#define BENCHMARK_MEMCPY(bm_arg1, bm_arg2, memcpy) \
	BENCHMARK_CAPTURE(bench_copy_block, (memcpy/bm_arg1/bm_arg2), bm_arg1, bm_arg2, memcpy)->ThreadRange(1, 4)
#undef BENCHMARK_INLINE_MEMCPY
#define BENCHMARK_INLINE_MEMCPY(id, bm_arg1, bm_arg2) \
	BENCHMARK_CAPTURE(bench_copy_block<id>, (_##id/bm_arg1/bm_arg2), bm_arg1, bm_arg2)->ThreadRange(1, 4)

#undef BM_ARG1
#undef BM_ARG2
#define BM_ARG1 1
#define BM_ARG2 32
#include "./memcpy_bench.h"

#undef BM_ARG1
#undef BM_ARG2
#define BM_ARG1 32
#define BM_ARG2 256
#include "./memcpy_bench.h"

#undef BM_ARG1
#undef BM_ARG2
#define BM_ARG1 256
#define BM_ARG2 1*KB
#include "./memcpy_bench.h"

#undef BM_ARG1
#undef BM_ARG2
#define BM_ARG1 1*KB
#define BM_ARG2 4*KB
#include "./memcpy_bench.h"

#undef BM_ARG1
#undef BM_ARG2
#define BM_ARG1 4*KB
#define BM_ARG2 16*KB
#include "./memcpy_bench.h"

#undef BM_ARG1
#undef BM_ARG2
#define BM_ARG1 16*KB
#define BM_ARG2 64*KB
#include "./memcpy_bench.h"

#undef BM_ARG1
#undef BM_ARG2
#define BM_ARG1 64*KB
#define BM_ARG2 256*KB
#include "./memcpy_bench.h"

#undef BM_ARG1
#undef BM_ARG2
#define BM_ARG1 256*KB
#define BM_ARG2 1*MB
#include "./memcpy_bench.h"

#undef BM_ARG1
#undef BM_ARG2
#define BM_ARG1 1*MB
#define BM_ARG2 4*MB
#include "./memcpy_bench.h"

#undef BM_ARG1
#undef BM_ARG2
#define BM_ARG1 4*MB
#define BM_ARG2 16*MB
#include "./memcpy_bench.h"

#undef BM_ARG1
#undef BM_ARG2
#define BM_ARG1 16*MB
#define BM_ARG2 128*MB
#include "./memcpy_bench.h"

#endif

BENCHMARK_MAIN();
