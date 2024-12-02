#include "memcpy-impl.h"

#include <emmintrin.h>
#include <immintrin.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

/* 各种 memcpy 实现全部使用内嵌汇编, 为避免编译器不确定优化导致性能不稳定 */

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

// AVX 版最小 memcpy
extern "C" void* memcpy_jart1(void *dest, const void *src, size_t size);

//extern "C" void* memcpy_jart(void* dst, const void* src, size_t size);
#if 1
__attribute__((optimize("align-functions=4096")))
void* memcpySSE2(void* __restrict destination, const void* __restrict source, size_t size) {
    uint8_t* dst = reinterpret_cast<uint8_t*>(destination);
    const uint8_t* src = reinterpret_cast<const uint8_t*>(source);

    if (xsun_likely(size > 16)) {
#if 1/* 这种方法更优 */
        __asm__ volatile(
                "vmovups    (%[d]), %%xmm0\n\t"
                "vmovups    %%xmm0, (%[s])\n\t"
                :
                : [d] "r"(dst), [s] "r"(src)
                : "xmm0", "memory");
        // align destination to 16 bytes boundary
        size_t padding = 16 - ((size_t)dst & 15);
        uint8_t* dst_end = (uint8_t*)(((size_t)dst + size) & -16);

        size -= (dst_end - dst);// 提前计算 size 避免循环後计算影响性能
        dst += padding;
        src += padding;

        while(dst != dst_end) {
            __asm__ volatile(
                    "vmovups    (%[d]), %%xmm0\n\t"
                    "vmovups    %%xmm0, (%[s])\n\t"
                    :
                    : [d] "r"(dst), [s] "r"(src)
                    : "xmm0", "memory");
            dst += 16;
            src += 16;
        }
#else/* 这种方式会导致编译器优化问题 */
        // align destination to 16 bytes boundary
        padding = 16 - (reinterpret_cast<size_t>(dst) & 15);

        __asm__ volatile(
                "vmovups    (%0), %%xmm0\n\t"
                "vmovups    %%xmm0, (%1)\n\t"
                :
                : "r"(dst), "r"(src)
                : "xmm0", "memory");

        dst += padding;
        src += padding;
        size -= padding;

        while(size >= 16) {// gcc v14.20 优化异常
            __asm__ volatile(
                    "vmovups    (%0), %%xmm0\n\t"
                    "vmovups    %%xmm0, (%1)\n\t"
                    :
                    : "r"(dst), "r"(src)
                    : "xmm0", "memory");
            size -= 16;// 这个会被优化掉, 出循环後再计算
            dst += 16;
            src += 16;
        }
        // 出循环後 size dst src 都会重新计算, 优化不佳
#endif
    }
    // small memory copy
    memcpy_jart1(dst, src, size);
    return destination;
}

__attribute__((optimize("align-functions=4096")))
void* memcpyAVX2(void* __restrict destination, const void* __restrict source, size_t size) {
    uint8_t* dst = reinterpret_cast<uint8_t*>(destination);
    const uint8_t* src = reinterpret_cast<const uint8_t*>(source);

    if (xsun_likely(size > 32)) {
        __asm__ volatile(
                "vmovups    (%[s]), %%ymm0\n\t"
                "vmovups    %%ymm0, (%[d])\n\t"
                :
                : [d] "r"(dst), [s] "r"(src)
                : "ymm0", "memory");
        // align destination to 32 bytes boundary
        size_t padding = 32 - ((size_t)dst & 31);
        uint8_t* dst_end = (uint8_t*)(((size_t)dst + size) & -32);

        size -= (dst_end - dst);// 提前计算 size 避免循环後计算影响性能
        dst += padding;
        src += padding;

        while(dst != dst_end) {
            __asm__ volatile(
                    "vmovups    (%[s]), %%ymm0\n\t"
                    "vmovups    %%ymm0, (%[d])\n\t"
                    :
                    : [d] "r"(dst), [s] "r"(src)
                    : "ymm0", "memory");
            dst += 32;
            src += 32;
        }
    }
    // small memory copy
    memcpy_jart1(dst, src, size);
    return destination;
}
#endif
__attribute__((optimize("align-functions=4096")))
void* memcpySSE2Unrolled2(void* __restrict destination, const void* __restrict source, size_t size) {
    uint8_t* dst = reinterpret_cast<uint8_t*>(destination);
    const uint8_t* src = reinterpret_cast<const uint8_t*>(source);

    if (xsun_likely(size > 32)) {
        __asm__ volatile(
                "vmovups    (%[d]), %%xmm0\n\t"
                "vmovups    %%xmm0, (%[s])\n\t"
                :
                : [d] "r"(dst), [s] "r"(src)
                : "xmm0", "memory");
        // align destination to 16 bytes boundary
        size_t padding = 16 - ((size_t)dst & 15);
        uint8_t* dst_end = dst + size - 32;

        dst += padding;
        src += padding;

        while(dst_end > dst) {
            __asm__ volatile(
                    "vmovups    (%[s]), %%xmm0\n\t"
                    "vmovups    0x10(%[s]), %%xmm1\n\t"
                    "add        $0x20, %[s]\n\t"
                    "vmovups    %%xmm0, (%[d])\n\t"
                    "vmovups    %%xmm1, 0x10(%[d])\n\t"
                    "add        $0x20, %[d]\n\t"
                    : [d] "+r"(dst), [s] "+r"(src)
                    :
                    : "xmm0", "xmm1", "memory");
        }
        size = 32 - (dst - dst_end);
    }
    // small memory copy
    memcpy_jart1(dst, src, size);
    return destination;
}

