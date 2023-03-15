#include <stdlib.h>

#include "memory.h"

void *gvm_malloc(size_t size)
{
	return malloc(size);
}

void *gvm_realloc(void *ptr, size_t oldsize, size_t newsize)
{
	// oldsize noted here for memory management later down the line
	return realloc(ptr, newsize);
}

void gvm_free(void *ptr)
{
	free(ptr);
}
