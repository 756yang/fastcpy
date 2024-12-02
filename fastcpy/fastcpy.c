#include "fastcpy.h"

//#include <immintrin.h>

/* 注意: 此文件仅能在 x86-64 平台上的 gcc 编译 */

/* 已检查无误, 小尺寸和中尺寸复制测试通过 2024/11/25 */
/* 已检查无误, 大尺寸复制测试通过 2024/11/29 */
/* 已测试无误, SSE,AVX,AVX512 版本皆可工作 2024/12/03 */
/* 已测试无误, fast_stpcpy,fast_wstpcpy,fast_dstpcpy,fast_qstpcpy 2024/12/06 */

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

/* 假设页面大小 4KB, 缓存行大小 64B */
#define PAGE_SIZE 4096
#define CACHELINE_SIZE 64

#ifndef FASTCPY_NO_NTA_PART
size_t fastcpy_non_temporal_threshold = 0xffffffffffffffffLL;
#endif

#if VEC_SIZE != 0/* fastcpy 开始 */

/* 实测使用 sub 代替 dec 无明显性能影响, 由于代码体积可能会略微下降 @r7-3700x
   根据优化手册, 某些架构 inc/dec 指令有两周期延迟, 例如 P4 和 Alder Lake-E
   当操作数为内存时或者 Alder Lake-E 上 inc/dec 吞吐量更低, 则建议使用 add/sub */
#define FASTCPY_NOUSE_INC_DEC 0

typedef float __vmm __attribute__((vector_size(VEC_SIZE)));

/* 为了性能可控, 只能以汇编写此函数 */

#if VEC_SIZE == 64
#ifdef FASTCPY_OWNER_SOURCE
__attribute__((used,optimize("align-functions=4096")))
#else
__attribute__((used,optimize("align-functions=32")))
#endif
static void fastcpy_32_to_64(void *dst, const void *src, size_t size) {
	register __vmm v0, v1;
	asm volatile(ASM_OPT(
		VEX_PRE(movups) (%[src]), %t[v1]\n\t
		VEX_PRE(movups) -32(%[src],%[size]), %t[v0]\n\t
		VEX_PRE(movups) %t[v1], (%[dst])\n\t
		VEX_PRE(movups) %t[v0], -32(%[dst],%[size])\n\t
		VZEROUPPER\n\t
		): [dst]"+r"(dst), [src]"+r"(src), [size]"+r"(size), [v0]"=x"(v0), [v1]"=x"(v1)
		: : "memory"
	);
}
#endif

/* 复制小内存, size <= VEC_SIZE */
#if defined(FASTCPY_OWNER_SOURCE) && VEC_SIZE < 64
__attribute__((naked,optimize("align-functions=4096")))
#else
__attribute__((naked,optimize(ASM_OPT(align-functions=CACHELINE_SIZE))))
#endif
void* fastcpy_tiny(void *dst, const void *src, size_t size) {
	register __vmm v0, v1;
	asm volatile(ASM_OPT(
		mov 	%[dst], %%rax\n\t
		cmp 	$(VEC_SIZE), %[size]\n\t
		ja  	.F_fastcpy_more_vec\n\t
		.p2align 4\n\t
.F_fastcpy_less_vec:\n\t
#if VEC_SIZE == 64
		cmpl	$32, %k[size]\n\t
		jae 	fastcpy_32_to_64\n\t
#endif
#if VEC_SIZE > 16
		cmpl	$16, %k[size]\n\t
		jb  	.L_fastcpy_less_16\n\t
		VEX_PRE(movups) (%[src]), %x[v1]\n\t
		VEX_PRE(movups) -16(%[src],%[size]), %x[v0]\n\t
		VEX_PRE(movups) %x[v1], (%[dst])\n\t
		VEX_PRE(movups) %x[v0], -16(%[dst],%[size])\n\t
		ret\n\t
		.p2align 2\n\t
#endif
.L_fastcpy_less_16:\n\t
		cmpl	$8, %k[size]\n\t
		jae 	.L_fastcpy_in_8_15\n\t
		sub 	$4, %[size]\n\t
		jae 	.L_fastcpy_in_4_7\n\t
		cmpl	$-3, %k[size]\n\t
		jl  	.L_fastcpy_in_0\n\t
		movzbl	(%[src]), %k[dst]\n\t
		je  	.L_fastcpy_in_1\n\t
		movzwl	2(%[src],%[size]), %k[src]\n\t
		movw	%w[src], 2(%%rax,%[size])\n\t
.L_fastcpy_in_1:\n\t
		movb	%b[dst], (%%rax)\n\t
.L_fastcpy_in_0:\n\t
		ret\n\t
		.p2align 2\n\t
.L_fastcpy_in_4_7:\n\t
		movl	(%[src]), %k[dst]\n\t
		movl	(%[src],%[size]), %k[src]\n\t
		movl	%k[dst], (%%rax)\n\t
		movl	%k[src], (%%rax,%[size])\n\t
		ret\n\t
		.p2align 2\n\t
.L_fastcpy_in_8_15:\n\t
		movq	(%[src]), %[dst]\n\t
		movq	-8(%[src],%[size]), %[src]\n\t
		movq	%[dst], (%%rax)\n\t
		movq	%[src], -8(%%rax,%[size])\n\t
		ret\n\t
		): [dst]"+r"(dst), [src]"+r"(src), [size]"+r"(size), [v0]"=x"(v0), [v1]"=x"(v1)
		: : "rax", "memory"
	);
}