__attribute__((optimize("align-functions=4096")))
void* memcpySSE2Unrolled4(void* __restrict destination, const void* __restrict source, size_t size) {
    uint8_t* dst = reinterpret_cast<uint8_t*>(destination);
    const uint8_t* src = reinterpret_cast<const uint8_t*>(source);

    if (xsun_likely(size > 64)) {
        __asm__ volatile(
                "vmovups    (%[d]), %%xmm0\n\t"
                "vmovups    %%xmm0, (%[s])\n\t"
                :
                : [d] "r"(dst), [s] "r"(src)
                : "xmm0", "memory");
        // align destination to 16 bytes boundary
        size_t padding = 16 - ((size_t)dst & 15);
        uint8_t* dst_end = dst + size - 64;

        dst += padding;
        src += padding;

        while(dst_end > dst) {
            __asm__ volatile(
                    "vmovups    (%[s]), %%xmm0\n\t"
                    "vmovups    0x10(%[s]), %%xmm1\n\t"
                    "vmovups    0x20(%[s]), %%xmm2\n\t"
                    "vmovups    0x30(%[s]), %%xmm3\n\t"
                    "add        $0x40, %[s]\n\t"
                    "vmovups    %%xmm0, (%[d])\n\t"
                    "vmovups    %%xmm1, 0x10(%[d])\n\t"
                    "vmovups    %%xmm2, 0x20(%[d])\n\t"
                    "vmovups    %%xmm3, 0x30(%[d])\n\t"
                    "add        $0x40, %[d]\n\t"
                    : [d] "+r"(dst), [s] "+r"(src)
                    :
                    : "xmm0", "xmm1", "xmm2", "xmm3", "memory");
        }
        size = 64 - (dst - dst_end);
    }
    // small memory copy
    memcpy_jart1(dst, src, size);
    return destination;
}

__attribute__((optimize("align-functions=4096")))
void* memcpySSE2Unrolled8(void* __restrict destination, const void* __restrict source, size_t size) {
    uint8_t* dst = reinterpret_cast<uint8_t*>(destination);
    const uint8_t* src = reinterpret_cast<const uint8_t*>(source);

    if (xsun_likely(size > 128)) {
        __asm__ volatile(
                "vmovups    (%[d]), %%xmm0\n\t"
                "vmovups    %%xmm0, (%[s])\n\t"
                :
                : [d] "r"(dst), [s] "r"(src)
                : "xmm0", "memory");
        // align destination to 16 bytes boundary
        size_t padding = 16 - ((size_t)dst & 15);
        uint8_t* dst_end = dst + size - 128;

        dst += padding;
        src += padding;

        while(dst_end > dst) {
            __asm__ volatile(
                    "vmovups    (%[s]), %%xmm0\n\t"
                    "vmovups    0x10(%[s]), %%xmm1\n\t"
                    "vmovups    0x20(%[s]), %%xmm2\n\t"
                    "vmovups    0x30(%[s]), %%xmm3\n\t"
                    "vmovups    0x40(%[s]), %%xmm4\n\t"
                    "vmovups    0x50(%[s]), %%xmm5\n\t"
                    "vmovups    0x60(%[s]), %%xmm6\n\t"
                    "vmovups    0x70(%[s]), %%xmm7\n\t"
                    "add        $0x80, %[s]\n\t"
                    "vmovups    %%xmm0, (%[d])\n\t"
                    "vmovups    %%xmm1, 0x10(%[d])\n\t"
                    "vmovups    %%xmm2, 0x20(%[d])\n\t"
                    "vmovups    %%xmm3, 0x30(%[d])\n\t"
                    "vmovups    %%xmm4, 0x40(%[d])\n\t"
                    "vmovups    %%xmm5, 0x50(%[d])\n\t"
                    "vmovups    %%xmm6, 0x60(%[d])\n\t"
                    "vmovups    %%xmm7, 0x70(%[d])\n\t"
                    "add        $0x80, %[d]\n\t"
                    : [d] "+r"(dst), [s] "+r"(src)
                    :
                    : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "memory");
        }
        size = 128 - (dst - dst_end);
    }
    // small memory copy
    memcpy_jart1(dst, src, size);
    return destination;
}

