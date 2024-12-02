#ifndef LIB_FAST_CPY_H
#define LIB_FAST_CPY_H

#ifndef LIBRARY_GLOBAL_H
#define LIBRARY_GLOBAL_H

#if defined(_MSC_VER) || defined(WIN64) || defined(_WIN64) || defined(__WIN64__) || defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#  define Q_DECL_EXPORT __declspec(dllexport)
#  define Q_DECL_IMPORT __declspec(dllimport)
#else
#  define Q_DECL_EXPORT     __attribute__((visibility("default")))
#  define Q_DECL_IMPORT     __attribute__((visibility("default")))
#endif

#if defined(FASTCPY_SHARED_LIBRARY)
#  define FASTCPY_EXPORT Q_DECL_EXPORT
#elif defined(FASTCPY_STATIC_LIBRARY) || defined(FASTCPY_OWNER_SOURCE)
#  define FASTCPY_EXPORT
#else
#  define FASTCPY_EXPORT Q_DECL_IMPORT
#endif

#endif // LIBRARY_GLOBAL_H

#include <stddef.h>
#if !defined(__AVX__) && !defined(__SSE__)
#error "You platform must support SSE!"
#endif


#ifdef __cplusplus
extern "C"
{
#endif

extern FASTCPY_EXPORT size_t fastcpy_non_temporal_threshold;

/* 分支为小内存复制优化 */
FASTCPY_EXPORT void* fastcpy_tiny(void *dst, const void *src, size_t size);

/* 均衡的内存复制实现 */
FASTCPY_EXPORT void* fastcpy(void *dst, const void *src, size_t size);

#ifdef __cplusplus
}
#endif

#endif // LIB_FAST_CPY_H