__attribute__((naked,optimize(ASM_OPT(align-functions=CACHELINE_SIZE))))
void* fastcpy(void *dst, const void *src, size_t size) {// 已优化到极限了
	register __vmm v0 asm(VMM_NAME "0"), v1, v2, v3, v4, v5;
	/* 主逻辑部分 */
	asm volatile(ASM_OPT(
		mov 	%[dst], %%rax\n\t
		cmp 	$(VEC_SIZE), %[size]\n\t
		jb  	.F_fastcpy_less_vec\n\t
.F_fastcpy_more_vec:\n\t
		VEX_PRE(movups) (%[src]), %[v0]\n\t
		cmp 	$(8*VEC_SIZE), %[size]\n\t
		ja  	_F_fastcpy_more_8x_vec\n\t
		VEX_PRE(movups) -VEC_SIZE(%[src],%[size]), %[v1]\n\t
		VEX_PRE(movups) %[v0], (%[dst])\n\t
		VEX_PRE(movups) %[v1], -VEC_SIZE(%[dst],%[size])\n\t
		cmpl	$(2*VEC_SIZE), %k[size]\n\t
		jbe 	0f\n\t
		VEX_PRE(movups) VEC_SIZE(%[src]), %[v0]\n\t
		VEX_PRE(movups) -2*VEC_SIZE(%[src],%[size]), %[v1]\n\t
		VEX_PRE(movups) %[v0], VEC_SIZE(%[dst])\n\t
		VEX_PRE(movups) %[v1], -2*VEC_SIZE(%[dst],%[size])\n\t
		add 	$(-4*VEC_SIZE), %[size]\n\t
		jle 	0f\n\t
		VEX_PRE(movups) 2*VEC_SIZE(%[src]), %[v3]\n\t
		VEX_PRE(movups) 3*VEC_SIZE(%[src]), %[v2]\n\t
		VEX_PRE(movups) (%[src],%[size]), %[v1]\n\t
		VEX_PRE(movups) VEC_SIZE(%[src],%[size]), %[v0]\n\t
		VEX_PRE(movups) %[v3], 2*VEC_SIZE(%[dst])\n\t
		VEX_PRE(movups) %[v2], 3*VEC_SIZE(%[dst])\n\t
		VEX_PRE(movups) %[v1], (%[dst],%[size])\n\t
		VEX_PRE(movups) %[v0], VEC_SIZE(%[dst],%[size])\n\t
		0:\n\t
		VZEROUPPER\n\t
		ret\n\t
		/* 以上 jcc 指令字节数必须为 2 否则跨缓存行导致效率降低 */
		.p2align 4,,4\n\t
		1:\n\t
		call	get_non_temporal_threshold\n\t
_F_fastcpy_more_8x_vec:\n\t
		subq	%[src], %%rax\n\t
		cmp 	%[size], fastcpy_non_temporal_threshold(%%rip)\n\t
		/* 分支初始执行, 条件跳转预取指令地址更低的位置 , 间接跳转预取当前位置 */
		jb  	memcpy_large_memcpy\n\t
		jl  	1b\n\t
		testb	$((PAGE_SIZE-16*VEC_SIZE)>>8), %%ah\n\t
		je  	.F_fastcpy_more_8x_vec_backward\n\t
		): [dst]"+r"(dst), [src]"+r"(src), [size]"+r"(size), [v0]"=x"(v0), [v1]"=x"(v1), [v2]"=x"(v2), [v3]"=x"(v3)
		: : "rax", "memory"
	);
	/* `6 vmm`循环有更大吞吐量, 但首尾更难处理,`4 vmm`或`8 vmm`循环可解决此问题,
	   但`Win64 ABI`仅6个易失xmm, 若`Sysv ABI`建议以8向量循环且无需保存xmm */
	memcpy_more_8x_vec_forward: {
	size_t ldz;// uint8_t *dsa, *sra, *dsz;
	asm volatile(ASM_OPT(
		/* 循环准备 */
		.p2align 4,,11\n\t
.F_fastcpy_more_8x_vec_forward:\n\t
#if !defined(FASTCPY_UNROLLED_4VEC) && VEC_SIZE < 64
		/* VEX_PRE(movups) (%[sra]), %[v0]\n\t 已经读取了 v0 */
		VEX_PRE(movups) -VEC_SIZE(%[sra],%[dsz]), %[v1]\n\t
		lea 	-VEC_SIZE(%[dsa],%[dsz]), %[dsz]\n\t
		movq	%[dsa], %%rax\n\t
		VEX_PRE(movups) %[v0], (%[dsa])\n\t
		orq 	$(VEC_SIZE-1), %[dsa]\n\t
		lea 	(5*VEC_SIZE+1)(%[dsa]), %[dsa]\n\t
		VEX_PRE(movups) %[v1], (%[dsz])\n\t
		sub 	%%rax, %[sra]\n\t
		add 	%[dsa], %[sra]\n\t
		mov 	$(-6*VEC_SIZE), %[ldz]\n\t
		/* 循环复制 */
		.p2align 4,,12\n\t
		0:\n\t
		VEX_PRE(movups) -5*VEC_SIZE(%[sra]), %[v5]\n\t
		VEX_PRE(movups) -4*VEC_SIZE(%[sra]), %[v4]\n\t
		VEX_PRE(movups) -3*VEC_SIZE(%[sra]), %[v3]\n\t
		VEX_PRE(movups) -2*VEC_SIZE(%[sra]), %[v2]\n\t
		VEX_PRE(movups) -1*VEC_SIZE(%[sra]), %[v1]\n\t
		VEX_PRE(movups) 0*VEC_SIZE(%[sra]), %[v0]\n\t
		sub 	%[ldz], %[sra]\n\t
		VEX_PRE(movaps) %[v5], -5*VEC_SIZE(%[dsa])\n\t
		VEX_PRE(movaps) %[v4], -4*VEC_SIZE(%[dsa])\n\t
		VEX_PRE(movaps) %[v3], -3*VEC_SIZE(%[dsa])\n\t
		VEX_PRE(movaps) %[v2], -2*VEC_SIZE(%[dsa])\n\t
		VEX_PRE(movaps) %[v1], -1*VEC_SIZE(%[dsa])\n\t
		VEX_PRE(movaps) %[v0], 0*VEC_SIZE(%[dsa])\n\t
		sub 	%[ldz], %[dsa]\n\t
		cmp 	%[dsa], %[dsz]\n\t
		ja  	0b\n\t
		/* 循环收尾 */
		lea 	VEC_SIZE(%[ldz]), %[ldz]\n\t
		sub 	%[dsa], %[dsz]\n\t
		cmp 	%[dsz], %[ldz]\n\t
		jge 	2f\n\t
		.p2align 4,,8\n\t
		1:\n\t
		VEX_PRE(movups) (%[sra],%[ldz]), %[v0]\n\t
		VEX_PRE(movaps) %[v0], (%[dsa],%[ldz])\n\t
		add 	$(VEC_SIZE), %[ldz]\n\t
		cmp 	%[dsz], %[ldz]\n\t
		jl  	1b\n\t
		2:\n\t
#else
		/* VEX_PRE(movups) (%[sra]), %[v0]\n\t 已经读取了 v0 */
		movq	%[dsa], %%rax\n\t
		orq 	$(VEC_SIZE-1), %[dsa]\n\t
		VEX_PRE(movups) -VEC_SIZE(%[sra],%[dsz]), %[v4]\n\t
		add 	$(1+2*VEC_SIZE), %[dsa]\n\t
		VEX_PRE(movups) -2*VEC_SIZE(%[sra],%[dsz]), %[v5]\n\t
		sub 	%%rax, %[sra]\n\t
		add 	%[dsa], %[sra]\n\t
		VEX_PRE(movups) %[v0], (%%rax)\n\t
		lea 	-2*VEC_SIZE(%%rax,%[dsz]), %[dsz]\n\t
		.p2align 4,,13\n\t
		0:\n\t
		VEX_PRE(movups) -2*VEC_SIZE(%[sra]), %[v3]\n\t
		VEX_PRE(movups) -1*VEC_SIZE(%[sra]), %[v2]\n\t
		VEX_PRE(movups) 0*VEC_SIZE(%[sra]), %[v1]\n\t
		VEX_PRE(movups) 1*VEC_SIZE(%[sra]), %[v0]\n\t
		sub 	$(-4*VEC_SIZE), %[sra]\n\t
		VEX_PRE(movaps) %[v3], -2*VEC_SIZE(%[dsa])\n\t
		VEX_PRE(movaps) %[v2], -1*VEC_SIZE(%[dsa])\n\t
		VEX_PRE(movaps) %[v1], 0*VEC_SIZE(%[dsa])\n\t
		VEX_PRE(movaps) %[v0], 1*VEC_SIZE(%[dsa])\n\t
		sub 	$(-4*VEC_SIZE), %[dsa]\n\t
		cmp 	%[dsa], %[dsz]\n\t
		ja  	0b\n\t
		add 	$(-2*VEC_SIZE), %[dsa]\n\t
		VEX_PRE(movups) %[v4], 1*VEC_SIZE(%[dsz])\n\t
		VEX_PRE(movups) %[v5], 0*VEC_SIZE(%[dsz])\n\t
		cmp 	%[dsa], %[dsz]\n\t
		jbe 	1f\n\t
		VEX_PRE(movups) -2*VEC_SIZE(%[sra]), %[v1]\n\t
		VEX_PRE(movups) -1*VEC_SIZE(%[sra]), %[v0]\n\t
		VEX_PRE(movaps) %[v1], 0*VEC_SIZE(%[dsa])\n\t
		VEX_PRE(movaps) %[v0], 1*VEC_SIZE(%[dsa])\n\t
		1:\n\t
#endif
		VZEROUPPER\n\t
		ret\n\t
		): [dsa]"=r"(dst), [sra]"=r"(src), [dsz]"=r"(size), [ldz]"=&r"(ldz), [v0]"=x"(v0), [v1]"=x"(v1), [v2]"=x"(v2), [v3]"=x"(v3), [v4]"=x"(v4), [v5]"=x"(v5)
		:"0"(dst), "1"(src), "2"(size) : "rax", "memory"
	);}
	memcpy_more_8x_vec_backward: {
	uint8_t *dsz;// size_t ldz; uint8_t *sra;
#ifdef XMM_NEED_SAVE/* ms_abi */
	register uint8_t *dsa asm("rax");
#else/* sysv_abi */
	register uint8_t *dsa asm("rcx");
#endif
	asm volatile(ASM_OPT(
		/* 循环准备 */
		.p2align 4\n\t
.F_fastcpy_more_8x_vec_backward:\n\t
#if !defined(FASTCPY_UNROLLED_4VEC) && VEC_SIZE < 64
		/* VEX_PRE(movups) (%[src]), %[v0]\n\t 已经读取了 v0 */
		VEX_PRE(movups) -VEC_SIZE(%[src],%[size]), %[v1]\n\t
		lea 	(-1-VEC_SIZE)(%[dst],%[size]), %[dsa]\n\t
		lea 	5*VEC_SIZE(%[dst]), %[dsz]\n\t
		movl	$(6*VEC_SIZE), %k[ldz]\n\t
		VEX_PRE(movups) %[v0], (%[dst])\n\t
		VEX_PRE(movups) %[v1], 1(%[dsa])\n\t
		and 	$(-VEC_SIZE), %[dsa]\n\t
		sub 	%[dst], %[sra]\n\t
		add 	%[dsa], %[sra]\n\t
#ifdef XMM_NEED_SAVE
		xchg	%%rcx, %%rax\n\t
#else
		mov 	%[dst], %%rax\n\t
#endif
		/* 循环复制 */
		.p2align 4,,10\n\t
		0:\n\t
		VEX_PRE(movups) 0*VEC_SIZE(%[sra]), %[v5]\n\t
		VEX_PRE(movups) -1*VEC_SIZE(%[sra]), %[v4]\n\t
		VEX_PRE(movups) -2*VEC_SIZE(%[sra]), %[v3]\n\t
		VEX_PRE(movups) -3*VEC_SIZE(%[sra]), %[v2]\n\t
		VEX_PRE(movups) -4*VEC_SIZE(%[sra]), %[v1]\n\t
		VEX_PRE(movups) -5*VEC_SIZE(%[sra]), %[v0]\n\t
		sub 	%[ldz], %[sra]\n\t
		VEX_PRE(movaps) %[v5], 0*VEC_SIZE(%%rcx)\n\t
		VEX_PRE(movaps) %[v4], -1*VEC_SIZE(%%rcx)\n\t
		VEX_PRE(movaps) %[v3], -2*VEC_SIZE(%%rcx)\n\t
		VEX_PRE(movaps) %[v2], -3*VEC_SIZE(%%rcx)\n\t
		VEX_PRE(movaps) %[v1], -4*VEC_SIZE(%%rcx)\n\t
		VEX_PRE(movaps) %[v0], -5*VEC_SIZE(%%rcx)\n\t
		sub 	%[ldz], %%rcx\n\t
		cmp 	%%rcx, %[dsz]\n\t
		jb  	0b\n\t
		/* 循环收尾 */
		cmp 	%%rcx, %%rax\n\t
		jnb 	2f\n\t
		.p2align 4,,7\n\t
		1:\n\t
		VEX_PRE(movups) (%[sra]), %[v0]\n\t
		VEX_PRE(movaps) %[v0], (%%rcx)\n\t
		sub 	$(VEC_SIZE), %%rcx\n\t
		sub 	$(VEC_SIZE), %[sra]\n\t
		cmp 	%%rcx, %%rax\n\t
		jb  	1b\n\t
		2:\n\t
#else
		/* VEX_PRE(movups) (%[src]), %[v0]\n\t 已经读取了 v0 */
		lea 	-VEC_SIZE(%[dst],%[size]), %[dsz]\n\t
		VEX_PRE(movups) -VEC_SIZE(%[src],%[size]), %[v1]\n\t
		lea 	(-1-VEC_SIZE)(%[dsz]), %[dsa]\n\t
		and 	$(-VEC_SIZE), %[dsa]\n\t
		VEX_PRE(movups) %[v1], (%[dsz])\n\t
		lea 	2*VEC_SIZE(%[dst]), %[ldz]\n\t
		VEX_PRE(movups) VEC_SIZE(%[src]), %[v5]\n\t
		sub 	%[dst], %[sra]\n\t
		add 	%[dsa], %[sra]\n\t
#ifdef XMM_NEED_SAVE
		xchg	%%rcx, %%rax\n\t
#else
		mov 	%[dst], %%rax\n\t
#endif
		.p2align 4,,12\n\t
		0:\n\t
		VEX_PRE(movups) 1*VEC_SIZE(%[sra]), %[v4]\n\t
		VEX_PRE(movups) 0*VEC_SIZE(%[sra]), %[v3]\n\t
		VEX_PRE(movups) -1*VEC_SIZE(%[sra]), %[v2]\n\t
		VEX_PRE(movups) -2*VEC_SIZE(%[sra]), %[v1]\n\t
		add 	$(-4*VEC_SIZE), %[sra]\n\t
		VEX_PRE(movaps) %[v4], 1*VEC_SIZE(%%rcx)\n\t
		VEX_PRE(movaps) %[v3], 0*VEC_SIZE(%%rcx)\n\t
		VEX_PRE(movaps) %[v2], -1*VEC_SIZE(%%rcx)\n\t
		VEX_PRE(movaps) %[v1], -2*VEC_SIZE(%%rcx)\n\t
		add 	$(-4*VEC_SIZE), %%rcx\n\t
		cmp 	%%rcx, %[ldz]\n\t
		jb  	0b\n\t
		sub 	$(-2*VEC_SIZE), %%rcx\n\t
		VEX_PRE(movups) %[v5], 1*VEC_SIZE(%%rax)\n\t
		VEX_PRE(movups) %[v0], 0*VEC_SIZE(%%rax)\n\t
		cmp 	%%rcx, %[ldz]\n\t
		jae 	1f\n\t
		VEX_PRE(movups) 1*VEC_SIZE(%[sra]), %[v2]\n\t
		VEX_PRE(movups) 0*VEC_SIZE(%[sra]), %[v1]\n\t
		VEX_PRE(movaps) %[v2], -1*VEC_SIZE(%%rcx)\n\t
		VEX_PRE(movaps) %[v1], -2*VEC_SIZE(%%rcx)\n\t
		1:\n\t
#endif
		VZEROUPPER\n\t
		ret\n\t
		): [dsa]"=r"(dsa), [sra]"=r"(src), [ldz]"=r"(size), [dsz]"=&r"(dsz), [v0]"=x"(v0), [v1]"=x"(v1), [v2]"=x"(v2), [v3]"=x"(v3), [v4]"=x"(v4), [v5]"=x"(v5)
		: [dst]"r"(dst), [src]"1"(src), [size]"2"(size) :
#ifndef XMM_NEED_SAVE
		"rax",
#endif
		"memory"
	);}
}


