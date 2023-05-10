#include <errno.h>
#include <stdlib.h> // strtod
#include <string.h>

#define CROCOMACS_IMPL
#include "common.h"
#include "parser.h"
#include "scanner.h"
#include "table.h"

// TODO: We can localise these but the results are ugly - worth it?
static struct definition {
	const char *chars; // Not own memory
	int length;
	int arg_count;
	Sentence definition; // Ownership given away on success
} *macros = NULL; // Lifespan within compile() call
static int macro_count = 0;
static int macro_capacity = 0;

static Token param_names[256]; // TODO: nobody needs more than this, right?

static void error_token(Token t)
{
	ccm_log("[%d:%d] Parse error: %.*s\n", t.line, t.character, t.length, t.chars);
}

static void error_at(Token t, const char *message)
{
	ccm_log("[%d:%d] Parse error: %s\n", t.line, t.character, message);
}

// TODO: Should this live in table?
static bool init_sentence(Sentence *sentence)
{
	sentence->word_count = 0;
	sentence->word_capacity = 8; // TODO: magic number
	sentence->words = ccm_malloc(sentence->word_capacity * sizeof(*sentence->words));

	return (sentence->words != NULL);
}

// Allocates space to start a new definition, updating global state. Also works
// for the very first definition (when macros == NULL).
static bool start_macro_def(const char *name, int name_length, int arg_count)
{
	if (macro_count >= macro_capacity) {
		int old_capacity = macro_capacity;
		struct definition *old_macros = macros;
		macro_capacity = (macro_count == 0) ? 8 : 2 * macro_count; // TODO: Magic number
		macros = ccm_realloc(macros, macro_capacity * sizeof(*macros));
		if (macros == NULL) {
			// realloc() leaves memory untouched on failure, so caller can clean up
			macro_capacity = old_capacity;
			macros = old_macros;
			return false;
		}
	}

	macros[macro_count].chars = name;
	macros[macro_count].length = name_length;
	macros[macro_count].arg_count = arg_count;
	if (init_sentence(&macros[macro_count].definition)) {
		++macro_count;
		return true;
	} else {
		return false;
	}
}

// Consumes a macro declaration, saving params to global state
static bool declaration(Token name, Sentence *_)
{
	Token lp = scan_token();
	if (lp.type != TOKEN_LEFT_PAREN) {
		error_at(name, "Expect '('");
		return false;
	}

	int arg_count = 0;
	Token first = scan_token();
	if (first.type == TOKEN_RIGHT_PAREN) {
		return start_macro_def(&name.chars[1], name.length - 1, arg_count);
	} else if (first.type == TOKEN_PARAMETER) {
		param_names[arg_count++] = first;
	} else {
		error_at(first, "Expect ')' or parameter name");
		return false;
	}

	while (true) {
		Token delimiter = scan_token();
		switch (delimiter.type) {
			case TOKEN_COMMA: {
				Token param = scan_token();
				if (param.type != TOKEN_PARAMETER) {
					error_at(param, "Expect parameter name");
					return false;
				} else if (arg_count >= 256) {
					error_at(param, "Too many parameters");
					return false;
				} else {
					param_names[arg_count++] = param;
					continue;
				}
			}
			case TOKEN_RIGHT_PAREN:
				return start_macro_def(&name.chars[1], name.length - 1, arg_count);
			default:
				error_at(delimiter, "Expect ',' or ')'");
				return false;
		}
	}
}

// Returns: pointer to next word to populate, or NULL if out of memory
// TODO: Should this live in table?
static Word *next_word_of(Sentence *sentence)
{
	if (sentence->word_capacity <= sentence->word_count) {
		int old_capacity = sentence->word_capacity;
		Word *old_words = sentence->words;
		sentence->word_capacity = 2 * sentence->word_count;
		sentence->words = ccm_realloc(sentence->words, sentence->word_capacity * sizeof(*sentence->words));
		if (sentence->words == NULL) {
			sentence->word_capacity = old_capacity;
			sentence->words = old_words;
			return NULL;
		}
	}

	return &sentence->words[sentence->word_count++];
}

