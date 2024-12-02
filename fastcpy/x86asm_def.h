#ifndef __X86_ASM_DEFINED_H__
#define __X86_ASM_DEFINED_H__

#ifndef VEC_SIZE
# if defined(__AVX512F__)
#   define VEC_SIZE 64
# elif defined(__AVX__)
#   define VEC_SIZE 32
# elif defined(__SSE__)
#   define VEC_SIZE 16
# else
#   define VEC_SIZE 0
# endif
#elif VEC_SIZE != 0
# if VEC_SIZE != 16 && VEC_SIZE != 32 && VEC_SIZE != 64
#   error Unsupported VEC_SIZE!
# endif
# if !defined(__AVX512F__) && !defined(__AVX__) && !defined(__SSE__)
#   error Unsupported Platform!
# endif
#endif

#ifndef __STR_CAT
# define __STR_CAT(...) #__VA_ARGS__
#endif

#ifndef __TC_CAT
# define __TC_CAT(x,y) x##y
#endif

#ifdef __AVX__
# define VZEROUPPER vzeroupper
# define VZEROUPPER_ASM asm volatile("vzeroupper"::)
# define VEX_PRE(name) __TC_CAT(v,name)
# define VEX_ARG(...) __VA_ARGS__
# define NOVEX_ARG(...)
# define VEX_PRE_ARG3(name, src, mid, dst, mov) __TC_CAT(v,name) src, mid, dst
#else
# define VZEROUPPER
# define VZEROUPPER_ASM
# define VEX_PRE(name) name
# define VEX_ARG(...)
# define NOVEX_ARG(...) __VA_ARGS__
# define VEX_PRE_ARG3(name, src, mid, dst, mov) mov mid, dst; name src, dst
#endif

#ifdef __AVX512F__
# define EVEX_ARG(...) __VA_ARGS__
# define NOEVEX_ARG(...)
#else
# define EVEX_ARG(...)
# define NOEVEX_ARG(...) __VA_ARGS__
#endif

/* 用于适配 SSE, AVX, AVX512 的指令名称 */
#define EVEX_PICK(evex, norm) EVEX_ARG(evex) NOEVEX_ARG(VEX_PRE(norm))


#if VEC_SIZE == 16
# define SIMD_OPT(name) _mm_##name
# define VMM_TYPE(...) __m128##__VA_ARGS__
# define VMM_NAME "xmm"
#elif VEC_SIZE == 32
# define SIMD_OPT(name) _mm256_##name
# define VMM_TYPE(...) __m256##__VA_ARGS__
# define VMM_NAME "ymm"
#elif  VEC_SIZE == 64
# define SIMD_OPT(name) _mm512_##name
# define VMM_TYPE(...) __m512##__VA_ARGS__
# define VMM_NAME "zmm"
#endif

#if defined(__LP64__) || defined(_WIN64) || (defined(__x86_64__) && !defined(__ILP32__)) || defined(_M_X64) || defined(__ia64) || defined (_M_IA64) || defined(__aarch64__) || defined(__powerpc64__)
# define REG_SYS(x) %%r##x
#else/* 32 bits x86 */
# define REG_SYS(x) %%e##x
#endif

#define ASM_OPT(...) __STR_CAT(__VA_ARGS__)

/* 使用此修改栈指针请必要看编译器生成的汇编代码是否符合 */
#define ADD_STACK_ASM(x) asm volatile(ASM_OPT(add $(x), REG_SYS(sp))::)


#if defined(_WIN32) || defined(__CYGWIN__)
# define XMM_NEED_SAVE
# define XMM_SAVE_ARG(...) __VA_ARGS__
# define XMM_NOSAVE_ARG(...)
# define X64_CALL __attribute__((ms_abi))
#elif defined(__unix__) || defined(__linux__)
# define XMM_SAVE_ARG(...)
# define XMM_NOSAVE_ARG(...) __VA_ARGS__
# define X64_CALL __attribute__((sysv_abi))
#else
#error Unknown environment!
#endif

#if VEC_SIZE != 0/* start of VEC_SIZE != 0 */

#define PUSH_XMM_ASM(xmm, offset) \
	asm volatile(ASM_OPT(VEX_PRE(movups) %%xmm, %c[i](REG_SYS(sp)))::[i]"i"(offset))
#define POP_XMM_ASM(xmm, offset) \
	asm volatile(ASM_OPT(VEX_PRE(movups) %c[i](REG_SYS(sp)), %%xmm)::[i]"i"(offset))
#define LOAD_ASM1(name, vec, offset, base) asm volatile(\
	ASM_OPT(VEX_PRE(name) %c[i](%[b]), %[v])\
	: [v] "=x"(vec) : [b] "r"(base), [i] "i"(offset))
#define LOAD_ASM2(name, vec, offset, base, dir, index) asm volatile(\
	ASM_OPT(VEX_PRE(name) %c[i](%[b],%[a],%c[d]), %[v])\
	: [v] "=x"(vec) : [b] "r"(base), [a] "r"(index), [d] "i"(dir), [i] "i"(offset))
#define STORE_ASM1(name, vec, offset, base) asm volatile(\
	ASM_OPT(VEX_PRE(name) %[v], %c[i](%[b]))\
	:: [v] "x"(vec), [b] "r"(base), [i] "i"(offset) :"memory")
#define STORE_ASM2(name, vec, offset, base, dir, index) asm volatile(\
	ASM_OPT(VEX_PRE(name) %[v], %c[i](%[b],%[a],%c[d]))\
	:: [v] "x"(vec), [b] "r"(base), [a] "r"(index), [d] "i"(dir), [i] "i"(offset) :"memory")

#define SIMD_LOAD1(name, float, vec, offset, base) \
	vec = SIMD_OPT(name)((float*)((char*)(base) + (offset)))
#define SIMD_LOAD2(name, float, vec, offset, base, dir, index) \
	vec = SIMD_OPT(name)((float*)((char*)(base) + (dir) * (index) + (offset)))
#define SIMD_STORE1(name, float, vec, offset, base) \
	SIMD_OPT(name)((float*)((char*)(base) + (offset)), vec)
#define SIMD_STORE2(name, float, vec, offset, base, dir, index) \
	SIMD_OPT(name)((float*)((char*)(base) + (dir) * (index) + (offset)), vec)

#endif/* end of VEC_SIZE != 0 */

#endif