/* large_memcpy 内部循环的每次字节数  */
#if VEC_SIZE == 64
# define LARGE_LOAD_SIZE (VEC_SIZE * 2)
#else
# define LARGE_LOAD_SIZE (VEC_SIZE * 4)
#endif

#if !defined(FASTCPY_USE_PREFETCH) || FASTCPY_USE_PREFETCH
# define PREFETCH(addr) prefetcht0 addr
#else
# define PREFETCH(addr)
#endif

/* 假设预取大小为缓存行大小 */
#ifndef PREFETCH_SIZE
# define PREFETCH_SIZE CACHELINE_SIZE
#endif

#define PREFETCHED_LOAD_SIZE LARGE_LOAD_SIZE


#if PREFETCH_SIZE == 64 || PREFETCH_SIZE == 128
# if PREFETCHED_LOAD_SIZE == PREFETCH_SIZE
#  define PREFETCH_ONE_SET(dir, base, offset) \
	PREFETCH((offset)base);
# elif PREFETCHED_LOAD_SIZE == 2 * PREFETCH_SIZE
#  define PREFETCH_ONE_SET(dir, base, offset) \
	PREFETCH((offset)base); \
	PREFETCH((offset + dir * PREFETCH_SIZE)base);
# elif PREFETCHED_LOAD_SIZE == 4 * PREFETCH_SIZE
#  define PREFETCH_ONE_SET(dir, base, offset) \
	PREFETCH((offset)base); \
	PREFETCH((offset + dir * PREFETCH_SIZE)base); \
	PREFETCH((offset + dir * PREFETCH_SIZE * 2)base); \
	PREFETCH((offset + dir * PREFETCH_SIZE * 3)base);
