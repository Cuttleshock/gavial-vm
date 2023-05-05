#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#define CROCOMACS_IMPL
#include "common.h"
#include "crocomacs.h"
#include "eval.h"
#include "parser.h"
#include "table.h"

// Magic macro 'name' for evaluation
const char *CURRENT_PROGRAM = "";

// TODO: Alternative: declare ccm_log etc. in crocomacs/common.h but don't
// define. The user is required to provide definitions at compile time.
static void ccm_log_default(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stdout, format, args);
}

CcmLogger ccm_log = ccm_log_default;
CcmMalloc ccm_malloc = malloc;
CcmRealloc ccm_realloc = realloc;
CcmFree ccm_free = free;

void ccm_set_logger(CcmLogger logger)
{
	if (logger != NULL) {
		ccm_log = logger;
	}
}

// TODO: Kind of silly to expose the possibility of changing allocators after
// init and before freeing
void ccm_set_allocators(CcmMalloc new_malloc, CcmRealloc new_realloc, CcmFree new_free)
{
	if (new_malloc != NULL && new_realloc != NULL && new_free != NULL) {
		ccm_malloc = new_malloc;
		ccm_realloc = new_realloc;
		ccm_free = new_free;
	}
}

// TODO: Not sure about this and friends just deferring to eval.c
bool ccm_define_primitive(const char *name, int name_length, int arg_count, CcmHook hook)
{
	return define_primitive(name, name_length, arg_count, hook);
}

void ccm_set_number_hook(CcmNumberHook hook)
{
	set_number_hook(hook);
}

void ccm_set_string_hook(CcmStringHook hook)
{
	set_string_hook(hook);
}

void ccm_set_symbol_hook(CcmSymbolHook hook)
{
	set_symbol_hook(hook);
}

// Stops execution, but does not forget macros
void ccm_runtime_error(const char *message)
{
	runtime_error(message);
}

// Shallow-define macros but do not run.
// Failure modes:
// - Parse error
bool ccm_compile(const char *source, int length, int initial_line)
{
	if (source == NULL) {
		return false;
	}

	return compile(source, length, CURRENT_PROGRAM, 0, initial_line);
}

// Fully expand the provided source, running all primitives.
// Failure modes:
// - Parse error
// - An undefined macro is encountered
// - Stack overflow (likely caused by recursion)
// - ccm_static_error() is called by a hook
bool ccm_execute(const char *source, int length, int initial_line)
{
	if (!ccm_compile(source, length, initial_line)) {
		return false;
	}

	return eval_macro(CURRENT_PROGRAM, 0);
}

// Frees all memory taken by Crocomacs, forgetting macros and primitives.
void ccm_cleanup()
{
	ccm_set_number_hook(NULL);
	ccm_set_string_hook(NULL);
	ccm_set_symbol_hook(NULL);
	forget_primitives();
	forget_macros();
}
