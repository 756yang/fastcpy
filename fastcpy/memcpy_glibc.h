#ifndef __MEMCPY_GLIBC_H__
#define __MEMCPY_GLIBC_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

struct memcpy_glibc_config {
	size_t __x86_shared_non_temporal_threshold;
};

extern struct memcpy_glibc_config __memcpy_glibc_config;


void* memcpy_glibc(void *dst_, const void *src_, size_t size);

#ifdef __cplusplus
}
#endif

#endif