# else
#   error Unsupported PREFETCHED_LOAD_SIZE!
# endif
#else
# error Unsupported PREFETCH_SIZE!
#endif


#if LARGE_LOAD_SIZE == (VEC_SIZE * 2)
# define LOAD_ONE_SET(base, offset, vec0, vec1, ...) \
	VEX_PRE(movups) (offset)base, vec0; \
	VEX_PRE(movups) ((offset) + VEC_SIZE)base, vec1;
# define STORE_ONE_SET(base, offset, vec0, vec1, ...) \
	VEX_PRE(movntps) vec0, (offset)base; \
	VEX_PRE(movntps) vec1, ((offset) + VEC_SIZE)base;
#elif LARGE_LOAD_SIZE == (VEC_SIZE * 4)
# define LOAD_ONE_SET(base, offset, vec0, vec1, vec2, vec3) \
	VEX_PRE(movups) (offset)base, vec0; \
	VEX_PRE(movups) ((offset) + VEC_SIZE)base, vec1; \
	VEX_PRE(movups) ((offset) + VEC_SIZE * 2)base, vec2; \
	VEX_PRE(movups) ((offset) + VEC_SIZE * 3)base, vec3;
# define STORE_ONE_SET(base, offset, vec0, vec1, vec2, vec3) \
	VEX_PRE(movntps) vec0, (offset)base; \
	VEX_PRE(movntps) vec1, ((offset) + VEC_SIZE)base; \
	VEX_PRE(movntps) vec2, ((offset) + VEC_SIZE * 2)base; \
	VEX_PRE(movntps) vec3, ((offset) + VEC_SIZE * 3)base;
#else
# error Invalid LARGE_LOAD_SIZE
#endif


