#ifndef CROCOMACS_IMPL
	#error "Implementation file: do not include"
#endif

#ifndef CROCOMACS_TABLE_H
#define CROCOMACS_TABLE_H

#include <stdbool.h>

#include "shared_types.h"

// TODO: This, free_word(), init_sentence() and others should be lifted out
typedef enum {
	WORD_STRING,
	WORD_NUMBER,
	WORD_MACRO,
	WORD_PRIMITIVE,
	WORD_SYMBOL,
	WORD_PARAMETER,
	WORD_CALL,
} WordType;

struct word;

typedef struct {
	struct word *words;
	int word_count;
	int word_capacity;
} Sentence;

typedef struct word {
	WordType type;
	union {
		double number; // WORD_NUMBER
		struct {
			char *chars;
			int length;
		} str; // WORD_STRING, WORD_SYMBOL, WORD_MACRO, WORD_PRIMITIVE
		int arg_index; // WORD_PARAMETER
		struct {
			Sentence *args;
			int arg_count;
		} call; // WORD_CALL
	} as;
} Word;

void free_word(Word *word);
void free_sentence(Sentence *sentence);
bool define_primitive(const char *name, int name_length, int arg_count, CcmHook hook);
void forget_primitives();
CcmHook get_primitive(const char *name, int name_length, int *out_arg_count);
bool define_macro(const char *name, int name_length, int arg_count, Sentence *expansion);
void forget_macro(const char *name, int name_length);
void forget_macros();
Sentence *expand_macro(const char *name, int name_length, int *out_arg_count);

#endif // CROCOMACS_TABLE_H