__attribute__((optimize("align-functions=4096")))
void* memcpyAVXUnrolled2(void* __restrict destination, const void* __restrict source, size_t size) {
    uint8_t* dst = reinterpret_cast<uint8_t*>(destination);
    const uint8_t* src = reinterpret_cast<const uint8_t*>(source);

    if (xsun_likely(size > 64)) {
        __asm__ volatile(
                "vmovups    (%[s]), %%ymm0\n\t"
                "vmovups    %%ymm0, (%[d])\n\t"
                :
                : [d] "r"(dst), [s] "r"(src)
                : "ymm0", "memory");
        // align destination to 32 bytes boundary
        size_t padding = 32 - ((size_t)dst & 31);
        uint8_t* dst_end = dst + size - 64;

        dst += padding;
        src += padding;

        while(dst_end > dst) {
            __asm__ volatile(
                    "vmovups    (%[s]), %%ymm0\n\t"
                    "vmovups    0x20(%[s]), %%ymm1\n\t"
                    "add        $0x40, %[s]\n\t"
                    "vmovups    %%ymm0, (%[d])\n\t"
                    "vmovups    %%ymm1, 0x20(%[d])\n\t"
                    "add        $0x40, %[d]\n\t"
                    : [d] "+r"(dst), [s] "+r"(src)
                    :
                    : "ymm0", "ymm1", "memory");
        }
        size = 64 - (dst - dst_end);
    }
    // small memory copy
    memcpy_jart1(dst, src, size);
    return destination;
}

__attribute__((optimize("align-functions=4096")))
void* memcpyAVXUnrolled4(void* __restrict destination, const void* __restrict source, size_t size) {
    uint8_t* dst = reinterpret_cast<uint8_t*>(destination);
    const uint8_t* src = reinterpret_cast<const uint8_t*>(source);

    if (xsun_likely(size > 128)) {
        __asm__ volatile(
                "vmovups    (%[s]), %%ymm0\n\t"
                "vmovups    %%ymm0, (%[d])\n\t"
                :
                : [d] "r"(dst), [s] "r"(src)
                : "ymm0", "memory");
        // align destination to 32 bytes boundary
        size_t padding = 32 - ((size_t)dst & 31);
        uint8_t* dst_end = dst + size - 128;

        dst += padding;
        src += padding;

        while(dst_end > dst) {
            __asm__ volatile(
                    "vmovups    (%[s]), %%ymm0\n\t"
                    "vmovups    0x20(%[s]), %%ymm1\n\t"
                    "vmovups    0x40(%[s]), %%ymm2\n\t"
                    "vmovups    0x60(%[s]), %%ymm3\n\t"
                    "add        $0x80, %[s]\n\t"
                    "vmovups    %%ymm0, (%[d])\n\t"
                    "vmovups    %%ymm1, 0x20(%[d])\n\t"
                    "vmovups    %%ymm2, 0x40(%[d])\n\t"
                    "vmovups    %%ymm3, 0x60(%[d])\n\t"
                    "add        $0x80, %[d]\n\t"
                    : [d] "+r"(dst), [s] "+r"(src)
                    :
                    : "ymm0", "ymm1", "ymm2", "ymm3", "memory");
        }
        size = 128 - (dst - dst_end);
    }
    // small memory copy
    memcpy_jart1(dst, src, size);
    return destination;
}