__attribute__((used,naked,optimize("align-functions=32")))
X64_CALL static uint8_t* memcpy_large_memcpy(uint8_t *dst_, const uint8_t *src_, size_t size_/*, register __vmm v0 */) {// 已优化到极限了
	register __vmm v0 asm(VMM_NAME "0");// 传入的寄存器参数
	register __vmm v1 asm(VMM_NAME "1");
	register __vmm v2 asm(VMM_NAME "2");
	register __vmm v3 asm(VMM_NAME "3");
	register __vmm v4 asm(VMM_NAME "4");
	register __vmm v5 asm(VMM_NAME "5");
	register __vmm v6 asm(VMM_NAME "6");
	register __vmm v7 asm(VMM_NAME "7");
	register __vmm v8 asm(VMM_NAME "8");
	register __vmm v9 asm(VMM_NAME "9");
	register __vmm v10 asm(VMM_NAME "10");
	register __vmm v11 asm(VMM_NAME "11");
	register __vmm v12 asm(VMM_NAME "12");
	register __vmm v13 asm(VMM_NAME "13");
	register __vmm v14 asm(VMM_NAME "14");
	register __vmm v15 asm(VMM_NAME "15");
#ifdef XMM_NEED_SAVE/* ms_abi */
	register void *dst asm("rcx") = dst_;
	register const void *src asm("rdx") = src_;
	register size_t size asm("r8") = size_;
	register size_t cxo asm("rdi");// 外层计数
	register size_t cxi asm("rsi");// 内层计数
	register size_t cxz asm("r8");// 尾部计数
#else/* sysv_abi */
	register void *dst asm("rdi") = dst_;
	register const void *src asm("rsi") = src_;
	register size_t size asm("rdx") = size_;
	register size_t cxo asm("rcx");
	register size_t cxi asm("rdx");
	register size_t cxz asm("r8");
#endif
	asm volatile(ASM_OPT(
		xor 	%%r9, %%r9\n\t
#ifdef XMM_NEED_SAVE
		sub 	$0xB0, %%rsp\n\t
		mov 	%[cxo], (%%rsp)\n\t
		mov 	%[cxi], 8(%%rsp)\n\t
		VEX_PRE(movups) %%xmm6, 0x10(%%rsp)\n\t
		VEX_PRE(movups) %%xmm7, 0x20(%%rsp)\n\t
#else
		mov 	%[size], %%r8\n\t
#endif
		/* rax 仍保留 dst - src 的值 */
		dec 	%%rax\n\t
		lea 	(%[src],%%r8), %[cxi]\n\t
#if VEC_SIZE < 64
		VEX_PRE(movups) VEC_SIZE(%[src]), %[v1]\n\t
#endif
#if VEC_SIZE < 32
		VEX_PRE(movups) 2*VEC_SIZE(%[src]), %[v2]\n\t
		VEX_PRE(movups) 3*VEC_SIZE(%[src]), %[v3]\n\t
#endif
		testb	$((PAGE_SIZE-16*VEC_SIZE)>>8), %%ah\n\t
		cmovne	fastcpy_non_temporal_threshold(%%rip), %%r9\n\t
		mov 	%[dst], %[cxo]\n\t
		mov 	%[dst], %%rax\n\t
		VEX_PRE(movups) -VEC_SIZE(%[cxi]), %[v4]\n\t
		VEX_PRE(movups) -2*VEC_SIZE(%[cxi]), %[v5]\n\t
		andl	$63, %k[cxo]\n\t
		sub 	$64, %[cxo]\n\t
		VEX_PRE(movups) -3*VEC_SIZE(%[cxi]), %[v6]\n\t
		VEX_PRE(movups) -4*VEC_SIZE(%[cxi]), %[v7]\n\t
		lea 	(%[dst],%%r8), %[cxi]\n\t
		lea 	-1(%%r8,%[cxo]), %[cxz]\n\t
		VEX_PRE(movups) %[v0], (%[dst])\n\t
#if VEC_SIZE < 64
		VEX_PRE(movups) %[v1], VEC_SIZE(%[dst])\n\t
#endif
#if VEC_SIZE < 32
		VEX_PRE(movups) %[v2], 2*VEC_SIZE(%[dst])\n\t
		VEX_PRE(movups) %[v3], 3*VEC_SIZE(%[dst])\n\t
#endif
		sub 	%[cxo], %[src]\n\t
		sub 	%[cxo], %[dst]\n\t
		VEX_PRE(movups) %[v4], -VEC_SIZE(%[cxi])\n\t
		VEX_PRE(movups) %[v5], -2*VEC_SIZE(%[cxi])\n\t
		shl 	$4, %%r9\n\t
		mov 	%[cxz], %[cxo]\n\t
		VEX_PRE(movups) %[v6], -3*VEC_SIZE(%[cxi])\n\t
		VEX_PRE(movups) %[v7], -4*VEC_SIZE(%[cxi])\n\t
		shr 	$(%c[lgvs]+2), %k[cxz]\n\t
		movl	$(PAGE_SIZE/LARGE_LOAD_SIZE), %k[cxi]\n\t
		cmp 	%%r9, %[cxo]\n\t
		jae 	.L_fastcpy_large_memcpy_4x\n\t
.L_fastcpy_large_memcpy_2x:\n\t
		shr 	$(%c[lgpg]+1), %[cxo]\n\t
		/* Copy 4x VEC at a time from 2 pages.  */
		.p2align 4\n\t
		0:\n\t
		LOAD_ONE_SET((%[src]), 0, %[v0], %[v1], %[v2], %[v3])\n\t
		LOAD_ONE_SET((%[src]), PAGE_SIZE, %[v4], %[v5], %[v6], %[v7])\n\t
		/* 为下次循环而预取, 应置于加载之後 */
		sub 	$(-LARGE_LOAD_SIZE), %[src]\n\t
		PREFETCH_ONE_SET(1, (%[src]), 0)\n\t
		PREFETCH_ONE_SET(1, (%[src]), PAGE_SIZE)\n\t
		STORE_ONE_SET((%[dst]), 0, %[v0], %[v1], %[v2], %[v3])\n\t
		STORE_ONE_SET((%[dst]), PAGE_SIZE, %[v4], %[v5], %[v6], %[v7])\n\t
		sub 	$(-LARGE_LOAD_SIZE), %[dst]\n\t
#if FASTCPY_NOUSE_INC_DEC
		subl	$1, %k[cxi]\n\t
#else
		decl	%k[cxi]\n\t
#endif
		jne 	0b\n\t
		add 	$(PAGE_SIZE), %[src]\n\t
		lea 	PAGE_SIZE(%[dst]), %[dst]\n\t
		movl	$(PAGE_SIZE/LARGE_LOAD_SIZE), %k[cxi]\n\t
#if FASTCPY_NOUSE_INC_DEC
		sub 	$1, %[cxo]\n\t
#else
		dec 	%[cxo]\n\t
#endif
		jne 	0b\n\t
		/* 尾部处理 */
		andl	$((2*PAGE_SIZE-1)>>(%c[lgvs]+2)), %k[cxz]\n\t
.L_fastcpy_large_memcpy_tail:\n\t
#ifdef XMM_NEED_SAVE
		VEX_PRE(movups) 0x20(%%rsp),%%xmm7\n\t
		VEX_PRE(movups) 0x10(%%rsp),%%xmm6\n\t
		mov 	8(%%rsp), %[cxi]\n\t
		mov 	(%%rsp), %[cxo]\n\t
#endif
		je  	1f\n\t
		.p2align 4,,10\n\t
		0:\n\t
		VEX_PRE(movups) (%[src]), %[v0]\n\t
		VEX_PRE(movups) VEC_SIZE(%[src]), %[v1]\n\t
		VEX_PRE(movups) 2*VEC_SIZE(%[src]), %[v2]\n\t
		VEX_PRE(movups) 3*VEC_SIZE(%[src]), %[v3]\n\t
		sub 	$(-4*VEC_SIZE), %[src]\n\t
		PREFETCH_ONE_SET(1, (%[src]), 0)\n\t
		VEX_PRE(movntps) %[v0], (%[dst])\n\t
		VEX_PRE(movntps) %[v1], VEC_SIZE(%[dst])\n\t
		VEX_PRE(movntps) %[v2], 2*VEC_SIZE(%[dst])\n\t
		VEX_PRE(movntps) %[v3], 3*VEC_SIZE(%[dst])\n\t
		sub 	$(-4*VEC_SIZE), %[dst]\n\t
#if FASTCPY_NOUSE_INC_DEC
		subl	$1, %k[cxz]\n\t
#else
		decl	%k[cxz]\n\t
#endif
		jne 	0b\n\t
		1:\n\t
		sfence\n\t
#ifdef XMM_NEED_SAVE
		add 	$0xB0, %%rsp\n\t
#endif
		VZEROUPPER\n\t
		ret\n\t
#if defined(XMM_NEED_SAVE) && VEC_SIZE != 64
		.p2align 4\n\t
.L_fastcpy_large_memcpy_4x:\n\t
		VEX_PRE(movups) %%xmm8,0x30(%%rsp)\n\t
		VEX_PRE(movups) %%xmm9,0x40(%%rsp)\n\t
		VEX_PRE(movups) %%xmm10,0x50(%%rsp)\n\t
		VEX_PRE(movups) %%xmm11,0x60(%%rsp)\n\t
		VEX_PRE(movups) %%xmm12,0x70(%%rsp)\n\t
		VEX_PRE(movups) %%xmm13,0x80(%%rsp)\n\t
		VEX_PRE(movups) %%xmm14,0x90(%%rsp)\n\t
		VEX_PRE(movups) %%xmm15,0xA0(%%rsp)\n\t
#else
		.p2align 3,,3\n\t
		.p2align 2\n\t
.L_fastcpy_large_memcpy_4x:\n\t
#endif
		shr 	$(%c[lgpg]+2), %[cxo]\n\t
		/* Copy 4x VEC at a time from 4 pages.  */
		.p2align 4\n\t
		0:\n\t
		LOAD_ONE_SET((%[src]), 0, %[v0], %[v1], %[v8], %[v9])\n\t
		LOAD_ONE_SET((%[src]), PAGE_SIZE, %[v2], %[v3], %[v10], %[v11])\n\t
		LOAD_ONE_SET((%[src]), 2*PAGE_SIZE, %[v4], %[v5], %[v12], %[v13])\n\t
		LOAD_ONE_SET((%[src]), 3*PAGE_SIZE, %[v6], %[v7], %[v14], %[v15])\n\t
		sub 	$(-LARGE_LOAD_SIZE), %[src]\n\t
		/* 为下次循环而预取, 应置于加载之後 */
		PREFETCH_ONE_SET(1, (%[src]), 0)\n\t
		PREFETCH_ONE_SET(1, (%[src]), PAGE_SIZE)\n\t
		PREFETCH_ONE_SET(1, (%[src]), 2*PAGE_SIZE)\n\t
		PREFETCH_ONE_SET(1, (%[src]), 3*PAGE_SIZE)\n\t
		STORE_ONE_SET((%[dst]), 0, %[v0], %[v1], %[v8], %[v9])\n\t
		STORE_ONE_SET((%[dst]), PAGE_SIZE, %[v2], %[v3], %[v10], %[v11])\n\t
		STORE_ONE_SET((%[dst]), 2*PAGE_SIZE, %[v4], %[v5], %[v12], %[v13])\n\t
		STORE_ONE_SET((%[dst]), 3*PAGE_SIZE, %[v6], %[v7], %[v14], %[v15])\n\t
		sub 	$(-LARGE_LOAD_SIZE), %[dst]\n\t
#if FASTCPY_NOUSE_INC_DEC
		subl	$1, %k[cxi]\n\t
#else
		decl	%k[cxi]\n\t
#endif
		jne 	0b\n\t
		add 	$(3*PAGE_SIZE), %[src]\n\t
		lea 	3*PAGE_SIZE(%[dst]), %[dst]\n\t
		movl	$(PAGE_SIZE/LARGE_LOAD_SIZE), %k[cxi]\n\t
#if FASTCPY_NOUSE_INC_DEC
		sub 	$1, %[cxo]\n\t
#else
		dec 	%[cxo]\n\t
#endif
		jne 	0b\n\t
		andl	$((4*PAGE_SIZE-1)>>(%c[lgvs]+2)), %k[cxz]\n\t
#if defined(XMM_NEED_SAVE) && VEC_SIZE != 64
		VEX_PRE(movups) 0xA0(%%rsp),%%xmm15\n\t
		VEX_PRE(movups) 0x90(%%rsp),%%xmm14\n\t
		VEX_PRE(movups) 0x80(%%rsp),%%xmm13\n\t
		VEX_PRE(movups) 0x70(%%rsp),%%xmm12\n\t
		VEX_PRE(movups) 0x60(%%rsp),%%xmm11\n\t
		VEX_PRE(movups) 0x50(%%rsp),%%xmm10\n\t
		VEX_PRE(movups) 0x40(%%rsp),%%xmm9\n\t
		VEX_PRE(movups) 0x30(%%rsp),%%xmm8\n\t
#endif
		jmp 	.L_fastcpy_large_memcpy_tail\n\t
		): [cxo]"=r"(cxo), [cxi]"=r"(cxi), [cxz]"=r"(cxz), [v0]"=x"(v0), [v1]"=x"(v1), [v2]"=x"(v2), [v3]"=x"(v3), [v4]"=x"(v4), [v5]"=x"(v5), [v6]"=x"(v6), [v7]"=x"(v7), [v8]"=x"(v8), [v9]"=x"(v9), [v10]"=x"(v10), [v11]"=x"(v11), [v12]"=x"(v12), [v13]"=x"(v13), [v14]"=x"(v14), [v15]"=x"(v15)
		: [dst]"r"(dst), [src]"r"(src), [size]"r"(size), [lgpg]"i"((int)__builtin_log2(PAGE_SIZE)), [lgvs]"i"((int)__builtin_log2(VEC_SIZE)) : "r9", "rax", "memory"
	);
}

