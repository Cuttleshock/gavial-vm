#include <stdarg.h>
#include <stdio.h>

#include "common.h"

static void log_to(FILE *stream, const char *format, va_list args)
{
	vfprintf(stream, format, args);
	va_end(args);
}

void gvm_log(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	log_to(stdout, format, args);
}

void gvm_error(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	log_to(stderr, format, args);
}