__attribute__((optimize("align-functions=4096")))
void* memcpyAVXUnrolled8(void* __restrict destination, const void* __restrict source, size_t size) {
    uint8_t* dst = reinterpret_cast<uint8_t*>(destination);
    const uint8_t* src = reinterpret_cast<const uint8_t*>(source);

    if (xsun_likely(size > 256)) {
        __asm__ volatile(
                "vmovups    (%[s]), %%ymm0\n\t"
                "vmovups    %%ymm0, (%[d])\n\t"
                :
                : [d] "r"(dst), [s] "r"(src)
                : "ymm0", "memory");
        // align destination to 32 bytes boundary
        size_t padding = 32 - ((size_t)dst & 31);
        uint8_t* dst_end = dst + size - 256;

        dst += padding;
        src += padding;

        while(dst_end > dst) {
            __asm__ volatile(
                    "vmovups    (%[s]), %%ymm0\n\t"
                    "vmovups    0x20(%[s]), %%ymm1\n\t"
                    "vmovups    0x40(%[s]), %%ymm2\n\t"
                    "vmovups    0x60(%[s]), %%ymm3\n\t"
                    "vmovups    0x80(%[s]), %%ymm4\n\t"
                    "vmovups    0xa0(%[s]), %%ymm5\n\t"
                    "vmovups    0xc0(%[s]), %%ymm6\n\t"
                    "vmovups    0xe0(%[s]), %%ymm7\n\t"
                    "add        $0x100, %[s]\n\t"
                    "vmovaps    %%ymm0, (%[d])\n\t"
                    "vmovaps    %%ymm1, 0x20(%[d])\n\t"
                    "vmovaps    %%ymm2, 0x40(%[d])\n\t"
                    "vmovaps    %%ymm3, 0x60(%[d])\n\t"
                    "vmovaps    %%ymm4, 0x80(%[d])\n\t"
                    "vmovaps    %%ymm5, 0xa0(%[d])\n\t"
                    "vmovaps    %%ymm6, 0xc0(%[d])\n\t"
                    "vmovaps    %%ymm7, 0xe0(%[d])\n\t"
                    "add        $0x100, %[d]\n\t"
                    : [d] "+r"(dst), [s] "+r"(src)
                    :
                    : "ymm0", "ymm1", "ymm2", "ymm3", "ymm4", "ymm5", "ymm6", "ymm7", "memory");
        }
        size = 256 - (dst - dst_end);
    }
    // small memory copy
    memcpy_jart1(dst, src, size);
    return destination;
}


