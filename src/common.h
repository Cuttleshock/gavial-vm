#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define RED "\x1B[31m"
#define GRN "\x1B[32m"
#define YLW "\x1B[33m"
#define BLU "\x1B[34m"
#define MAG "\x1B[35m"
#define CYN "\x1B[36m"
#define WHT "\x1B[37m"
#define COLOR_RESET "\x1B[0m"

#define SPRITE_SZ 12
#define SPRITE_COLS 7
#define SPRITE_ROWS 100
#define N_SPRITE_FLAGS 8

void gvm_log(const char *format, ...);
void gvm_error(const char *format, ...);
void gvm_assert(bool assertion, const char *format, ...);

long gvm_rand(long seed);

#endif // COMMON_H
