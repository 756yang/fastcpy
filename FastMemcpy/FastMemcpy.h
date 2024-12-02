#ifndef FAST_MEMCPY_H
#define FAST_MEMCPY_H

#include <stddef.h>

#define FAST_MEMCPY_PREFETCH

#ifdef __cplusplus
extern "C" {
#endif

void* memcpy_fast_sse(void* __restrict destination, const void* __restrict source, size_t size);

void* memcpy_fast_avx(void* __restrict destination, const void* __restrict source, size_t size);

#ifdef __cplusplus
}
#endif

#endif

