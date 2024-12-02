#ifndef _MALLOC_USER_H_
#define _MALLOC_USER_H_

#include <stdint.h>

typedef struct blockflag
{
	struct blockflag *nextblock;//此块空闲则指向下一个空闲块,否则指向上一个空闲块
	uint32_t size;//块所占内存的大小
	//char *blockmem;//块内存,指向块标志之后
} blockflag;//动态存储区中的内存块


class mempool {
	char *memory;
	char *endmem;
	blockflag *blockpos;//动态存储区的块使用标志
public:
	mempool(size_t size) {
		if(size == 0) {
			endmem = memory = NULL;
			blockpos = NULL;
		}
		else {
			endmem = (memory = new char[size]()) + size;
			blockpos = (blockflag*)memory;
			blockpos->nextblock = blockpos;
			blockpos->size = size;
		}
	}
	~mempool() {
		delete[] memory;
	}

	void reset() {
		blockpos = (blockflag*)memory;
		blockpos->nextblock = blockpos;
		blockpos->size = endmem - memory;
	}
	void* memalloc(uint32_t size);
	void memfree(void *p);
};


#endif