// TODO: Why do we pass Sentence * and not Word * to all of these?
// Helper to clip string-like Words, omitting ""`# etc.
static bool stringish(const char *chars, int length, WordType type, int line, int character, Sentence *current)
{
	char *str = ccm_malloc(length);
	if (str == NULL) {
		return false;
	}

	Word *word = next_word_of(current);
	if (word == NULL) {
		ccm_free(str);
		return false;
	}

	memcpy(str, chars, length);
	word->type = type;
	word->line = line;
	word->character = character;
	word->as.str.chars = str;
	word->as.str.length = length;

	return true;
}

// Returns: copy of str with backslashes unescaped, not null-terminated
// On success, the length of the unescaped string is placed in dest_length.
// A single trailing backslash is treated as literal.
// TODO: The trailing backslash clause is unnecessary thanks to the scanner.
// In general, while simple, this set of escaping rules is unusual - pends
// further discussion and needs clear specification.
static char *unescape_string(const char *str, int length, int *dest_length)
{
	const char *end = str + length - 1;

	*dest_length = length;
	const char *first = memchr(str, '\\', length);
	while (first != NULL && first != end) {
		--(*dest_length);
		first += 2; // it's fine if first overshoots end by 1
		first = memchr(first, '\\', end - first + 1); // ... because we memchr(0)
	}

	char *ret = ccm_malloc(*dest_length);
	if (ret == NULL) {
		return NULL;
	}

	const char *from = str;
	const char *to = memchr(from, '\\', length);
	char *target = ret;
	while (to != NULL && to != end) {
		memcpy(target, from, to - from);
		target += to - from;
		from = to + 1; // if (from == end) then (to == NULL)
		to = memchr(from + 1, '\\', end - from); // ... because we memchr(0)
	}

	memcpy(target, from, end - from + 1);
	return ret;
}

// Unescapes Token by replacing "\c" -> "c" (c any byte) before saving as Word
static bool string(Token t, Sentence *current)
{
	int length;
	char *str = unescape_string(t.chars + 1, t.length - 2, &length);
	if (str == NULL) {
		return false;
	}

	Word *word = next_word_of(current);
	if (word == NULL) {
		ccm_free(str);
		return false;
	}

	word->type = WORD_STRING;
	word->line = t.line;
	word->character = t.character;
	word->as.str.chars = str;
	word->as.str.length = length;

	return true;
}

// TODO: See scanner::number()
static bool number(Token t, Sentence *current)
{
	char *endptr;
	double d = strtod(t.chars, &endptr);
	if (endptr != t.chars + t.length) {
		// Scanner passed us a bad double
		error_at(t, "Failed to parse number");
		return false;
	} // TODO: also check errno

	Word *word = next_word_of(current);
	if (word == NULL) {
		return false;
	}

	word->type = WORD_NUMBER;
	word->line = t.line;
	word->character = t.character;
	word->as.number = d;

	return true;
}

static bool macro(Token t, Sentence *current)
{
	return stringish(t.chars, t.length, WORD_MACRO, t.line, t.character, current);
}

static bool primitive(Token t, Sentence *current)
{
	return stringish(t.chars + 1, t.length - 1, WORD_PRIMITIVE, t.line, t.character, current);
}

static bool symbol(Token t, Sentence *current)
{
	return stringish(t.chars + 1, t.length - 1, WORD_SYMBOL, t.line, t.character, current);
}

static bool parameter(Token t, Sentence *current)
{
	int l = t.length;
	for (int i = 0; i < macros[macro_count - 1].arg_count; ++i) {
		if (l == param_names[i].length && strncmp(t.chars, param_names[i].chars, l) == 0) {
			Word *word = next_word_of(current);
			if (word != NULL) {
				word->type = WORD_PARAMETER;
				word->line = t.line;
				word->character = t.character;
				word->as.arg_index = i;
				return true;
			} else {
				return false;
			}
		}
	}

	error_at(t, "Could not resolve parameter");
	return false;
}