#ifndef FASTCPY_NO_NTA_PART/* FASTCPY_NO_NTA_PART 开始 */

#ifndef __cpuid
#define __cpuid(level, a, b, c, d)					\
  __asm__ __volatile__ ("cpuid\n\t"					\
			: "=a" (a), "=b" (b), "=c" (c), "=d" (d)	\
			: "0" (level))
#endif
#ifndef __cpuid_count
#define __cpuid_count(level, count, a, b, c, d)				\
  __asm__ __volatile__ ("cpuid\n\t"					\
			: "=a" (a), "=b" (b), "=c" (c), "=d" (d)	\
			: "0" (level), "2" (count))
#endif

#define CPU_VENDOR_TO_INT32(a,b,c,d, e,f,g,h, i,j,k,l) \
	(((a^e^i)&0xff)+(((b^f^j)&0xff)<<8)+(((c^g^k)&0xff)<<16)+(((d^h^l)&0xff)<<24))

/* warning: '_F_fastcpy_more_8x_vec' used but never defined 此警告无法抑制请忽略之 */
//static void* _F_fastcpy_more_8x_vec(void *dst, const void *src, size_t size)/*  asm("_F_fastcpy_more_8x_vec") 可用以修改汇编代码中的名称 */;

/**
 * 通过 cpuid 指令查询 L3 cache 与 logical CPUs 设置 fastcpy_non_temporal_threshold
 *  cpuid 参考: https://www.sandpile.org/x86/cpuid.htm
 *  glibc 参考: https://sourceware.org/git/?p=glibc.git;a=blob;f=sysdeps/x86/cacheinfo.c;hb=d3c57027470b78dba79c6d931e4e409b1fecfc80
 *  libcpuid 参考: https://libcpuid.sourceforge.net/documentation.html
 */
#ifndef DLLFASTCPY_LIBRARY
static
#endif
__attribute__((used,optimize("align-functions=32")))
void* get_non_temporal_threshold(void *dst, const void *src, size_t size) {
	// fastcpy_non_temporal_threshold = l3cache / sqrt(threads + 2*cores)
	const uint32_t intel_vendor = CPU_VENDOR_TO_INT32('G','e','n','u','i','n','e','I','n','t','e','l');
	const uint32_t amd_vendor = CPU_VENDOR_TO_INT32('A','u','t','h','e','n','t','i','c','A','M','D');
	uint32_t l3cache, threads;
	register uint32_t eax, ebx, ecx, edx;
	__cpuid(0, eax, ebx, ecx, edx);
	uint32_t vendor_id = ebx ^ edx ^ ecx;
	do {
		if(xsun_likely(vendor_id == intel_vendor)) {// intel cpu
			if(xsun_unlikely(eax < 4)) break;// leaf 2 仅旧 CPU 暂不支持
			if(xsun_likely(eax >= 0xb)) {// leaf 11
				for(int i = 0; ; ++i) {// 获取处理器拓扑结构
					__cpuid_count(0xb, i, eax, ebx, ecx, edx);
					int32_t type = ecx & 0xff00;
					if(type == 0) {
						threads = 0;
						break;
					}
					if(type == 0x200) {// 逻辑核心级别
						threads = ebx & 0xff;// 逻辑核心数
						break;
					}
				}
			}
			else {
				__cpuid(1, eax, ebx, ecx, edx);
				threads = (ebx << 8 >> 24);// 适用于旧 CPU, 为逻辑CPU数量
			}
			for(int i = 0; ; ++i) {// leaf 4
				__cpuid_count(4, i, eax, ebx, ecx, edx);
				if((eax & 0x1F) == 0) break;
				if((eax & 0xe0) == 0x60) {// 只计算 L3 缓存
					int32_t ways = (ebx >> 22) + 1;
					int32_t parts = (ebx << 10 >> 22) + 1;
					int32_t sets = ecx + 1;
					int32_t bytes = (ebx & 0xfff) + 1;
					l3cache = (bytes * parts) * (sets * ways);
					if(threads <= 1) threads = 1;
					else threads += threads;
threshold_calc:
					if(l3cache == 0) break;
					asm volatile(ASM_OPT(
						VEX_PRE(cvtsi2ss) %[td], VEX_ARG(%%xmm1,) %%xmm1\n\t
						VEX_PRE(cvtsi2ss) %[l3], VEX_ARG(%%xmm2,) %%xmm2\n\t
						VEX_PRE(rsqrtss) %%xmm1, VEX_ARG(%%xmm1,) %%xmm1\n\t
						VEX_PRE(mulss) %%xmm2, VEX_ARG(%%xmm1,) %%xmm1\n\t
						VEX_PRE(cvtss2si) %%xmm1, %k[th]\n\t
						):[th]"=r"(threads): [l3]"r"(l3cache), [td]"r"(threads)
						: "xmm1","xmm2"
					);/* 注意不能改变 xmm0 的内容 */
					goto threshold_done;
				}
			}
		}
		else if(vendor_id == amd_vendor) {// amd cpu
			__cpuid(0x80000000, eax, ebx, ecx, edx);
			uint32_t max_id = eax;
			if(xsun_unlikely(eax < 0x80000006)) break;// 不支持置默认值
			__cpuid(1, eax, ebx, ecx, edx);
			threads = (ebx << 8 >> 24);// 逻辑核心数
			if((edx & 0x10000000) && threads != 0) {// 支持 HTT
				uint32_t smt_count = 0;
				/* ext_family >= 17h 在 AMD Zen 之後, 需获取 SMT 数量 */
				if((eax & 0xf00) == 0xf00 && (eax & 0xff00000) >= 0x800000 && max_id >= 0x8000001e) {
					__cpuid(0x8000001e, eax, ebx, ecx, edx);
					smt_count = (ebx << 16 >> 24) + 1;// SMT 数量
				}
				__cpuid(0x80000008, eax, ebx, ecx, edx);
				uint32_t cores = (ecx & 0xff) + 1;// 内核数量
				if(smt_count != 0) cores /= smt_count;
				threads += 2*cores;
			}
			else threads = 1;
			__cpuid(0x80000006, eax, ebx, ecx, edx);// leaf 8000_0006h
			if(((edx >> 12) & 0xf) != 0) {
				l3cache = edx >> 18 << 19;
				goto threshold_calc;
			}
		}
	} while(0);
	threads = 0x200000;
threshold_done:
	fastcpy_non_temporal_threshold = threads;
#ifdef XMM_NEED_SAVE/* ms_abi */
	asm volatile(ASM_OPT(
		mov %0, %%rcx\n\t
		mov %1, %%rdx\n\t
		mov %2, %%r8\n\t
	):: "r"(dst), "r"(src), "r"(size): "rcx", "rdx", "r8");
#else/* sysv_abi */
	asm volatile(ASM_OPT(
		mov %0, %%rdi\n\t
		mov %1, %%rsi\n\t
		mov %2, %%rdx\n\t
	):: "r"(dst), "r"(src), "r"(size): "rdi", "rsi", "rdx");
#endif
	return dst;
}
#endif/* FASTCPY_NO_NTA_PART 结束 */

