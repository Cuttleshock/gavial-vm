#ifndef CROCOMACS_IMPL
	#error "Implementation file: do not include"
#endif

#ifndef CROCOMACS_PARSER_H
#define CROCOMACS_PARSER_H

#include <stdbool.h>

bool compile(const char *source, int length, const char *initial_name, int initial_name_length, int initial_line);

#endif // CROCOMACS_PARSER_H
