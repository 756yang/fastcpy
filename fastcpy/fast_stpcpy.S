
#ifndef VZERO/* 定义存储 zero 的 SIMD 寄存器 */
# ifdef XMM_NEED_SAVE
#   define VZERO v5
# else
#   define VZERO v6
#  endif
#endif

#if VEC_SIZE < 32 && CHAR_SIZE == 8
# error SSE and SSE2 not support pcmpeqq instructions for qstpcpy!
#endif

#undef L_STPCPY
#undef PMIN
#undef PCMPEQ
#undef VPTESTN
#undef KMOV
#undef KORTEST
#undef KREG
#define KREG k

__attribute__((used,naked))
#if CHAR_SIZE == 1
# define PMIN pminub
# define PCMPEQ pcmpeqb
# define VPTESTN vptestnmb
# define KMOV kmovq
# define KORTEST kortestq
# undef KREG
# define KREG
# define L_STPCPY(label) .L_stpcpy_##label
static uint8_t* fast_stpcpy_page_cross(uint8_t *dst_, const uint8_t *src_)
#elif CHAR_SIZE == 2
# define PMIN pminuw/* SSE41 */
# define PCMPEQ pcmpeqw
# define VPTESTN vptestnmw
# define KMOV kmovd
# define KORTEST kortestd
# define L_STPCPY(label) .L_wstpcpy_##label
static uint16_t* fast_wstpcpy_page_cross(uint16_t *dst_, const uint16_t *src_)
#elif CHAR_SIZE == 4
# define PMIN pminud/* SSE41 */
# define PCMPEQ pcmpeqd
# define VPTESTN vptestnmd
# define KMOV kmovw
# define KORTEST kortestw
# define L_STPCPY(label) .L_dstpcpy_##label
static uint32_t* fast_dstpcpy_page_cross(uint32_t *dst_, const uint32_t *src_)
#elif CHAR_SIZE == 8
# define PMIN pminuq/* AVX512 */
# define PCMPEQ pcmpeqq/* SSE41 */
# define VPTESTN vptestnmq
# define KMOV kmovb
# define KORTEST kortestb
# define L_STPCPY(label) .L_qstpcpy_##label
static uint64_t* fast_qstpcpy_page_cross(uint64_t *dst_, const uint64_t *src_)
#else
# error Unsupported CHAR_SIZE!
#endif
{
#ifdef XMM_NEED_SAVE/* ms_abi */
	register void *dst asm("rcx") = dst_;
	register const void *src asm("rdx") = src_;
	register size_t zdx asm("r8");
	register size_t zcx asm("r9");
#else/* sysv_abi */
	register void *dst asm("rdi") = dst_;
	register const void *src asm("rsi") = src_;
	register size_t zdx asm("rdx");
	register size_t zcx asm("rcx");
#endif
	register __vmm v0 asm(VMM_NAME "0");
	register __vmm v1 asm(VMM_NAME "1");
	register __vmm v2 asm(VMM_NAME "2");
	register __vmm v3 asm(VMM_NAME "3");
	register __vmm v4 asm(VMM_NAME "4");
	register __vmm v5 asm(VMM_NAME "5");
	register __vmm v6 asm(VMM_NAME "6");
	asm volatile(ASM_OPT(
L_STPCPY(page_cross):\n\t
#if VEC_SIZE < 64
		NOVEX_ARG(andl	$(VEC_SIZE-1), %%eax)\n\t
		movq	%[src], %[zdx]\n\t
		andq	$(-VEC_SIZE), %[zdx]\n\t
		VEX_PRE_ARG3(PCMPEQ, (%[zdx]), %[VZERO], %[v4], movdqa)\n\t
		VEX_PRE(pmovmskb) %[v4], %k[zdx]\n\t
		shrxl	%%eax, %k[zdx], %k[zdx]\n\t
#else
		vpxorq %x[VZERO], %x[VZERO], %x[VZERO]\n\t
		movq	%[src], %[zdx]\n\t
		andq	$(-VEC_SIZE), %[zdx]\n\t
		VEX_PRE(PCMPEQ) (%[zdx]), %[VZERO], %%k0\n\t
		KMOV	%%k0, %KREG[zdx]\n\t
# if CHAR_SIZE > 1
		andl	$(VEC_SIZE-1), %%eax\n\t
		shrl	$((CHAR_SIZE/2)-(CHAR_SIZE/8)), %%eax\n\t
		shrx	%%eax, %KREG[zdx], %KREG[zdx]\n\t
# else
		shrx	%%rax, %KREG[zdx], %KREG[zdx]\n\t
# endif
#endif
#if 0/* Microsoft x64 的 src, dst 不是 rsi, rdi 寄存器, 故不采用 rep movsb */
		/* 此为冷代码, 因此积极优化代码大小  */
		/* 这会将后面的结果添加一次，从而获得正确的复制边界。
		注意：这永远无法将非零 rcx 清零，因为在跨页情况下
		rsi 无法对齐，并且我们已经通过错位将 rcx 右移 */
		shll	$(CHAR_SIZE), %k[zdx]\n\t
		je  	L_STPCPY(page_cross_continue)\n\t
		bsfl	%k[zdx], %k[zdx]\n\t
		rep movsb\n\t
		leaq	-CHAR_SIZE(%[dst]), %%rax\n\t
		VZEROUPPER\n\t
		ret\n\t
#else/* 复用 fastcpy 的代码 */
		mov 	%[dst], %%rax\n\t
# if VEC_SIZE < 64
		shll	$(CHAR_SIZE), %k[zdx]\n\t
		je  	L_STPCPY(page_cross_continue)\n\t
		bsfl	%k[zdx], %k[zdx]\n\t
		lea 	-CHAR_SIZE(%[dst],%[zdx]), %%r9\n\t
# else
		shl 	$1, %KREG[zdx]\n\t
		je  	L_STPCPY(page_cross_continue)\n\t
		bsf 	%KREG[zdx], %KREG[zdx]\n\t
		lea 	-CHAR_SIZE(%[dst],%[zdx],CHAR_SIZE), %%r9\n\t
#   if CHAR_SIZE > 1
		shl 	$((CHAR_SIZE/2)-(CHAR_SIZE/8)), %KREG[zdx]\n\t
#   endif
# endif
		VZEROUPPER\n\t
		call	.F_fastcpy_less_vec\n\t
		mov 	%%r9, %%rax\n\t
		ret\n\t
#endif
		): [dst]"+r"(dst), [src]"+r"(src), [zdx]"=r"(zdx), [zcx]"=r"(zcx), [v0]"=x"(v0), [v1]"=x"(v1), [v2]"=x"(v2), [v3]"=x"(v3), [v4]"=x"(v4), [v5]"=x"(v5), [v6]"=x"(v6)
		:: "rax" EVEX_ARG(,"k0","k1"), "memory"
	);
}