static bool call(Token left_paren, Sentence *current)
{
	int arg_capacity = sizeof(param_names) / sizeof(param_names[0]); // TODO: lazy
	Sentence *arglist = ccm_malloc(arg_capacity * sizeof(Sentence));
	if (arglist == NULL) {
		return false;
	}

	if (!init_sentence(&arglist[0])) {
		ccm_free(arglist);
		return false;
	}

	Word *word = next_word_of(current);
	if (word == NULL) {
		free_sentence(&arglist[0]);
		ccm_free(arglist);
		return false;
	}
	// After this, the current macro definition is always in a consistent state,
	// so we can leave cleanup to the caller

	word->type = WORD_CALL;
	word->line = left_paren.line;
	word->character = left_paren.character;
	word->as.call.arg_count = 1;
	word->as.call.args = arglist;

#define CASE(type, method) \
	case type: \
		if (method(t, &word->as.call.args[word->as.call.arg_count - 1])) { \
			continue; \
		} else { \
			return false; \
		}

	while (true) {
		Token t = scan_token();
		switch (t.type) {
			CASE(TOKEN_STRING, string);
			CASE(TOKEN_NUMBER, number);
			CASE(TOKEN_MACRO, macro);
			CASE(TOKEN_PRIMITIVE, primitive);
			CASE(TOKEN_SYMBOL, symbol);
			CASE(TOKEN_PARAMETER, parameter);
			CASE(TOKEN_LEFT_PAREN, call);
			case TOKEN_RIGHT_PAREN:
				return true;
			case TOKEN_COMMA:
				if (word->as.call.arg_count >= arg_capacity) {
					error_at(t, "Too many arguments");
					return false;
				} else if (!init_sentence(&word->as.call.args[word->as.call.arg_count])) {
					return false;
				} else {
					++word->as.call.arg_count;
					continue;
				}
			case TOKEN_DECLARATION:
				error_at(t, "Declarations are only permitted at top level");
				return false;
			case TOKEN_EOF:
				error_at(t, "Expect ')'");
				return false;
			case TOKEN_ERROR:
				error_token(t);
				return false;
		}
	}

#undef CASE
}

static void cleanup_parser()
{
	for (int i = 0; i < macro_count; ++i) {
		free_sentence(&macros[i].definition);
	}
	ccm_free(macros);
	macros = NULL;
	macro_count = 0;
	macro_capacity = 0;
}

static bool compile_impl(const char *source, int length, const char *initial_name, int initial_name_length, int initial_line)
{
	if (!start_macro_def(initial_name, initial_name_length, 0)) {
		return false;
	}

	set_source(source, length, initial_line);

#define CASE(type, method) \
	case type: \
		if (method(t, &macros[macro_count - 1].definition)) { \
			continue; \
		} else { \
			return false; \
		}

	while (true) {
		Token t = scan_token();
		switch (t.type) {
			CASE(TOKEN_STRING, string);
			CASE(TOKEN_NUMBER, number);
			CASE(TOKEN_MACRO, macro);
			CASE(TOKEN_PRIMITIVE, primitive);
			CASE(TOKEN_SYMBOL, symbol);
			CASE(TOKEN_DECLARATION, declaration);
			CASE(TOKEN_PARAMETER, parameter);
			CASE(TOKEN_LEFT_PAREN, call);
			case TOKEN_RIGHT_PAREN:
				error_at(t, "Unmatched ')'");
				return false;
			case TOKEN_COMMA:
				error_at(t, "Commas are only allowed in argument lists");
				return false;
			case TOKEN_ERROR:
				error_token(t);
				return false;
			case TOKEN_EOF:
				// Define in reverse order so it's easy to clean up in case of failure
				int total_count = macro_count;
				for (; macro_count > 0; --macro_count) {
					struct definition m = macros[macro_count - 1];
					if (!define_macro(m.chars, m.length, m.arg_count, &m.definition)) {
						for (int j = macro_count; j < total_count; ++j) {
							forget_macro(macros[j].chars, macros[j].length);
						}
						return false;
					}
				}
				return true;
		}
	}

#undef CASE
}

// Return value: success
// If successful, subsequent executions will respect all new definitions.
// Otherwise, no change is made.
bool compile(const char *source, int length, const char *initial_name, int initial_name_length, int initial_line)
{
	bool success = compile_impl(source, length, initial_name, initial_name_length, initial_line);
	cleanup_parser();
	return success;
}
