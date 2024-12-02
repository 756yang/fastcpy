/*-*- mode:unix-assembly; indent-tabs-mode:t; tab-width:8; coding:utf-8     -*-│
│vi: set et ft=asm ts=8 tw=8 fenc=utf-8                                     :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2020 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ Permission to use, copy, modify, and/or distribute this software for         │
│ any purpose with or without fee is hereby granted, provided that the         │
│ above copyright notice and this permission notice appear in all copies.      │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL                │
│ WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED                │
│ WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE             │
│ AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL         │
│ DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR        │
│ PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER               │
│ TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR             │
│ PERFORMANCE OF THIS SOFTWARE.                                                │
╚─────────────────────────────────────────────────────────────────────────────*/
//	Rewrite to win64 call
//	Copies memory.
//
//	DEST and SRC must not overlap, unless DEST≤SRC.
//
//	@param	rcx is dest
//	@param	rdx is src
//	@param	r8 is number of bytes
//	@return	original rcx copied to rax
//	@mode	long
//	@asyncsignalsafe
memcpy_jart:	mov	%rcx,%rax
//	𝑠𝑙𝑖𝑑𝑒
	.align	16
#ifndef _WIN32
	.type	memcpy_jart,@function
	.size	memcpy_jart,.-memcpy_jart
#endif
	.globl	memcpy_jart

//	Copies memory w/ minimal impact ABI.
//
//	@param	rcx is dest
//	@param	rdx is src
//	@param	r8 is number of bytes
//	@clob	flags,r9,r10,xmm3,xmm4
//	@mode	long
MemCpy:	mov	$.Lmemcpytab.size,%r9d
	cmp	%r9,%r8
	cmovb	%r8,%r9
	jmp	*memcpytab(,%r9,8)
.Lanchorpoint:
.L16r:	cmp	$1024,%r8
	jae	.Lerms
.L16:	movdqu	-16(%rdx,%r8),%xmm4
	mov	$16,%r9
0:	add	$16,%r9
	movdqu	-32(%rdx,%r9),%xmm3
	movdqu	%xmm3,-32(%rcx,%r9)
	cmp	%r9,%r8
	ja	0b
	movdqu	%xmm4,-16(%rcx,%r8)
	pxor	%xmm4,%xmm4
	pxor	%xmm3,%xmm3
	ret
.L8:
	mov	(%rdx),%r9
	mov	-8(%rdx,%r8),%r10
	mov	%r9,(%rcx)
	mov	%r10,-8(%rcx,%r8)
1:
.L0:	ret
.L4:
	mov	(%rdx),%r9d
	mov	-4(%rdx,%r8),%r10d
	mov	%r9d,(%rcx)
	mov	%r10d,-4(%rcx,%r8)
	jmp	1b
.L3:
	mov	(%rdx),%r9w
	mov	-2(%rdx,%r8),%r10w
	mov	%r9w,(%rcx)
	mov	%r10w,-2(%rcx,%r8)
	jmp	1b
.L2:	mov	(%rdx),%r9w
	mov	%r9w,(%rcx)
	ret
.L1:	mov	(%rdx),%r9b
	mov	%r9b,(%rcx)
	ret
.Lerms:	cmp	$1024*1024,%r8
	ja	.Lnts
	push	%rdi
	push	%rsi
	mov %rcx,%rdi
	mov %rdx,%rsi
	mov	%r8,%rcx
	rep movsb
	pop	%rsi
	pop	%rdi
	ret
.Lnts:	movdqu	(%rdx),%xmm3
	movdqu	%xmm3,(%rcx)
	lea	16(%rcx),%r9
	and	$-16,%r9
	sub	%rcx,%r9
	add	%r9,%rcx
	add	%r9,%rdx
	sub	%r9,%r8
	mov	$16,%r9
0:	add	$16,%r9
	movdqu	-32(%rdx,%r9),%xmm3
	movntdq	%xmm3,-32(%rcx,%r9)
	cmp	%r9,%r8
	ja	0b
	sfence
	movdqu	-16(%rdx,%r8),%xmm3
	movdqu	%xmm3,-16(%rcx,%r8)
	pxor	%xmm3,%xmm3
	ret
#ifndef _WIN32
	.type	MemCpy,@function
	.size	MemCpy,.-MemCpy
#endif
	.globl	MemCpy

	.section .rdata
	.align	8
memcpytab:
	.quad	.L0
	.quad	.L1
	.quad	.L2
	.quad	.L3
	.rept	4
	.quad	.L4
	.endr
	.rept	8
	.quad	.L8
	.endr
	.rept	16
	.quad	.L16
	.endr
	.equ	.Lmemcpytab.size,(.-memcpytab)/8
	.quad	.L16r # SSE + ERMS + NTS
#ifndef _WIN32
	.type	memcpytab,@object
	.previous
#endif