__attribute__((naked,optimize(ASM_OPT(align-functions=CACHELINE_SIZE))))
#if CHAR_SIZE == 1
uint8_t* fast_stpcpy(uint8_t *dst_, const uint8_t *src_)
#elif CHAR_SIZE == 2
uint16_t* fast_wstpcpy(uint16_t *dst_, const uint16_t *src_)
#elif CHAR_SIZE == 4
uint32_t* fast_dstpcpy(uint32_t *dst_, const uint32_t *src_)
#elif CHAR_SIZE == 8
uint64_t* fast_qstpcpy(uint64_t *dst_, const uint64_t *src_)
#endif
{
#ifdef XMM_NEED_SAVE/* ms_abi */
	register void *dst asm("rcx") = dst_;
	register const void *src asm("rdx") = src_;
	register size_t zdx asm("r8");
	register size_t zcx asm("r9");
#else/* sysv_abi */
	register void *dst asm("rdi") = dst_;
	register const void *src asm("rsi") = src_;
	register size_t zdx asm("rdx");
	register size_t zcx asm("rcx");
#endif
	register __vmm v0 asm(VMM_NAME "0");
	register __vmm v1 asm(VMM_NAME "1");
	register __vmm v2 asm(VMM_NAME "2");
	register __vmm v3 asm(VMM_NAME "3");
	register __vmm v4 asm(VMM_NAME "4");
	register __vmm v5 asm(VMM_NAME "5");
	register __vmm v6 asm(VMM_NAME "6");
	asm volatile(ASM_OPT(
#if CHAR_SIZE > 1
		testb	$(CHAR_SIZE-1), %b[src]\n\t
# if CHAR_SIZE == 2
		jne 	fast_wstpcpy_unalign\n\t
# elif CHAR_SIZE == 4
		jne 	fast_dstpcpy_unalign\n\t
# elif CHAR_SIZE == 8
		jne 	fast_qstpcpy_unalign\n\t
# endif
#endif
#if VEC_SIZE < 64
		VEX_PRE(pxor) %x[VZERO], VEX_ARG(%x[VZERO],) %x[VZERO]\n\t
#endif
		movl	%k[src], %%eax\n\t
		andl	$(PAGE_SIZE-1), %%eax\n\t
		cmpl	$(PAGE_SIZE-VEC_SIZE), %%eax\n\t
		ja  	L_STPCPY(page_cross)\n\t// 跳转当 [src, src+VEC_SIZE) 位置跨页边界
L_STPCPY(page_cross_continue):\n\t
		EVEX_PICK(vmovdqu64,movdqu) (%[src]), %[v0]\n\t
#if VEC_SIZE < 64
		VEX_PRE_ARG3(PCMPEQ, %[v0], %[VZERO], %[v4], movdqa)\n\t
		VEX_PRE(pmovmskb) %[v4], %k[zdx]\n\t
		testl	%k[zdx], %k[zdx]\n\t
		je  	L_STPCPY(more_1x_vec)\n\t
		/* 不再使用 ymm 寄存器故而先 vzeroupper */
		VZEROUPPER\n\t
		//xorl	%k[zcx], %k[zcx]\n\t
		bsfl	%k[zdx], %k[zcx]\n\t
		leaq	(%[dst], %[zcx]), %%rax\n\t
#else
		VPTESTN %[v0], %[v0], %%k0\n\t
		KMOV	%%k0, %KREG[zdx]\n\t
		test	%KREG[zdx], %KREG[zdx]\n\t
		je  	L_STPCPY(more_1x_vec)\n\t
		//xorq	%KREG[zcx], %KREG[zcx]\n\t
		bsf 	%KREG[zdx], %KREG[zcx]\n\t
		leaq	(%[dst],%[zcx],CHAR_SIZE), %%rax\n\t
#endif
		/* 通过 mask 位测试决定复制方式, 低位为零代表低区间没有NULL字符 */
#if VEC_SIZE >= 64 /* 32~64 字节处理 */
# if CHAR_SIZE == 1
		testl	%k[zdx], %k[zdx]\n\t
		jne 	L_STPCPY(copy_0_31)\n\t
# elif CHAR_SIZE == 2
		testw	%w[zdx], %w[zdx]\n\t
		jne 	L_STPCPY(copy_0_31)\n\t
# elif CHAR_SIZE == 4
		testb	%b[zdx], %b[zdx]\n\t
		jne 	L_STPCPY(copy_0_31)\n\t
# elif CHAR_SIZE == 8
		testb	$0x7, %b[zdx]\n\t
		jne 	L_STPCPY(copy_0_31)\n\t
# endif
		EVEX_PICK(vmovdqu64,movdqu) -(32-CHAR_SIZE)(%[src],%[zcx],CHAR_SIZE), %t[v1]\n\t
		EVEX_PICK(vmovdqu64,movdqu) %t[v0], (%[dst])\n\t
		EVEX_PICK(vmovdqu64,movdqu) %t[v1], -(32-CHAR_SIZE)(%%rax)\n\t
		VZEROUPPER\n\t
		ret\n\t
		.p2align 4\n\t
L_STPCPY(copy_0_31):\n\t
		VZEROUPPER\n\t
#endif /* 结束 32~64 字节处理 */

#if VEC_SIZE < 64 || CHAR_SIZE == 1/* 0~31 字节的位测试 */
/* zdx 的每一位对应一个字节 */
# if CHAR_SIZE == 1
#   if VEC_SIZE > 16
		testw	%w[zdx], %w[zdx]\n\t
		je  	L_STPCPY(copy_16_31)\n\t
#   endif
		testb	%b[zdx], %b[zdx]\n\t
		je  	L_STPCPY(copy_8_15)\n\t
		testb	$0x7, %b[zdx]\n\t
		je  	L_STPCPY(copy_4_7)\n\t
# elif CHAR_SIZE == 2
#   if VEC_SIZE > 16
		testw	%w[zdx], %w[zdx]\n\t
		je  	L_STPCPY(copy_16_31)\n\t
#   endif
		testb	$0x3f, %b[zdx]\n\t
		je  	L_STPCPY(copy_8_15)\n\t
# elif CHAR_SIZE == 4
#   if VEC_SIZE > 16
		testw	$0xfff, %w[zdx]\n\t
		je  	L_STPCPY(copy_16_31)\n\t
#   else
		testb	%b[zdx], %b[zdx]\n\t
		je  	L_STPCPY(copy_8_15)\n\t
#   endif
# elif CHAR_SIZE == 8
#   if VEC_SIZE > 16
		testw	%w[zdx], %w[zdx]\n\t
		je  	L_STPCPY(copy_16_31)\n\t
#   endif
# endif
#elif CHAR_SIZE == 2 /* zdx 的每一位对应两个字节 */
		testb	%b[zdx], %b[zdx]\n\t
		je  	L_STPCPY(copy_16_31)\n\t
		testb	$0x7, %b[zdx]\n\t
		je  	L_STPCPY(copy_8_15)\n\t
#elif CHAR_SIZE == 4 /* zdx 的每一位对应四个字节 */
		testb	$0x7, %b[zdx]\n\t
		je  	L_STPCPY(copy_16_31)\n\t
#elif CHAR_SIZE == 8 /* zdx 的每一位对应八个字节 */
		// 不需要额外操作
#endif /* 结束 0~31 字节的位测试 */

		testl	%k[zcx], %k[zcx]\n\t
		je  	L_STPCPY(set_null_term)\n\t

#if CHAR_SIZE == 1 /* 末尾字节处理 */
		VEX_PRE(movd) %x[v0], %k[zdx]\n\t
		movw	%w[zdx], (%[dst])\n\t
		.p2align 4,,2\n\t
L_STPCPY(set_null_term):\n\t
		movb	$0, (%%rax)\n\t
		ret\n\t
#elif CHAR_SIZE == 2
		VEX_PRE(movd) %x[v0], (%[dst])\n\t
		.p2align 4,,4\n\t
L_STPCPY(set_null_term):\n\t
		movw	$0, (%%rax)\n\t
		ret\n\t
#elif CHAR_SIZE == 4
		VEX_PRE(movq) %x[v0], (%[dst])\n\t
		.p2align 4,,5\n\t
L_STPCPY(set_null_term):\n\t
		movl	$0, (%%rax)\n\t
		ret\n\t
#elif CHAR_SIZE == 8
		VEX_PRE(movdqu) %x[v0], (%[dst])\n\t
		.p2align 4,,6\n\t
L_STPCPY(set_null_term):\n\t
		movq	$0, (%%rax)\n\t
		ret\n\t
#endif /* 结束末尾字节处理 */

#if CHAR_SIZE == 1
		.p2align 4,,12\n\t
L_STPCPY(copy_4_7):\n\t
		movl	-(4-CHAR_SIZE)(%[src],%[zcx] EVEX_ARG(,CHAR_SIZE)), %k[zdx]\n\t
		VEX_PRE(movd) %x[v0], (%[dst])\n\t
		movl	%k[zdx], -(4-CHAR_SIZE)(%%rax)\n\t
		ret\n\t
#endif

#if VEC_SIZE > 16 && (CHAR_SIZE != 8 || VEC_SIZE < 64)
		.p2align 4,,10\n\t
L_STPCPY(copy_16_31):\n\t
		EVEX_PICK(vmovdqu64,movdqu) -(16-CHAR_SIZE)(%[src],%[zcx] EVEX_ARG(,CHAR_SIZE)), %x[v1]\n\t
		EVEX_PICK(vmovdqu64,movdqu) %x[v0], (%[dst])\n\t
		EVEX_PICK(vmovdqu64,movdqu) %x[v1], -(16-CHAR_SIZE)(%%rax)\n\t
		ret\n\t
#endif

#if CHAR_SIZE < 4 || (CHAR_SIZE == 4 && VEC_SIZE == 16)
		.p2align 4,,12\n\t
L_STPCPY(copy_8_15):\n\t
		movq	-(8-CHAR_SIZE)(%[src],%[zcx] EVEX_ARG(,CHAR_SIZE)), %[zdx]\n\t
		VEX_PRE(movq) %x[v0], (%[dst])\n\t
		movq	%[zdx], -(8-CHAR_SIZE)(%%rax)\n\t
		ret\n\t
#endif
		.p2align 4\n\t
L_STPCPY(more_1x_vec):\n\t
		EVEX_PICK(vmovdqu64,movdqu) %[v0], (%[dst])\n\t
		subq	%[src], %[dst]\n\t
		orq 	$(VEC_SIZE-1), %[src]\n\t
		addq	%[src], %[dst]\n\t
		/* src+1, dst+1 已对齐到 VEC_SIZE 字节 */
		EVEX_PICK(vmovdqa64,movdqa) 1(%[src]), %[v1]\n\t
		/* 尝试在多个加载後顺序存储, 为了避免潜在的伪依赖 */
#if VEC_SIZE < 64
		VEX_PRE_ARG3(PCMPEQ, %[v1], %[VZERO], %[v4], movdqa)\n\t
		VEX_PRE(pmovmskb) %[v4], %k[zdx]\n\t
		testl	%k[zdx], %k[zdx]\n\t
		jne 	L_STPCPY(ret_vec_x1)\n\t
#else
		VPTESTN %[v1], %[v1], %%k0\n\t
		KMOV	%%k0, %KREG[zdx]\n\t
		test	%KREG[zdx], %KREG[zdx]\n\t
		jne 	L_STPCPY(ret_vec_x1)\n\t
#endif
		EVEX_PICK(vmovdqa64,movdqa) (VEC_SIZE+1)(%[src]), %[v2]\n\t
		EVEX_PICK(vmovdqu64,movdqu) %[v1], 1(%[dst])\n\t
#if VEC_SIZE < 64
		VEX_PRE_ARG3(PCMPEQ, %[v2], %[VZERO], %[v4], movdqa)\n\t
		VEX_PRE(pmovmskb) %[v4], %k[zdx]\n\t
		testl	%k[zdx], %k[zdx]\n\t
		jne 	L_STPCPY(ret_vec_x2)\n\t
#else
		VPTESTN %[v2], %[v2], %%k0\n\t
		KMOV	%%k0, %KREG[zdx]\n\t
		test	%KREG[zdx], %KREG[zdx]\n\t
		jne 	L_STPCPY(ret_vec_x2)\n\t
#endif
		EVEX_PICK(vmovdqa64,movdqa) (2*VEC_SIZE+1)(%[src]), %[v3]\n\t
		EVEX_PICK(vmovdqu64,movdqu) %[v2], (VEC_SIZE+1)(%[dst])\n\t
#if VEC_SIZE < 64
		VEX_PRE_ARG3(PCMPEQ, %[v3], %[VZERO], %[v4], movdqa)\n\t
		VEX_PRE(pmovmskb) %[v4], %k[zdx]\n\t
		testl	%k[zdx], %k[zdx]\n\t
		jne 	L_STPCPY(ret_vec_x3)\n\t
#else
		VPTESTN %[v3], %[v3], %%k0\n\t
		KMOV	%%k0, %KREG[zdx]\n\t
		test	%KREG[zdx], %KREG[zdx]\n\t
		jne 	L_STPCPY(ret_vec_x3)\n\t
#endif
		EVEX_PICK(vmovdqa64,movdqa) (3*VEC_SIZE+1)(%[src]), %[v0]\n\t
		EVEX_PICK(vmovdqu64,movdqu) %[v3], (2*VEC_SIZE+1)(%[dst])\n\t
#if VEC_SIZE < 64
		VEX_PRE_ARG3(PCMPEQ, %[v0], %[VZERO], %[v4], movdqa)\n\t
		VEX_PRE(pmovmskb) %[v4], %k[zcx]\n\t
		testl	%k[zcx], %k[zcx]\n\t
		jne 	L_STPCPY(ret_vec_x4)\n\t
#else
		VPTESTN %[v0], %[v0], %%k0\n\t
		KMOV	%%k0, %KREG[zcx]\n\t
		test	%KREG[zcx], %KREG[zcx]\n\t
		jne 	L_STPCPY(ret_vec_x4)\n\t
#endif
		EVEX_PICK(vmovdqu64,movdqu) %[v0], (3*VEC_SIZE+1)(%[dst])\n\t
		/* 重新对齐 src+1, dst+1 到 4*VEC_SIZE 字节 */
		subq	%[src], %[dst]\n\t
		incq	%[src]\n\t
		orq 	$(4*VEC_SIZE-1), %[src]\n\t
		/* 提前执行半个循环操作以使循环可以存储开始 */
#if (CHAR_SIZE > 1 && VEC_SIZE == 16) || (CHAR_SIZE == 8 && VEC_SIZE == 32)
		VEX_PRE(movdqa) (0*VEC_SIZE+1)(%[src]), %[v1]\n\t
		VEX_PRE_ARG3(pxor, (1*VEC_SIZE+1)(%[src]), %[v1], %[v0], movdqa)\n\t
		VEX_PRE_ARG3(PCMPEQ, %[v0], %[v1], %[v4], movdqa)\n\t
		VEX_PRE(movdqa) (2*VEC_SIZE+1)(%[src]), %[v3]\n\t
		VEX_PRE_ARG3(pxor, (3*VEC_SIZE+1)(%[src]), %[v3], %[v2], movdqa)\n\t
		VEX_PRE(pxor) %[v0], VEX_ARG(%[v1],) %[v1]\n\t
		VEX_PRE_ARG3(PCMPEQ, %[v1], %[v0], %[v5], movdqa)\n\t
		VEX_PRE(pxor) %[v1], VEX_ARG(%[v0],) %[v0]\n\t
		VEX_PRE(por) %[v5], VEX_ARG(%[v4],) %[v4]\n\t
		VEX_PRE_ARG3(PCMPEQ, %[v2], %[v3], %[v5], movdqa)\n\t
		VEX_PRE(por) %[v5], VEX_ARG(%[v4],) %[v4]\n\t
		VEX_PRE(pxor) %[v2], VEX_ARG(%[v3],) %[v3]\n\t
		VEX_PRE_ARG3(PCMPEQ, %[v3], %[v2], %[v5], movdqa)\n\t
		VEX_PRE(pxor) %[v3], VEX_ARG(%[v2],) %[v2]\n\t
		VEX_PRE(por) %[v5], VEX_ARG(%[v4],) %[v4]\n\t
		VEX_PRE(pmovmskb) %[v4], %k[zcx]\n\t
		addq	%[src], %[dst]\n\t
		testl	%k[zcx], %k[zcx]\n\t
		jne 	L_STPCPY(loop_4x_done)\n\t
#else
		EVEX_PICK(vmovdqa64,movdqa) (0*VEC_SIZE+1)(%[src]), %[v0]\n\t
		EVEX_PICK(vmovdqa64,movdqa) (1*VEC_SIZE+1)(%[src]), %[v1]\n\t
		EVEX_PICK(vmovdqa64,movdqa) (2*VEC_SIZE+1)(%[src]), %[v2]\n\t
		EVEX_PICK(vmovdqa64,movdqa) (3*VEC_SIZE+1)(%[src]), %[v3]\n\t
		VEX_PRE_ARG3(PMIN, %[v0], %[v1], %[v4], movdqa)\n\t
		VEX_PRE_ARG3(PMIN, %[v2], %[v3], %[v5], movdqa)\n\t
# if VEC_SIZE < 64
		VEX_PRE(PMIN) %[v5], VEX_ARG(%[v4],) %[v4]\n\t
		XMM_SAVE_ARG(VEX_PRE(pxor) %[v5], VEX_ARG(%[v5],) %[v5])\n\t
		VEX_PRE(PCMPEQ) %[VZERO], VEX_ARG(%[v4],) %[v4]\n\t
		VEX_PRE(pmovmskb) %[v4], %k[zcx]\n\t
		addq	%[src], %[dst]\n\t
		testl	%k[zcx], %k[zcx]\n\t
		jne 	L_STPCPY(loop_4x_done)\n\t
# else
		VPTESTN %[v4], %[v4], %%k0\n\t
		VPTESTN %[v5], %[v5], %%k1\n\t
		addq	%[src], %[dst]\n\t
		KORTEST %%k0, %%k1\n\t
		jne 	L_STPCPY(loop_4x_done)\n\t
# endif
#endif
		.p2align 4,,13\n\t
L_STPCPY(loop_4x_vec):\n\t
		EVEX_PICK(vmovdqu64,movdqu) %[v0], (0*VEC_SIZE+1)(%[dst])\n\t
		EVEX_PICK(vmovdqu64,movdqu) %[v1], (1*VEC_SIZE+1)(%[dst])\n\t
		subq	$(-4*VEC_SIZE), %[src]\n\t
		EVEX_PICK(vmovdqu64,movdqu) %[v2], (2*VEC_SIZE+1)(%[dst])\n\t
		EVEX_PICK(vmovdqu64,movdqu) %[v3], (3*VEC_SIZE+1)(%[dst])\n\t
#if (CHAR_SIZE > 1 && VEC_SIZE == 16) || (CHAR_SIZE == 8 && VEC_SIZE == 32)
		VEX_PRE(movdqa) (0*VEC_SIZE+1)(%[src]), %[v1]\n\t
		VEX_PRE_ARG3(pxor, (1*VEC_SIZE+1)(%[src]), %[v1], %[v0], movdqa)\n\t
		VEX_PRE_ARG3(PCMPEQ, %[v0], %[v1], %[v4], movdqa)\n\t
		VEX_PRE(movdqa) (2*VEC_SIZE+1)(%[src]), %[v3]\n\t
		VEX_PRE_ARG3(pxor, (3*VEC_SIZE+1)(%[src]), %[v3], %[v2], movdqa)\n\t
		VEX_PRE(pxor) %[v0], VEX_ARG(%[v1],) %[v1]\n\t
		VEX_PRE_ARG3(PCMPEQ, %[v1], %[v0], %[v5], movdqa)\n\t
		VEX_PRE(pxor) %[v1], VEX_ARG(%[v0],) %[v0]\n\t
		VEX_PRE(por) %[v5], VEX_ARG(%[v4],) %[v4]\n\t
		VEX_PRE_ARG3(PCMPEQ, %[v2], %[v3], %[v5], movdqa)\n\t
		VEX_PRE(por) %[v5], VEX_ARG(%[v4],) %[v4]\n\t
		VEX_PRE(pxor) %[v2], VEX_ARG(%[v3],) %[v3]\n\t
		VEX_PRE_ARG3(PCMPEQ, %[v3], %[v2], %[v5], movdqa)\n\t
		VEX_PRE(pxor) %[v3], VEX_ARG(%[v2],) %[v2]\n\t
		VEX_PRE(por) %[v5], VEX_ARG(%[v4],) %[v4]\n\t
		VEX_PRE(pmovmskb) %[v4], %k[zcx]\n\t
		subq	$(-4*VEC_SIZE), %[dst]\n\t
		testl	%k[zcx], %k[zcx]\n\t
		je  	L_STPCPY(loop_4x_vec)\n\t
#else
		EVEX_PICK(vmovdqa64,movdqa) (0*VEC_SIZE+1)(%[src]), %[v0]\n\t
		EVEX_PICK(vmovdqa64,movdqa) (1*VEC_SIZE+1)(%[src]), %[v1]\n\t
		EVEX_PICK(vmovdqa64,movdqa) (2*VEC_SIZE+1)(%[src]), %[v2]\n\t
		EVEX_PICK(vmovdqa64,movdqa) (3*VEC_SIZE+1)(%[src]), %[v3]\n\t
		VEX_PRE_ARG3(PMIN, %[v0], %[v1], %[v4], movdqa)\n\t
		VEX_PRE_ARG3(PMIN, %[v2], %[v3], %[v5], movdqa)\n\t
# if VEC_SIZE < 64
		VEX_PRE(PMIN) %[v5], VEX_ARG(%[v4],) %[v4]\n\t
		XMM_SAVE_ARG(VEX_PRE(pxor) %[v5], VEX_ARG(%[v5],) %[v5])\n\t
		VEX_PRE(PCMPEQ) %[VZERO], VEX_ARG(%[v4],) %[v4]\n\t
		VEX_PRE(pmovmskb) %[v4], %k[zcx]\n\t
		subq	$(-4*VEC_SIZE), %[dst]\n\t
		testl	%k[zcx], %k[zcx]\n\t
		je  	L_STPCPY(loop_4x_vec)\n\t
# else
		VPTESTN %[v4], %[v4], %%k0\n\t
		VPTESTN %[v5], %[v5], %%k1\n\t
		subq	$(-4*VEC_SIZE), %[dst]\n\t
		KORTEST %%k0, %%k1\n\t
		je  	L_STPCPY(loop_4x_vec)\n\t
# endif
#endif
L_STPCPY(loop_4x_done):\n\t
#if (CHAR_SIZE > 1 && VEC_SIZE == 16) || (CHAR_SIZE == 8 && VEC_SIZE == 32)
		VEX_PRE(pxor) %x[VZERO], VEX_ARG(%x[VZERO],) %x[VZERO]\n\t
#endif
#if VEC_SIZE < 64
		VEX_PRE_ARG3(PCMPEQ, %[v0], %[VZERO], %[v4], movdqa)\n\t
		VEX_PRE(pmovmskb) %[v4], %k[zdx]\n\t
		testl	%k[zdx], %k[zdx]\n\t
		jne 	L_STPCPY(ret_vec_x1)\n\t
#else
		VPTESTN %[v0], %[v0], %%k0\n\t
		KMOV	%%k0, %KREG[zdx]\n\t
		test	%KREG[zdx], %KREG[zdx]\n\t
		jne 	L_STPCPY(ret_vec_x1)\n\t
#endif
		EVEX_PICK(vmovdqu64,movdqu) %[v0], (0*VEC_SIZE+1)(%[dst])\n\t
#if VEC_SIZE < 64
		VEX_PRE_ARG3(PCMPEQ, %[v1], %[VZERO], %[v4], movdqa)\n\t
		VEX_PRE(pmovmskb) %[v4], %k[zdx]\n\t
		testl	%k[zdx], %k[zdx]\n\t
		jne 	L_STPCPY(ret_vec_x2)\n\t
#else
		VPTESTN %[v1], %[v1], %%k0\n\t
		KMOV	%%k0, %KREG[zdx]\n\t
		test	%KREG[zdx], %KREG[zdx]\n\t
		jne 	L_STPCPY(ret_vec_x2)\n\t
#endif
		EVEX_PICK(vmovdqu64,movdqu) %[v1], (1*VEC_SIZE+1)(%[dst])\n\t
#if VEC_SIZE < 64
		VEX_PRE_ARG3(PCMPEQ, %[v2], %[VZERO], %[v4], movdqa)\n\t
		VEX_PRE(pmovmskb) %[v4], %k[zdx]\n\t
		testl	%k[zdx], %k[zdx]\n\t
		jne 	L_STPCPY(ret_vec_x3)\n\t
#else
		VPTESTN %[v2], %[v2], %%k0\n\t
		KMOV	%%k0, %KREG[zdx]\n\t
		test	%KREG[zdx], %KREG[zdx]\n\t
		jne 	L_STPCPY(ret_vec_x3)\n\t
		KMOV	%%k1, %KREG[zcx]\n\t
#endif
		EVEX_PICK(vmovdqu64,movdqu) %[v2], (2*VEC_SIZE+1)(%[dst])\n\t
L_STPCPY(ret_vec_x4):\n\t
#if VEC_SIZE < 64
		bsfl	%k[zcx], %k[zcx]\n\t
#else
		bsf 	%KREG[zcx], %KREG[zcx]\n\t
#endif
		EVEX_PICK(vmovdqu64,movdqu) ((3*VEC_SIZE+1)-(VEC_SIZE-CHAR_SIZE))(%[src],%[zcx] EVEX_ARG(,CHAR_SIZE)), %[v1]\n\t
		EVEX_PICK(vmovdqu64,movdqu) %[v1], ((3*VEC_SIZE+1)-(VEC_SIZE-CHAR_SIZE))(%[dst],%[zcx] EVEX_ARG(,CHAR_SIZE))\n\t
		leaq	(3*VEC_SIZE+1)(%[dst],%[zcx] EVEX_ARG(,CHAR_SIZE)), %%rax\n\t
		VZEROUPPER\n\t
		ret\n\t
		.p2align 4,,10\n\t
L_STPCPY(ret_vec_x1):\n\t
#if VEC_SIZE < 64
		bsfl	%k[zdx], %k[zdx]\n\t
#else
		bsf 	%KREG[zdx], %KREG[zdx]\n\t
#endif
		EVEX_PICK(vmovdqu64,movdqu) (1-(VEC_SIZE-CHAR_SIZE))(%[src],%[zdx] EVEX_ARG(,CHAR_SIZE)), %[v1]\n\t
		EVEX_PICK(vmovdqu64,movdqu) %[v1], (1-(VEC_SIZE-CHAR_SIZE))(%[dst],%[zdx] EVEX_ARG(,CHAR_SIZE))\n\t
		leaq	1(%[dst],%[zdx] EVEX_ARG(,CHAR_SIZE)), %%rax\n\t
		VZEROUPPER\n\t
		ret\n\t
		.p2align 4,,10\n\t
L_STPCPY(ret_vec_x2):\n\t
#if VEC_SIZE < 64
		bsfl	%k[zdx], %k[zdx]\n\t
#else
		bsf 	%KREG[zdx], %KREG[zdx]\n\t
#endif
		EVEX_PICK(vmovdqu64,movdqu) ((VEC_SIZE+1)-(VEC_SIZE-CHAR_SIZE))(%[src],%[zdx] EVEX_ARG(,CHAR_SIZE)), %[v1]\n\t
		EVEX_PICK(vmovdqu64,movdqu) %[v1], ((VEC_SIZE+1)-(VEC_SIZE-CHAR_SIZE))(%[dst],%[zdx] EVEX_ARG(,CHAR_SIZE))\n\t
		leaq	(1*VEC_SIZE+1)(%[dst],%[zdx] EVEX_ARG(,CHAR_SIZE)), %%rax\n\t
		VZEROUPPER\n\t
		ret\n\t
		.p2align 4,,10\n\t
L_STPCPY(ret_vec_x3):\n\t
#if VEC_SIZE < 64
		bsfl	%k[zdx], %k[zdx]\n\t
#else
		bsf 	%KREG[zdx], %KREG[zdx]\n\t
#endif
		EVEX_PICK(vmovdqu64,movdqu) ((2*VEC_SIZE+1)-(VEC_SIZE-CHAR_SIZE))(%[src],%[zdx] EVEX_ARG(,CHAR_SIZE)), %[v1]\n\t
		EVEX_PICK(vmovdqu64,movdqu) %[v1], ((2*VEC_SIZE+1)-(VEC_SIZE-CHAR_SIZE))(%[dst],%[zdx] EVEX_ARG(,CHAR_SIZE))\n\t
		leaq	(2*VEC_SIZE+1)(%[dst],%[zdx] EVEX_ARG(,CHAR_SIZE)), %%rax\n\t
		VZEROUPPER\n\t
		ret\n\t
		): [dst]"+r"(dst), [src]"+r"(src), [zdx]"=r"(zdx), [zcx]"=r"(zcx), [v0]"=x"(v0), [v1]"=x"(v1), [v2]"=x"(v2), [v3]"=x"(v3), [v4]"=x"(v4), [v5]"=x"(v5), [v6]"=x"(v6)
		:: "rax" EVEX_ARG(,"k0","k1"), "memory"
	);
}

