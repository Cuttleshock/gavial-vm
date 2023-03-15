#include "common.h"

void *gvm_malloc(size_t size);
void *gvm_realloc(void *ptr, size_t oldsize, size_t newsize);
void gvm_free(void *ptr);