#else/* NO SSE version */
/* 参考 Linux kernel 的memcpy实现 */
__attribute__((naked,optimize(ASM_OPT(align-functions=CACHELINE_SIZE))))
void* fastcpy(void *dst, const void *src, size_t size) {
	asm volatile(ASM_OPT(
		push	%[dst]\n\t
		//movq	%[dst], %rax
		cmpq	$0x20, %[size]\n\t
		jb  	3f\n\t
		/* 检查是否發生内存假依赖(4K伪混叠)并跳转到适合的复制方法 */
		cmpb	%b[dst], %b[src]\n\t
		jl  	1f\n\t
		subq	$0x20, %[size]\n\t
		.p2align 4,,10\n\t
		0:\n\t
		subq	$0x20, %[size]\n\t
		/* 一次复制 4x8 字节的块 */
		movq	0*8(%[src]), %%rax\n\t
		movq	1*8(%[src]), %%r9\n\t
		movq	2*8(%[src]), %%r10\n\t
		movq	3*8(%[src]), %%r11\n\t
		leaq	4*8(%[src]), %[src]\n\t
		movq	%%rax, 0*8(%[dst])\n\t
		movq	%%r9, 1*8(%[dst])\n\t
		movq	%%r10, 2*8(%[dst])\n\t
		movq	%%r11, 3*8(%[dst])\n\t
		leaq	4*8(%[dst]), %[dst]\n\t
		jae 	0b\n\t
		addl	$0x20, %k[size]\n\t
		jmp 	3f\n\t
		1:\n\t
		/* 计算尾部的复制位置 */
		addq	%[size], %[src]\n\t
		addq	%[size], %[dst]\n\t
		subq	$0x20, %[size]\n\t
		/* 一个周期至少处理三个ALU操作因此填充NOPS对齐16字节块 */
		.p2align 4,,10\n\t
		2:\n\t
		subq	$0x20, %[size]\n\t
		movq	-1*8(%[src]), %%rax\n\t
		movq	-2*8(%[src]), %%r9\n\t
		movq	-3*8(%[src]), %%r10\n\t
		movq	-4*8(%[src]), %%r11\n\t
		leaq	-4*8(%[src]), %[src]\n\t
		movq	%%rax, -1*8(%[dst])\n\t
		movq	%%r9, -2*8(%[dst])\n\t
		movq	%%r10, -3*8(%[dst])\n\t
		movq	%%r11, -4*8(%[dst])\n\t
		leaq	-4*8(%[dst]), %[dst]\n\t
		jae 	2b\n\t
		/* 计算复制位置的首部 */
		addl	$0x20, %k[size]\n\t
		subq	%[size], %[src]\n\t
		subq	%[size], %[dst]\n\t
		3:\n\t
		cmpl	$16, %k[size]\n\t
		jb  	4f\n\t
		/* 复制16到31字节的数据 */
		movq	0*8(%[src]), %%rax\n\t
		movq	1*8(%[src]), %%r9\n\t
		movq	-2*8(%[src], %[size]), %%r10\n\t
		movq	-1*8(%[src], %[size]), %%r11\n\t
		movq	%%rax, 0*8(%[dst])\n\t
		movq	%%r9, 1*8(%[dst])\n\t
		movq	%%r10, -2*8(%[dst], %[size])\n\t
		movq	%%r11, -1*8(%[dst], %[size])\n\t
		popq	%%rax\n\t
		retq\n\t
		.p2align 4,,10\n\t
		4:\n\t
		cmpl	$8, %k[size]\n\t
		jae 	8f\n\t
		sub 	$4, %[size]\n\t
		jae 	7f\n\t
		cmpl	$-3, %k[size]\n\t
		jl  	6f\n\t
		movzbl	(%[src]), %%eax\n\t
		je  	5f\n\t
		movzwl	2(%[src],%[size]), %k[src]\n\t
		movw	%w[src], 2(%[dst],%[size])\n\t
		5:\n\t
		movb	%%al, (%[dst])\n\t
		6:\n\t
		popq	%%rax\n\t
		ret\n\t
		.p2align 4\n\t
		7:\n\t
		movl	(%[src]), %%eax\n\t
		movl	(%[src],%[size]), %k[src]\n\t
		movl	%%eax, (%[dst])\n\t
		movl	%k[src], (%[dst],%[size])\n\t
		popq	%%rax\n\t
		ret\n\t
		.p2align 4\n\t
		8:\n\t
		movq	(%[src]), %%rax\n\t
		movq	-8(%[src],%[size]), %[src]\n\t
		movq	%%rax, (%[dst])\n\t
		movq	%[src], -8(%[dst],%[size])\n\t
		popq	%%rax\n\t
		ret\n\t
		): [dst]"+r"(dst), [src]"+r"(src), [size]"+r"(size)
		:: "rax", "r9", "r10", "r11", "memory"
	);
}
void* fastcpy_tiny(void *dst, const void *src, size_t size) __attribute__((weak,alias("fastcpy")));
#endif/* fastcpy 结束 */


