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

// If assertion is falsey, log a message or open a message box with the same contents
// that assert_or_debug() would print; then exit the program cleanly.
void gvm_assert(bool assertion, const char *format, ...)
{
	if (!assertion) {
		va_list args;
		va_start(args, format);
		// TODO: Make text red (gvm_malloc(strlen(format) + 10-ish), strncat, log_to, gvm_free)
		log_to(stderr, format, args);
		// TODO: Cleaner exit
		exit(EXIT_FAILURE);
	}
}
