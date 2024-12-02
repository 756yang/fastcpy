#include <stdint.h>
#include <stddef.h>
#include <stdio.h>


#if defined(_WIN32) || defined(__CYGWIN__)
# define XMM_NEED_SAVE
# define X64_CALL __attribute__((ms_abi))
#elif defined(__unix__) || defined(__linux__)
# define X64_CALL __attribute__((sysv_abi))
#else
#error Unknown environment!
#endif

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

#define __ASM_OPT(...) #__VA_ARGS__
#define ASM_OPT(...) __ASM_OPT(__VA_ARGS__)

#ifdef __AVX__
# define VZEROUPPER vzeroupper
# define VZEROUPPER_ASM asm volatile("vzeroupper"::)
# define VEX_PRE(name) v##name
# define VEX_ARG(...) __VA_ARGS__
#else
# define VZEROUPPER
# define VZEROUPPER_ASM
# define VEX_PRE(name) name
# define VEX_ARG(...)
#endif

size_t fastcpy_non_temporal_threshold = 0xffffffffffffffffLL;

uint32_t g_threads = 0;
uint32_t g_l3cache = 0;
uint32_t g_cores = 0;

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

/**
 * 通过 cpuid 指令查询 L2 L3 cache 与 logical CPUs 设置 fastcpy_non_temporal_threshold
 *  cpuid 参考: https://www.sandpile.org/x86/cpuid.htm
 *  glibc 参考: https://sourceware.org/git/?p=glibc.git;a=blob;f=sysdeps/x86/cacheinfo.c;hb=d3c57027470b78dba79c6d931e4e409b1fecfc80
 *  libcpuid 参考: https://libcpuid.sourceforge.net/documentation.html
 */
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
					g_l3cache = l3cache;
					g_threads = threads;
					asm volatile(ASM_OPT(
						VEX_PRE(cvtsi2ss) %[td], VEX_ARG(%%xmm1,) %%xmm1\n\t
						VEX_PRE(cvtsi2ss) %[l3], VEX_ARG(%%xmm2,) %%xmm2\n\t
						VEX_PRE(rsqrtss) %%xmm1, VEX_ARG(%%xmm1,) %%xmm1\n\t
						VEX_PRE(mulss) %%xmm2, VEX_ARG(%%xmm1,) %%xmm1\n\t
						VEX_PRE(cvtss2si) %%xmm1, %k[th]\n\t
						):[th]"=r"(threads): [l3]"r"(l3cache), [td]"r"(threads)
						: "xmm1","xmm2"
					);
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
				g_cores = cores;
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


int main() {
	get_non_temporal_threshold(NULL, NULL, 0);
	printf("%u, %u, %u, fastcpy_non_temporal_threshold = %llu\n", g_l3cache, g_cores, g_threads, fastcpy_non_temporal_threshold);
	return 0;
}


