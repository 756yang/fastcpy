#ifndef MEMCPY_AMD64_MEMCPY_H
#define MEMCPY_AMD64_MEMCPY_H

#include <stddef.h>

#ifndef MEMCPY_AMD64_PREFIX
#define MEMCPY_AMD64_PREFIX custom_
#endif

//#cmakedefine MEMCPY_AMD64_PREFIX @MEMCPY_AMD64_PREFIX@

#define __MEMCPY_AMD64_CONCAT(X, Y) X ## Y
#define MEMCPY_AMD64_CONCAT(X, Y) __MEMCPY_AMD64_CONCAT(X, Y)
#define MEMCPY_AMD64_SYMBOL(X) MEMCPY_AMD64_CONCAT(MEMCPY_AMD64_PREFIX, X)

#ifdef __cplusplus
extern "C" {
#endif

void * MEMCPY_AMD64_SYMBOL(memcpy)(void *__restrict dst, const void *__restrict src, size_t size);

void MEMCPY_AMD64_SYMBOL(memcpy_set_erms_threshold)(size_t limit);

void MEMCPY_AMD64_SYMBOL(memcpy_set_nontemporal_threshold)(size_t limit);

void MEMCPY_AMD64_SYMBOL(memcpy_set_avx512)(bool status);

void MEMCPY_AMD64_SYMBOL(memcpy_set_erms)(bool status);

void MEMCPY_AMD64_SYMBOL(memcpy_set_ssse3)(bool status);

size_t MEMCPY_AMD64_SYMBOL(memcpy_get_erms_threshold)();

size_t MEMCPY_AMD64_SYMBOL(memcpy_get_nontemporal_threshold)();

bool MEMCPY_AMD64_SYMBOL(memcpy_get_avx512)();

bool MEMCPY_AMD64_SYMBOL(memcpy_get_erms)();

bool MEMCPY_AMD64_SYMBOL(memcpy_get_ssse3)();

#ifdef __cplusplus
}
#endif

#endif //MEMCPY_AMD64_MEMCPY_H
