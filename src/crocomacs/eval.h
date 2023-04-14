#ifndef CROCOMACS_IMPL
	#error "Implementation file: do not include"
#endif

#ifndef CROCOMACS_EVAL_H
#define CROCOMACS_EVAL_H

#include <stdbool.h>

#include "shared_types.h"

void set_number_hook(CcmNumberHook);
void set_string_hook(CcmStringHook);
void set_symbol_hook(CcmSymbolHook);
void runtime_error(const char *message);
bool eval_macro(const char *name, int name_length);

#endif // CROCOMACS_EVAL_H
