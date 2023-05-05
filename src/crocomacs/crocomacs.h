#ifndef CROCOMACS_H
#define CROCOMACS_H

#include <stdbool.h>

#include "shared_types.h"

void ccm_set_logger(CcmLogger logger);
void ccm_set_allocators(CcmMalloc, CcmRealloc, CcmFree);

bool ccm_define_primitive(const char *name, int name_length, int arg_count, CcmHook hook);
void ccm_set_number_hook(CcmNumberHook);
void ccm_set_string_hook(CcmStringHook);
void ccm_set_symbol_hook(CcmSymbolHook);

void ccm_runtime_error(const char *message); // TODO: accept '...'
bool ccm_compile(const char *source, int length, int initial_line);
bool ccm_execute(const char *source, int length, int initial_line);
void ccm_cleanup();

#endif // CROCOMACS_H
