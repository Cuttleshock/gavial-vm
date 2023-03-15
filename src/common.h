#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

void gvm_log(const char *format, ...);
void gvm_error(const char *format, ...);

#endif // COMMON_H