#if VEC_SIZE != 0/* stpcpy 开始 */
/* fast_stpcpy */
# undef CHAR_SIZE
# define CHAR_SIZE 1
# include "fast_stpcpy.S"
/* fast_wstpcpy */
# undef CHAR_SIZE
# define CHAR_SIZE 2
# include "fast_stpcpy.S"
/* fast_dstpcpy */
# undef CHAR_SIZE
# define CHAR_SIZE 4
# include "fast_stpcpy.S"
/* fast_qstpcpy */
# if VEC_SIZE > 16
#   undef CHAR_SIZE
#   define CHAR_SIZE 8
#   include "fast_stpcpy.S"
# endif
#else/* NO SSE version */
# if 0/* C语言版本, 适用于小端字节序 */
uint8_t* fast_stpcpy(uint8_t *dst, const uint8_t *src) {
	while(((size_t)src & 7) != 0) {
		uint8_t ch = *dst = *src;
		if(ch == 0) return dst;
		++src;
		++dst;
	}
	uint64_t value, magic = 0xfefefefefefefeffLL;
	do {
		value = *(uint64_t*)src;
		src += 8;
		uint64_t temp = value;
		temp += magic;
		if(value < -magic) break;
		temp ^= value;
		temp |= magic;
		if(++temp != 0) break;
		*(uint64_t*)dst = value;
		dst += 8;
	} while(1);
	do {
		uint8_t ch = *dst = value & 0xff;
		if(ch == 0) break;
		++dst;
		ch = *dst = (value >> 8) & 0xff;
		if(ch == 0) break;
		++dst;
		value >>= 16;
	} while(1);
	return dst;
}
# else
__attribute__((naked,optimize(ASM_OPT(align-functions=CACHELINE_SIZE))))
uint8_t* fast_stpcpy(uint8_t *dst, const uint8_t *src) {
	asm volatile(ASM_OPT(
		testb	$7, %b[src]\n\t
		je  	1f\n\t
		0:\n\t
		movzbl	(%[src]), %%eax\n\t
		testb	%%al, %%al\n\t
		movb	%%al, (%[dst])\n\t
		je  	4f\n\t
		inc 	%[src]\n\t
		inc 	%[dst]\n\t
		testb	$7, %b[src]\n\t
		jne 	0b\n\t
		1:\n\t
		mov 	$0xfefefefefefefeff, %%r8\n\t
		mov 	(%[src]), %%rax\n\t
		add 	$8, %[src]\n\t
		mov 	%%rax, %%r9\n\t
		add 	%%r8, %%r9\n\t
		jnc 	3f\n\t
		xor 	%%rax, %%r9\n\t
		or  	%%r8, %%r9\n\t
		inc 	%%r9\n\t
		jne 	3f\n\t
		.p2align 4,,10\n\t
		2:\n\t
		mov 	%%rax, (%[dst])\n\t
		add 	$8, %[dst]\n\t
		mov 	(%[src]), %%rax\n\t
		add 	$8, %[src]\n\t
		mov 	%%rax, %%r9\n\t
		add 	%%r8, %%r9\n\t
		jnc 	3f\n\t
		xor 	%%rax, %%r9\n\t
		or  	%%r8, %%r9\n\t
		inc 	%%r9\n\t
		je  	2b\n\t
		3:\n\t
		movb	%%al, (%[dst])\n\t
		testb	%%al, %%al\n\t
		je  	4f\n\t
		inc 	%[dst]\n\t
		movb	%%ah, (%[dst])\n\t
		testb	%%ah, %%ah\n\t
		je  	4f\n\t
		inc 	%[dst]\n\t
		shr 	$16, %%rax\n\t
		jmp 	3b\n\t
		4:\n\t
		mov 	%[dst], %%rax\n\t
		ret\n\t
		): [dst]"+r"(dst), [src]"+r"(src):: "rax", "r8", "r9", "memory"
	);
}
# endif

__attribute__((naked,optimize(ASM_OPT(align-functions=CACHELINE_SIZE))))
uint16_t* fast_wstpcpy(uint16_t *dst, const uint16_t *src) {
	asm volatile(ASM_OPT(
		testb	$1, %b[src]\n\t
		jne 	fast_wstpcpy_unalign\n\t
		testb	$7, %b[src]\n\t
		je  	1f\n\t
		0:\n\t
		movzwl	(%[src]), %%eax\n\t
		testw	%%ax, %%ax\n\t
		movw	%%ax, (%[dst])\n\t
		je  	4f\n\t
		add 	$2, %[src]\n\t
		add 	$2, %[dst]\n\t
		testb	$7, %b[src]\n\t
		jne 	0b\n\t
		1:\n\t
		mov 	$0xfffefffefffeffff, %%r8\n\t
		mov 	(%[src]), %%rax\n\t
		add 	$8, %[src]\n\t
		mov 	%%rax, %%r9\n\t
		add 	%%r8, %%r9\n\t
		jnc 	3f\n\t
		.p2align 4,,10\n\t
		2:\n\t
		xor 	%%rax, %%r9\n\t
		or  	%%r8, %%r9\n\t
		inc 	%%r9\n\t
		jne 	3f\n\t
		mov 	%%rax, (%[dst])\n\t
		add 	$8, %[dst]\n\t
		mov 	(%[src]), %%rax\n\t
		add 	$8, %[src]\n\t
		mov 	%%rax, %%r9\n\t
		add 	%%r8, %%r9\n\t
		jc  	2b\n\t
		3:\n\t
		movw	%%ax, (%[dst])\n\t
		testw	%%ax, %%ax\n\t
		je  	4f\n\t
		add 	$2, %[dst]\n\t
		shr 	$16, %%rax\n\t
		jmp 	3b\n\t
		4:\n\t
		mov 	%[dst], %%rax\n\t
		ret\n\t
		): [dst]"+r"(dst), [src]"+r"(src):: "rax", "r8", "r9", "memory"
	);
}
#endif/* stpcpy 结束 */

/* 仅当源地址不对齐到自然边界时才应使用 */
__attribute__((used,naked))
static uint16_t* fast_wstpcpy_unalign(uint16_t *dst, const uint16_t *src) {
	//assert((size_t)src & 1);
	asm volatile(ASM_OPT(
		inc 	%[src]\n\t
		testb	$7, %b[src]\n\t
		je  	1f\n\t
		0:\n\t
		movzwl	-1(%[src]), %%eax\n\t
		testw	%%ax, %%ax\n\t
		movw	%%ax, (%[dst])\n\t
		je  	4f\n\t
		add 	$2, %[src]\n\t
		add 	$2, %[dst]\n\t
		testb	$7, %b[src]\n\t
		jne 	0b\n\t
		1:\n\t
		mov 	$0xfffefffefffeffff, %%r8\n\t
		movzbl	-1(%[src]), %%r10d\n\t
		.p2align 4,,10\n\t
		2:\n\t
		mov 	(%[src]), %%rax\n\t
		add 	$8, %[src]\n\t
		rol 	$8, %%rax\n\t
		xchg	%%r10b, %%al\n\t
		mov 	%%rax, %%r9\n\t
		add 	%%r8, %%r9\n\t
		jnc 	3f\n\t
		xor 	%%rax, %%r9\n\t
		or  	%%r8, %%r9\n\t
		add 	$1, %%r9\n\t
		jne 	3f\n\t
		mov 	%%rax, (%[dst])\n\t
		add 	$8, %[dst]\n\t
		jmp 	2b\n\t
		.p2align 4,,10\n\t
		3:\n\t
		movw	%%ax, (%[dst])\n\t
		testw	%%ax, %%ax\n\t
		je  	4f\n\t
		add 	$2, %[dst]\n\t
		shr 	$16, %%rax\n\t
		jmp 	3b\n\t
		4:\n\t
		mov 	%[dst], %%rax\n\t
		ret\n\t
		): [dst]"+r"(dst), [src]"+r"(src):: "rax", "r8", "r9", "r10", "memory"
	);
}

__attribute__((used,naked))
#if VEC_SIZE != 0/* stpcpy 补充 */
static uint32_t* fast_dstpcpy_unalign(uint32_t *dst, const uint32_t *src)
#else
uint32_t* fast_dstpcpy(uint32_t *dst, const uint32_t *src)
#endif
{
	asm volatile(ASM_OPT(
		0:\n\t
		mov 	(%[src]), %%eax\n\t
		mov 	%%eax, (%[dst])\n\t
		add 	$4, %[src]\n\t
		add 	$4, %[dst]\n\t
		test	%%eax, %%eax\n\t
		jne  	0b\n\t
		lea 	-4(%[dst]), REG_SYS(ax)\n\t
		ret\n\t
		): [dst]"+r"(dst), [src]"+r"(src):: "rax", "memory"
	);
}

__attribute__((used,naked))
#if VEC_SIZE != 0 && VEC_SIZE != 16/* stpcpy 补充 */
static uint64_t* fast_qstpcpy_unalign(uint64_t *dst, const uint64_t *src)
#else
uint64_t* fast_qstpcpy(uint64_t *dst, const uint64_t *src)
#endif
{
	asm volatile(ASM_OPT(
		0:\n\t
		mov 	(%[src]), %%rax\n\t
		add 	$8, %[src]\n\t
		mov 	%%rax, (%[dst])\n\t
		add 	$8, %[dst]\n\t
		test	%%rax, %%rax\n\t
		jne  	0b\n\t
		lea 	-8(%[dst]), REG_SYS(ax)\n\t
		ret\n\t
		): [dst]"+r"(dst), [src]"+r"(src):: "rax", "memory"
	);
}





