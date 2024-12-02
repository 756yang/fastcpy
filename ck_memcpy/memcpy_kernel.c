#include <stddef.h>
#include <stdint.h>

// Win64 易失寄存器有: rcx, rdx, r8, r9, rax, r10, r11
// Amd64 易失寄存器有: rdi, rsi, rdx, rcx, r8, r9, rax, r10, r11

/* Linux 内核版本的 memcpy 实现 */
__attribute__((optimize("align-functions=4096"),naked))
void* memcpy_kernel(void *dest, const void *src, size_t size) {
	// r9, rax, r10, r11
	asm volatile(
		"pushq %[d]\n\t"
		//movq %[d], %rax

		"cmpq $0x20, %[sz]\n\t"
		"jb .Lhandle_tail\n\t"

		/*
		 * We check whether memory false dependence could occur,
		 * then jump to corresponding copy mode.
		 */
		"cmpb  %b[d], %b[s]\n\t"
		"jl .Lcopy_backward\n\t"
		"subq $0x20, %[sz]\n\n"
".Lcopy_forward_loop:\n\t"
		"subq $0x20, %[sz]\n\t"

		/*
		 * Move in blocks of 4x8 bytes:
		 */
		"movq 0*8(%[s]), %%rax\n\t"
		"movq 1*8(%[s]), %%r9\n\t"
		"movq 2*8(%[s]), %%r10\n\t"
		"movq 3*8(%[s]), %%r11\n\t"
		"leaq 4*8(%[s]), %[s]\n\t"

		"movq %%rax, 0*8(%[d])\n\t"
		"movq %%r9, 1*8(%[d])\n\t"
		"movq %%r10, 2*8(%[d])\n\t"
		"movq %%r11, 3*8(%[d])\n\t"
		"leaq 4*8(%[d]), %[d]\n\t"
		"jae  .Lcopy_forward_loop\n\t"
		"addl $0x20, %k[sz]\n\t"
		"jmp  .Lhandle_tail\n\n"

".Lcopy_backward:\n\t"
		/*
		 * Calculate copy position to tail.
		 */
		"addq %[sz], %[s]\n\t"
		"addq %[sz], %[d]\n\t"
		"subq $0x20, %[sz]\n\t"
		/*
		 * At most 3 ALU operations in one cycle,
		 * so append NOPS in the same 16 bytes trunk.
		 */
		".p2align 4\n\n"
".Lcopy_backward_loop:\n\t"
		"subq $0x20, %[sz]\n\t"
		"movq -1*8(%[s]), %%rax\n\t"
		"movq -2*8(%[s]), %%r9\n\t"
		"movq -3*8(%[s]), %%r10\n\t"
		"movq -4*8(%[s]), %%r11\n\t"
		"leaq -4*8(%[s]), %[s]\n\t"
		"movq %%rax, -1*8(%[d])\n\t"
		"movq %%r9, -2*8(%[d])\n\t"
		"movq %%r10, -3*8(%[d])\n\t"
		"movq %%r11, -4*8(%[d])\n\t"
		"leaq -4*8(%[d]), %[d]\n\t"
		"jae  .Lcopy_backward_loop\n\t"

		/*
		 * Calculate copy position to head.
		 */
		"addl $0x20, %k[sz]\n\t"
		"subq %[sz], %[s]\n\t"
		"subq %[sz], %[d]\n\n"
".Lhandle_tail:\n\t"
		"cmpl $16, %k[sz]\n\t"
		"jb   .Lless_16bytes\n\t"

		/*
		 * Move data from 16 bytes to 31 bytes.
		 */
		"movq 0*8(%[s]), %%rax\n\t"
		"movq 1*8(%[s]), %%r9\n\t"
		"movq -2*8(%[s], %[sz]), %%r10\n\t"
		"movq -1*8(%[s], %[sz]), %%r11\n\t"
		"movq %%rax, 0*8(%[d])\n\t"
		"movq %%r9, 1*8(%[d])\n\t"
		"movq %%r10, -2*8(%[d], %[sz])\n\t"
		"movq %%r11, -1*8(%[d], %[sz])\n\t"
		"popq %%rax\n\t"
		"retq\n\t"
		".p2align 4\n\n"
".Lless_16bytes:\n\t"
		"cmpl $8, %k[sz]\n\t"
		"jb   .Lless_8bytes\n\t"
		/*
		 * Move data from 8 bytes to 15 bytes.
		 */
		"movq 0*8(%[s]), %%rax\n\t"
		"movq -1*8(%[s], %[sz]), %%r9\n\t"
		"movq %%rax, 0*8(%[d])\n\t"
		"movq %%r9, -1*8(%[d], %[sz])\n\t"
		"popq %%rax\n\t"
		"retq\n\t"
		".p2align 4\n\n"
".Lless_8bytes:\n\t"
		"cmpl $4, %k[sz]\n\t"
		"jb   .Lless_3bytes\n\t"

		/*
		 * Move data from 4 bytes to 7 bytes.
		 */
		"movl (%[s]), %%r9d\n\t"
		"movl -4(%[s], %[sz]), %%eax\n\t"
		"movl %%r9d, (%[d])\n\t"
		"movl %%eax, -4(%[d], %[sz])\n\t"
		"popq %%rax\n\t"
		"retq\n\t"
		".p2align 4\n\n"
".Lless_3bytes:\n\t"
		"subl $1, %k[sz]\n\t"
		"jb .Lend\n\t"
		/*
		 * Move data from 1 bytes to 3 bytes.
		 */
		"movzbq (%[s]), %%r9\n\t"
		"jz .Lstore_1byte\n\t"
		"movzwl -1(%[s],%[sz]), %%eax\n\t"// 这种更快
		"movw %%ax, -1(%[d],%[sz])\n\n"
		/*"movzbq 1(%[s]), %%rax\n\t"
		"movzbq (%[s], %[sz]), %%r10\n\t"
		"movb %%al, 1(%[d])\n\t"
		"movb %%r10b, (%[d], %[sz])\n\n"*/
".Lstore_1byte:\n\t"
		"movb %%r9b, (%[d])\n\n"

".Lend:\n\t"
		"popq %%rax\n\t"
		"retq\n\t"
		: [d] "+r"(dest), [s] "+r"(src), [sz] "+r"(size)
		:
		: "rax", "r9", "r10", "r11"
	);
}