__attribute__((optimize("align-functions=4096")))
void* memcpy_my(void* __restrict destination, const void* __restrict source, size_t size) {
    uint8_t* dst = reinterpret_cast<uint8_t*>(destination);
    const uint8_t* src = reinterpret_cast<const uint8_t*>(source);
    uint8_t* ret = dst;

tail:
    if (size <= 16) {
        if (size >= 8) {
            __builtin_memcpy(dst + size - 8, src + size - 8, 8);
            __builtin_memcpy(dst, src, 8);
        } else if (size >= 4) {
            __builtin_memcpy(dst + size - 4, src + size - 4, 4);
            __builtin_memcpy(dst, src, 4);
        } else if (size >= 2) {
            __builtin_memcpy(dst + size - 2, src + size - 2, 2);
            __builtin_memcpy(dst, src, 2);
        } else if (size >= 1) {
            *dst = *src;
        }
    } else if (size <= 128) {// 这里会被循环展开
        __m128 xmm3, xmm4 = _mm_loadu_ps((float*)(src + size - 16));
        size_t sz_end = 16;
        do {
            sz_end += 16;
            xmm3 = _mm_loadu_ps((float*)(src + sz_end - 32));
            _mm_storeu_ps((float*)(dst + sz_end - 32), xmm3);
        } while(size > sz_end);
        _mm_storeu_ps((float*)(dst + size - 16), xmm4);
    } else {
        __asm__ volatile(
                "vmovups    (%[d]), %%xmm0\n\t"
                "vmovups    %%xmm0, (%[s])\n\t"
                :
                : [d] "r"(dst), [s] "r"(src)
                : "xmm0", "memory");
        // align destination to 16 bytes boundary
        size_t padding = 16 - ((size_t)dst & 15);
        uint8_t* dst_end = dst + size - 128;

        dst += padding;
        src += padding;

        while(dst_end > dst) {
            __asm__ volatile(
                    "vmovups    (%[s]), %%xmm0\n\t"
                    "vmovups    0x10(%[s]), %%xmm1\n\t"
                    "vmovups    0x20(%[s]), %%xmm2\n\t"
                    "vmovups    0x30(%[s]), %%xmm3\n\t"
                    "vmovups    0x40(%[s]), %%xmm4\n\t"
                    "vmovups    0x50(%[s]), %%xmm5\n\t"
                    "vmovups    0x60(%[s]), %%xmm6\n\t"
                    "vmovups    0x70(%[s]), %%xmm7\n\t"
                    "add        $0x80, %[s]\n\t"
                    "vmovups    %%xmm0, (%[d])\n\t"
                    "vmovups    %%xmm1, 0x10(%[d])\n\t"
                    "vmovups    %%xmm2, 0x20(%[d])\n\t"
                    "vmovups    %%xmm3, 0x30(%[d])\n\t"
                    "vmovups    %%xmm4, 0x40(%[d])\n\t"
                    "vmovups    %%xmm5, 0x50(%[d])\n\t"
                    "vmovups    %%xmm6, 0x60(%[d])\n\t"
                    "vmovups    %%xmm7, 0x70(%[d])\n\t"
                    "add        $0x80, %[d]\n\t"
                    : [d] "+r"(dst), [s] "+r"(src)
                    :
                    : "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "memory");
        }
        size = 128 - (dst - dst_end);
        goto tail;
    }

    return ret;
}

__attribute__((optimize("align-functions=4096")))
void* memcpy_my2(void* __restrict destination, const void* __restrict source, size_t size) {
    uint8_t* dst = reinterpret_cast<uint8_t*>(destination);
    const uint8_t* src = reinterpret_cast<const uint8_t*>(source);
    uint8_t* ret = dst;

tail:
    if (size <= 32) {
        if (size >= 16) {
            __builtin_memcpy(dst + size - 16, src + size - 16, 16);
            __builtin_memcpy(dst, src, 16);
        } else if (size >= 8) {
            __builtin_memcpy(dst + size - 8, src + size - 8, 8);
            __builtin_memcpy(dst, src, 8);
        } else if (size >= 4) {
            __builtin_memcpy(dst + size - 4, src + size - 4, 4);
            __builtin_memcpy(dst, src, 4);
        } else if (size >= 2) {
            __builtin_memcpy(dst + size - 2, src + size - 2, 2);
            __builtin_memcpy(dst, src, 2);
        } else if (size >= 1) {
            *dst = *src;
        }
    } else if (size <= 256) {// 这里会被循环展开
        __m256 ymm3, ymm4 = _mm256_loadu_ps((float*)(src + size - 32));
        size_t sz_end = 32;
        do {
            sz_end += 32;
            ymm3 = _mm256_loadu_ps((float*)(src + sz_end - 64));
            _mm256_storeu_ps((float*)(dst + sz_end - 64), ymm3);
        } while(size > sz_end);
        _mm256_storeu_ps((float*)(dst + size - 32), ymm4);
    } else {
        __asm__ volatile(
                "vmovups    (%[s]), %%ymm0\n\t"
                "vmovups    %%ymm0, (%[d])\n\t"
                :
                : [d] "r"(dst), [s] "r"(src)
                : "ymm0", "memory");
        // align destination to 32 bytes boundary
        size_t padding = 32 - ((size_t)dst & 31);
        uint8_t* dst_end = dst + size - 256;

        dst += padding;
        src += padding;

        while(dst_end > dst) {
            __asm__ volatile(
                    "vmovups    (%[s]), %%ymm0\n\t"
                    "vmovups    0x20(%[s]), %%ymm1\n\t"
                    "vmovups    0x40(%[s]), %%ymm2\n\t"
                    "vmovups    0x60(%[s]), %%ymm3\n\t"
                    "vmovups    0x80(%[s]), %%ymm4\n\t"
                    "vmovups    0xa0(%[s]), %%ymm5\n\t"
                    "vmovups    0xc0(%[s]), %%ymm6\n\t"
                    "vmovups    0xe0(%[s]), %%ymm7\n\t"
                    "add        $0x100, %[s]\n\t"
                    "vmovaps    %%ymm0, (%[d])\n\t"
                    "vmovaps    %%ymm1, 0x20(%[d])\n\t"
                    "vmovaps    %%ymm2, 0x40(%[d])\n\t"
                    "vmovaps    %%ymm3, 0x60(%[d])\n\t"
                    "vmovaps    %%ymm4, 0x80(%[d])\n\t"
                    "vmovaps    %%ymm5, 0xa0(%[d])\n\t"
                    "vmovaps    %%ymm6, 0xc0(%[d])\n\t"
                    "vmovaps    %%ymm7, 0xe0(%[d])\n\t"
                    "add        $0x100, %[d]\n\t"
                    : [d] "+r"(dst), [s] "+r"(src)
                    :
                    : "ymm0", "ymm1", "ymm2", "ymm3", "ymm4", "ymm5", "ymm6", "ymm7", "memory");
        }
        size = 256 - (dst - dst_end);
        goto tail;
    }

    return ret;
}

