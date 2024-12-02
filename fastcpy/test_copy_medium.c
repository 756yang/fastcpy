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

// 需实现两种, 前向复制和後向复制, 以最大限度避免 4K 伪混叠

/**
 * 读写首个和末个 vmm 後, 将目的地址对齐到 vmm 大小, 循环复制
 * 8 个或 4 个 vmm 最後剩余的以 tiny_copy 算法复制
 */

__attribute__((optimize("align-functions=4096")))
void* copy_forward(void *dst_, const void *src_, size_t size) {// 这个才是最快的
	VMM_TYPE() vmm0, vmm1, vmm2, vmm3, vmm4, vmm5;
#if 0
	uint8_t *dst = (uint8_t*)dst_, *src = (uint8_t*)src_;
	vmm0 = SIMD_OPT(loadu_ps)((float*)src);
	vmm1 = SIMD_OPT(loadu_ps)((float*)(src + size - VEC_SIZE));
	uint8_t *dsa = (uint8_t*)((size_t)dst | VEC_SIZE - 1) + 1 + 5 * VEC_SIZE;
	uint8_t *dsz = dst + size - VEC_SIZE;
	SIMD_OPT(storeu_ps)((float*)dst, vmm0);
	SIMD_OPT(storeu_ps)((float*)dsz, vmm1);
	uint8_t *sra = src + (dsa - dst);
	size_t load_size = 6 * VEC_SIZE;
#else
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
#endif
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
	STORE_ASM1(movaps, vmm0, -5*VEC_SIZE, dsa);
	STORE_ASM1(movaps, vmm1, -4*VEC_SIZE, dsa);
	STORE_ASM1(movaps, vmm2, -3*VEC_SIZE, dsa);
	STORE_ASM1(movaps, vmm3, -2*VEC_SIZE, dsa);
	STORE_ASM1(movaps, vmm4, -1*VEC_SIZE, dsa);
	STORE_ASM1(movaps, vmm5, 0*VEC_SIZE, dsa);
	asm volatile("add %[lz], %[da]" :[da]"+r"(dsa) :[lz]"r"(load_size));
	asm volatile("cmp %[da], %[dz]\n\t""ja 0b\n\t" ::[da]"r"(dsa), [dz]"r"(dsz));
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


/* glibc 的方法, size >= 5 * VEC_SIZE */
__attribute__((optimize("align-functions=4096")))
void* copy_unroll4(void *dst_, const void *src_, size_t size) {
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
		STORE_ASM1(movaps, vmm0, -4*VEC_SIZE, dsa);
		STORE_ASM1(movaps, vmm1, -3*VEC_SIZE, dsa);
		STORE_ASM1(movaps, vmm2, -2*VEC_SIZE, dsa);
		STORE_ASM1(movaps, vmm3, -VEC_SIZE, dsa);
	} while(dsz > dsa);
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


#if VEC_SIZE != 16 && VEC_SIZE != 32 && VEC_SIZE != 64
# error Unsupported VEC_SIZE!
#endif

__attribute__((optimize("align-functions=4096")))
X64_CALL void* copy_unroll6(void *dst_, const void *src_, size_t size) {
	VMM_TYPE() vmm0, vmm1, vmm2, vmm3, vmm4, vmm5;
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
#if VEC_SIZE == 32
	asm volatile(ASM_OPT(
		VEX_PRE(movups) (%[sa]), %[v1]\n\t
		VEX_PRE(movups) -%c[i](%[sa],%[dz]), %[v0]\n\t
		movq %[da], %[dd]\n\t
		leaq -3*%c[i](%[da],%[dz]), %[dz]\n\t
		VEX_PRE(movups) %[v1], (%[da])\n\t
		orq $(%c[i]-1), %[da]\n\t
		addq $(3*%c[i]+1), %[da]\n\t
		VEX_PRE(movups) %[v0], 2*%c[i](%[dz])\n\t
		subq %[dd], %[sa]\n\t
		addq %[da], %[sa]\n\t
		movl $(6*%c[i]), %k[lz]\n\t
		): [sa]"+r"(sra), [da]"+r"(dsa), [dz]"+r"(dsz), [dd]"=r"(dst), [lz]"=r"(load_size), [v0]"=x"(vmm0), [v1]"=x"(vmm1)
		: [i]"i"(VEC_SIZE) : "memory"
	);// 这段指令大小 0x30 bytes
#else/* 编译器优化不彻底, 需手动汇编 */
	dst = dsa;/* 注意, 6*VEC_SIZE 立即数在指令中占四字节, 这使指令更长而效率低 */
	vmm0 = SIMD_OPT(loadu_ps)((float*)sra);
	vmm1 = SIMD_OPT(loadu_ps)((float*)(sra + size - VEC_SIZE));
	dsa = (uint8_t*)((size_t)dst | VEC_SIZE - 1) + 1 + 3 * VEC_SIZE;
	dsz = dst + size - 3 *VEC_SIZE;
	SIMD_OPT(storeu_ps)((float*)dst, vmm0);
	SIMD_OPT(storeu_ps)((float*)(dsz + 2 * VEC_SIZE), vmm1);
	sra = sra + (dsa - dst);
	load_size = 6 * VEC_SIZE;
#endif
	/* 循环复制 */
	asm volatile(".p2align 4\n\t""0:\n\t"::);/* 指令字节优化 */
	LOAD_ASM1(movups, vmm0, -3*VEC_SIZE, sra);
	LOAD_ASM1(movups, vmm1, -2*VEC_SIZE, sra);
	LOAD_ASM1(movups, vmm2, -1*VEC_SIZE, sra);
	LOAD_ASM1(movups, vmm3, 0*VEC_SIZE, sra);
	LOAD_ASM1(movups, vmm4, 1*VEC_SIZE, sra);
	LOAD_ASM1(movups, vmm5, 2*VEC_SIZE, sra);
	// 避免 gcc 错误优化降低性能
	asm volatile("add %[lz], %[sa]" :[sa]"+r"(sra) :[lz]"r"(load_size));
	// store_ps 可能生成 movups 指令, 这在旧平台会降低性能
	STORE_ASM1(movaps, vmm0, -3*VEC_SIZE, dsa);
	STORE_ASM1(movaps, vmm1, -2*VEC_SIZE, dsa);
	STORE_ASM1(movaps, vmm2, -1*VEC_SIZE, dsa);
	STORE_ASM1(movaps, vmm3, 0*VEC_SIZE, dsa);
	STORE_ASM1(movaps, vmm4, 1*VEC_SIZE, dsa);
	STORE_ASM1(movaps, vmm5, 2*VEC_SIZE, dsa);
	asm volatile("add %[lz], %[da]" :[da]"+r"(dsa) :[lz]"r"(load_size));
	asm volatile("cmp %[da], %[dz]\n\t""ja 0b\n\t" ::[da]"r"(dsa), [dz]"r"(dsz));
	/* 循环收尾 */
#if 1
	/* 使用循环和分支一样快 */
	asm volatile(ASM_OPT(
		add $(2*%c[i]), %[dz]\n\t
		sub $(3*%c[i]), %[da]\n\t
		sub $(3*%c[i]), %[sa]\n\t
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
#elif 1
	asm volatile(ASM_OPT(
		sub %[da], %[dz]\n\t
#ifdef XMM_NEED_SAVE
		mov %[dd], %[dA]\n\t
#endif
		): [dz]"=r"(size), [dA]"=a"(dst)
		: "0"(dsz), [da]"r"(dsa), [dd]"r"(dst):
	);// 小型分支下的分支表比跳表更快
	if(xsun_likely((int32_t)size > -5 * VEC_SIZE)) {
		LOAD_ASM1(movups, vmm0, -3*VEC_SIZE, sra);
		STORE_ASM1(movaps, vmm0, -3*VEC_SIZE, dsa);
		if(xsun_likely((int32_t)size > -4 * VEC_SIZE)) {
			if(xsun_likely((int32_t)size > -3 * VEC_SIZE)) {
				if(xsun_likely((int32_t)size > -2 * VEC_SIZE)) {
					if(xsun_likely((int32_t)size > -1 * VEC_SIZE)) {
						LOAD_ASM1(movups, vmm4, 1*VEC_SIZE, sra);
						STORE_ASM1(movaps, vmm4, 1*VEC_SIZE, dsa);
					}
					LOAD_ASM1(movups, vmm3, 0*VEC_SIZE, sra);
					STORE_ASM1(movaps, vmm3, 0*VEC_SIZE, dsa);
				}
				LOAD_ASM1(movups, vmm2, -1*VEC_SIZE, sra);
				STORE_ASM1(movaps, vmm2, -1*VEC_SIZE, dsa);
			}
			LOAD_ASM1(movups, vmm1, -2*VEC_SIZE, sra);
			STORE_ASM1(movaps, vmm1, -2*VEC_SIZE, dsa);
		}
	}
#elif VEC_SIZE == 32
	asm volatile(ASM_OPT(
		mov %[da], %[jm]\n\t
		sub %[dz], %[jm]\n\t
		leaq 1f(%%rip), %[dz]\n\t
		shr $5, %[jm]\n\t
		shl $4, %[jm]\n\t
		add %[dz], %[jm]\n\t
		jmp *%[jm]\n\t
		.p2align 4\n\t
		1:\n\t
		VEX_PRE(movups) 0x20(%[sa]), %%ymm0\n\t
		VEX_PRE(movups) %%ymm0, 0x20(%[da])\n\t
		.p2align 4\n\t
		VEX_PRE(movups) 0x00(%[sa]), %%ymm0\n\t
		VEX_PRE(movups) %%ymm0, 0x00(%[da])\n\t
		.p2align 4\n\t
		VEX_PRE(movups) -0x20(%[sa]), %%ymm0\n\t
		VEX_PRE(movups) %%ymm0, -0x20(%[da])\n\t
		.p2align 4\n\t
		VEX_PRE(movups) -0x40(%[sa]), %%ymm0\n\t
		VEX_PRE(movups) %%ymm0, -0x40(%[da])\n\t
		.p2align 4\n\t
		VEX_PRE(movups) -0x60(%[sa]), %%ymm0\n\t
		VEX_PRE(movups) %%ymm0, -0x60(%[da])\n\t
		.p2align 4\n\t
		): [da]"+r"(dsa), [dz]"+r"(dsz), [sa]"+r"(sra), [jm]"+r"(load_size)
		:: "ymm0", "memory"
	);
#else
# error Unsupported VEC_SIZE for jumptable!
#endif
	VZEROUPPER_ASM;
	return dst;
}

__attribute__((naked,optimize("align-functions=4096")))
X64_CALL void* copy_unroll8(void *dst_, const void *src_, size_t size_) {
	register VMM_TYPE() v0 asm(VMM_NAME "0");// 固定寄存器位置
	register VMM_TYPE() v1 asm(VMM_NAME "1");
	register VMM_TYPE() v2 asm(VMM_NAME "2");
	register VMM_TYPE() v3 asm(VMM_NAME "3");
	register VMM_TYPE() v4 asm(VMM_NAME "4");
	register VMM_TYPE() v5 asm(VMM_NAME "5");
	register VMM_TYPE() v6 asm(VMM_NAME "6");
	register VMM_TYPE() v7 asm(VMM_NAME "7");
	// 仅六个 SIMD 寄存器可直接使用
#ifdef XMM_NEED_SAVE/* ms_abi */
	register uint8_t *dst asm("rcx") = (uint8_t*)dst_;
	register uint8_t *src asm("rdx") = (uint8_t*)src_;
	register size_t size asm("r8") = size_;
#else/* sysv_abi */
	register uint8_t *dst asm("rdi") = (uint8_t*)dst_;
	register uint8_t *src asm("rsi") = (uint8_t*)src_;
	register size_t size asm("rdx") = size_;
#endif
	/* 循环准备 */
	asm volatile(ASM_OPT(// 这段代码已经是最优序列了
		leaq	(%[src],%[size]), %%r9\n\t
#ifdef XMM_NEED_SAVE
		subq	$0x20, %%rsp\n\t
		VEX_PRE(movups) %%xmm6, (%%rsp)\n\t
		VEX_PRE(movups) %%xmm7, 0x10(%%rsp)\n\t
#endif
		movq	%[dst], %%rax\n\t
		VEX_PRE(movups) (%[src]), %[v0]\n\t
		movl	$(8*VEC_SIZE), %%r10d\n\t
		VEX_PRE(movups) -VEC_SIZE(%%r9), %[v1]\n\t
		VEX_PRE(movups) -2*VEC_SIZE(%%r9), %[v2]\n\t
		leaq	-4*VEC_SIZE(%[dst],%[size]), %%r8\n\t
		VEX_PRE(movups) -3*VEC_SIZE(%%r9), %[v3]\n\t
		VEX_PRE(movups) -4*VEC_SIZE(%%r9), %[v4]\n\t
		orq 	$(VEC_SIZE-1), %[dst]\n\t
		add 	$(1+4*VEC_SIZE), %[dst]\n\t
		VEX_PRE(movups) %[v1], 3*VEC_SIZE(%%r8)\n\t
		VEX_PRE(movups) %[v2], 2*VEC_SIZE(%%r8)\n\t
		subq	%%rax, %[src]\n\t
		VEX_PRE(movups) %[v3], 1*VEC_SIZE(%%r8)\n\t
		VEX_PRE(movups) %[v4], 0*VEC_SIZE(%%r8)\n\t
		addq	%[dst], %[src]\n\t
		VEX_PRE(movups) %[v0], (%%rax)\n\t
		.p2align 4\n\t
		0:\n\t
		VEX_PRE(movups) -4*VEC_SIZE(%[src]), %[v7]\n\t
		VEX_PRE(movups) -3*VEC_SIZE(%[src]), %[v6]\n\t
		VEX_PRE(movups) -2*VEC_SIZE(%[src]), %[v5]\n\t
		VEX_PRE(movups) -1*VEC_SIZE(%[src]), %[v4]\n\t
		VEX_PRE(movups) 0*VEC_SIZE(%[src]), %[v3]\n\t
		VEX_PRE(movups) 1*VEC_SIZE(%[src]), %[v2]\n\t
		VEX_PRE(movups) 2*VEC_SIZE(%[src]), %[v1]\n\t
		VEX_PRE(movups) 3*VEC_SIZE(%[src]), %[v0]\n\t
		add 	%%r10, %[src]\n\t
		VEX_PRE(movaps) %[v7], -4*VEC_SIZE(%[dst])\n\t
		VEX_PRE(movaps) %[v6], -3*VEC_SIZE(%[dst])\n\t
		VEX_PRE(movaps) %[v5], -2*VEC_SIZE(%[dst])\n\t
		VEX_PRE(movaps) %[v4], -1*VEC_SIZE(%[dst])\n\t
		VEX_PRE(movaps) %[v3], 0*VEC_SIZE(%[dst])\n\t
		VEX_PRE(movaps) %[v2], 1*VEC_SIZE(%[dst])\n\t
		VEX_PRE(movaps) %[v1], 2*VEC_SIZE(%[dst])\n\t
		VEX_PRE(movaps) %[v0], 3*VEC_SIZE(%[dst])\n\t
		add 	%%r10, %[dst]\n\t
		cmp 	%[dst], %%r8\n\t
		ja  	0b\n\t
		sub 	%[dst], %%r8\n\t
#ifdef XMM_NEED_SAVE
		VEX_PRE(movups) 0x10(%%rsp), %%xmm7\n\t
		VEX_PRE(movups) (%%rsp), %%xmm6\n\t
		addq	$0x20, %%rsp\n\t
#endif
		cmp 	$(-4*VEC_SIZE), %%r8\n\t
		jle 	0f\n\t
		VEX_PRE(movups) -4*VEC_SIZE(%[src]), %[v3]\n\t
		VEX_PRE(movups) -3*VEC_SIZE(%[src]), %[v2]\n\t
		VEX_PRE(movups) -2*VEC_SIZE(%[src]), %[v1]\n\t
		VEX_PRE(movups) -1*VEC_SIZE(%[src]), %[v0]\n\t
		VEX_PRE(movaps) %[v3], -4*VEC_SIZE(%[dst])\n\t
		VEX_PRE(movaps) %[v2], -3*VEC_SIZE(%[dst])\n\t
		VEX_PRE(movaps) %[v1], -2*VEC_SIZE(%[dst])\n\t
		VEX_PRE(movaps) %[v0], -1*VEC_SIZE(%[dst])\n\t
		0:\n\t
		VZEROUPPER\n\t
		ret\n\t
		): [dst]"+r"(dst), [src]"+r"(src), [size]"+r"(size), [v0]"=x"(v0), [v1]"=x"(v1), [v2]"=x"(v2), [v3]"=x"(v3), [v4]"=x"(v4), [v5]"=x"(v5), [v6]"=x"(v6), [v7]"=x"(v7)
		: : "rax", "memory"
	);
}



__attribute__((naked,optimize("align-functions=4096")))
X64_CALL void* copy_unroll9(void *dst_, const void *src_, size_t size_) {
	register VMM_TYPE() v0 asm(VMM_NAME "0");// 固定寄存器位置
	register VMM_TYPE() v1 asm(VMM_NAME "1");
	register VMM_TYPE() v2 asm(VMM_NAME "2");
	register VMM_TYPE() v3 asm(VMM_NAME "3");
	register VMM_TYPE() v4 asm(VMM_NAME "4");
	register VMM_TYPE() v5 asm(VMM_NAME "5");
	register VMM_TYPE() v6 asm(VMM_NAME "6");
	register VMM_TYPE() v7 asm(VMM_NAME "7");
	// 仅六个 SIMD 寄存器可直接使用
#ifdef XMM_NEED_SAVE/* ms_abi */
	register uint8_t *dst asm("rcx") = (uint8_t*)dst_;
	register uint8_t *src asm("rdx") = (uint8_t*)src_;
	register size_t size asm("r8") = size_;
#else/* sysv_abi */
	register uint8_t *dst asm("rdi") = (uint8_t*)dst_;
	register uint8_t *src asm("rsi") = (uint8_t*)src_;
	register size_t size asm("rdx") = size_;
#endif
	/* 循环准备 */
	asm volatile(ASM_OPT(// 这段代码已经是最优序列了
		movq	%[dst], %%rax\n\t
		andl	$(VEC_SIZE-1), %k[dst]\n\t
#ifdef XMM_NEED_SAVE
		subq	$0x20, %%rsp\n\t
		VEX_PRE(movups) %%xmm6, (%%rsp)\n\t
		VEX_PRE(movups) %%xmm7, 0x10(%%rsp)\n\t
#endif
		sub 	$(VEC_SIZE), %[dst]\n\t
		VEX_PRE(movups) (%[src]), %[v0]\n\t
		VEX_PRE(movups) -VEC_SIZE(%[src],%[size]), %[v1]\n\t
		sub 	%[dst], %[src]\n\t
		VEX_PRE(movups) %[v0], (%%rax)\n\t
		VEX_PRE(movups) %[v1], -VEC_SIZE(%%rax,%[size])\n\t
		lea 	-1(%[size],%[dst]), %%r8\n\t
		VEX_PRE(movups) (%[src]), %[v6]\n\t
		VEX_PRE(movups) VEC_SIZE(%[src]), %[v5]\n\t
		neg 	%[dst]\n\t
		VEX_PRE(movups) 2*VEC_SIZE(%[src]), %[v4]\n\t
		VEX_PRE(movups) 3*VEC_SIZE(%[src]), %[v3]\n\t
		sub 	$(-4*VEC_SIZE), %[src]\n\t
		add 	%%rax, %[dst]\n\t
		VEX_PRE(movups) (%[src]), %[v2]\n\t
		VEX_PRE(movups) VEC_SIZE(%[src]), %[v1]\n\t
		VEX_PRE(movups) 2*VEC_SIZE(%[src]), %[v0]\n\t
		mov 	%%r8, %%r9\n\t
		andl	$(7*VEC_SIZE), %%r8d\n\t
		VEX_PRE(movaps) %[v6], (%[dst])\n\t
		VEX_PRE(movaps) %[v5], 1*VEC_SIZE(%[dst])\n\t
		add 	%%r8, %[src]\n\t
		VEX_PRE(movaps) %[v4], 2*VEC_SIZE(%[dst])\n\t
		VEX_PRE(movaps) %[v3], 3*VEC_SIZE(%[dst])\n\t
		sub 	$(-4*VEC_SIZE), %[dst]\n\t
		mov 	$(-8*VEC_SIZE), %%r10\n\t
		VEX_PRE(movaps) %[v2], (%[dst])\n\t
		VEX_PRE(movaps) %[v1], VEC_SIZE(%[dst])\n\t
		VEX_PRE(movaps) %[v0], 2*VEC_SIZE(%[dst])\n\t
		add 	%%r8, %[dst]\n\t
		and 	%%r10, %%r9\n\t
		je  	1f\n\t
		.p2align 4\n\t
		0:\n\t
		VEX_PRE(movups) -4*VEC_SIZE(%[src]), %[v7]\n\t
		VEX_PRE(movups) -3*VEC_SIZE(%[src]), %[v6]\n\t
		VEX_PRE(movups) -2*VEC_SIZE(%[src]), %[v5]\n\t
		VEX_PRE(movups) -1*VEC_SIZE(%[src]), %[v4]\n\t
		VEX_PRE(movups) 0*VEC_SIZE(%[src]), %[v3]\n\t
		VEX_PRE(movups) 1*VEC_SIZE(%[src]), %[v2]\n\t
		VEX_PRE(movups) 2*VEC_SIZE(%[src]), %[v1]\n\t
		VEX_PRE(movups) 3*VEC_SIZE(%[src]), %[v0]\n\t
		sub 	%%r10, %[src]\n\t
		VEX_PRE(movaps) %[v7], -4*VEC_SIZE(%[dst])\n\t
		VEX_PRE(movaps) %[v6], -3*VEC_SIZE(%[dst])\n\t
		VEX_PRE(movaps) %[v5], -2*VEC_SIZE(%[dst])\n\t
		VEX_PRE(movaps) %[v4], -1*VEC_SIZE(%[dst])\n\t
		VEX_PRE(movaps) %[v3], 0*VEC_SIZE(%[dst])\n\t
		VEX_PRE(movaps) %[v2], 1*VEC_SIZE(%[dst])\n\t
		VEX_PRE(movaps) %[v1], 2*VEC_SIZE(%[dst])\n\t
		VEX_PRE(movaps) %[v0], 3*VEC_SIZE(%[dst])\n\t
		sub 	%%r10, %[dst]\n\t
		add 	%%r10, %%r9\n\t
		jne  	0b\n\t
		1:\n\t
#ifdef XMM_NEED_SAVE
		VEX_PRE(movups) 0x10(%%rsp), %%xmm7\n\t
		VEX_PRE(movups) (%%rsp), %%xmm6\n\t
		addq	$0x20, %%rsp\n\t
#endif
		VZEROUPPER\n\t
		ret\n\t
		): [dst]"+r"(dst), [src]"+r"(src), [size]"+r"(size), [v0]"=x"(v0), [v1]"=x"(v1), [v2]"=x"(v2), [v3]"=x"(v3), [v4]"=x"(v4), [v5]"=x"(v5), [v6]"=x"(v6), [v7]"=x"(v7)
		: : "rax", "memory"
	);
}








