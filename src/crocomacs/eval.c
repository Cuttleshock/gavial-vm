#include <stddef.h> // NULL

#define CROCOMACS_IMPL
#include "common.h"
#include "eval.h"
#include "table.h"

static CcmNumberHook number_hook = NULL;
static CcmStringHook string_hook = NULL;
static CcmSymbolHook symbol_hook = NULL;

struct call_stack {
	Word *call;
	struct call_stack *parent;
};

// TODO: It's starting to look like Sentence should contain its own arg_count
static struct {
	bool is_sentence;
	union {
		Sentence *sentence;
		CcmHook primitive;
	} as;
	int arg_count;
} pending_calls[256]; // TODO: arbitrary limit
static int pending_count = 0;

static bool had_error = false;

static void free_lists(CcmList *lists, int count)
{
	for (int i = 0; i < count; ++i) {
		ccm_free(lists[i].values);
	}
	ccm_free(lists);
}

static bool sentence(Sentence *s, struct call_stack cs, CcmList *list);

// TODO: The only instance of dynamic allocation at runtime - can we avoid it,
// e.g. by capping at 256 args and passing Sentences directly?
static CcmList *call_to_lists(Word call, struct call_stack cs)
{
	CcmList *lists = ccm_malloc(call.as.call.arg_count * sizeof(*lists));
	if (NULL == lists) {
		runtime_error("Out of memory");
		return NULL;
	}

	for (int i = 0; i < call.as.call.arg_count; ++i) {
		lists[i].count = 0;
		lists[i].capacity = 0;
		lists[i].values = NULL;
		if (!sentence(&call.as.call.args[i], cs, &lists[i])) {
			free_lists(lists, i);
			return NULL;
		}
	}

	return lists;
}

static bool push_value(CcmValue v, CcmList *list)
{
	if (list->capacity >= list->count) { // See parser::start_macro_def()
		CcmList old_list = *list;
		list->capacity = (list->count == 0) ? 8 : 2 * list->count; // TODO: Magic number
		list->values = ccm_realloc(list->values, list->capacity * sizeof(*list->values));
		if (NULL == list->values) {
			*list = old_list;
			runtime_error("Out of memory");
			return false;
		}
	}

	list->values[list->count++] = v;
	return true;
}

static bool push_string(Word w, CcmList *list)
{
	CcmValue v;
	v.type = CCM_STRING;
	v.as.str.chars = w.as.str.chars;
	v.as.str.length = w.as.str.length;
	return push_value(v, list);
}

static bool push_number(Word w, CcmList *list)
{
	CcmValue v;
	v.type = CCM_NUMBER;
	v.as.number = w.as.number;
	return push_value(v, list);
}

static bool push_symbol(Word w, CcmList *list)
{
	CcmValue v;
	v.type = CCM_SYMBOL;
	v.as.str.chars = w.as.str.chars;
	v.as.str.length = w.as.str.length;
	return push_value(v, list);
}

// TODO: Break up this 150-liner
static bool sentence(Sentence *s, struct call_stack cs, CcmList *list)
{
	for (int i = 0; i < s->word_count && !had_error; ++i) {
		Word w = s->words[i];
		if (pending_count == 0) {
			switch (w.type) {
				case WORD_STRING:
					if (list != NULL) {
						push_string(w, list);
					} else if (string_hook != NULL) {
						string_hook(w.as.str.chars, w.as.str.length);
					}
					continue;
				case WORD_NUMBER:
					if (list != NULL) {
						push_number(w, list);
					} else if (number_hook != NULL) {
						number_hook(w.as.number);
					}
					continue;
				case WORD_MACRO: {
					int arg_count;
					Sentence *expansion = expand_macro(w.as.str.chars, w.as.str.length, &arg_count);
					if (expansion == NULL) {
						runtime_error("Undefined macro");
					} else if (arg_count == 0) {
						struct call_stack empty = {
							.call = &w,
							.parent = &cs,
						};
						sentence(expansion, empty, list);
					} else {
						pending_calls[0].is_sentence = true;
						pending_calls[0].as.sentence = expansion;
						pending_calls[0].arg_count = arg_count;
						pending_count = 1;
					}
					continue;
				}
				case WORD_PRIMITIVE: {
					int arg_count;
					CcmHook primitive = get_primitive(w.as.str.chars, w.as.str.length, &arg_count);
					if (primitive == NULL) {
						runtime_error("Undefined primitive");
					} else if (arg_count == 0) {
						CcmList empty[] = {};
						primitive(empty);
					} else {
						pending_calls[0].is_sentence = false;
						pending_calls[0].as.primitive = primitive;
						pending_calls[0].arg_count = arg_count;
						pending_count = 1;
					}
					continue;
				}
				case WORD_SYMBOL:
					if (list != NULL) {
						push_symbol(w, list);
					} else if (symbol_hook != NULL) {
						symbol_hook(w.as.str.chars, w.as.str.length);
					}
					continue;
				case WORD_PARAMETER: {
					int index = w.as.arg_index;
					sentence(&cs.call->as.call.args[index], *cs.parent, list);
					continue;
				}
				case WORD_CALL:
					runtime_error("Unmatched argument list");
					continue;
			}
		} else {
			switch (w.type) {
				case WORD_STRING:
				case WORD_NUMBER:
				case WORD_SYMBOL:
				case WORD_PRIMITIVE:
					runtime_error("Expected argument list");
					continue;
				case WORD_MACRO: {
					int arg_count;
					Sentence *expansion = expand_macro(w.as.str.chars, w.as.str.length, &arg_count);
					if (expansion == NULL) {
						runtime_error("Undefined macro");
					} else if (arg_count == 0) {
						struct call_stack empty = {
							.call = &w,
							.parent = &cs,
						};
						sentence(expansion, empty, list);
					} else if (pending_count >= sizeof(pending_calls) / sizeof(pending_calls[0])) {
						runtime_error("Too many pending macro expansions");
					} else {
						pending_calls[pending_count].is_sentence = true;
						pending_calls[pending_count].as.sentence = expansion;
						pending_calls[pending_count].arg_count = arg_count;
						++pending_count;
					}
					continue;
				}
				case WORD_PARAMETER: {
					int index = w.as.arg_index;
					sentence(&cs.call->as.call.args[index], *cs.parent, list);
					continue;
				}
				case WORD_CALL:
					--pending_count;
					if (w.as.call.arg_count == pending_calls[pending_count].arg_count) {
						if (pending_calls[pending_count].is_sentence) {
							struct call_stack c = {
								.call = &w,
								.parent = &cs,
							};
							sentence(pending_calls[pending_count].as.sentence, c, list);
						} else {
							CcmList *args = call_to_lists(w, cs);
							if (args != NULL) {
								pending_calls[pending_count].as.primitive(args);
								free_lists(args, w.as.call.arg_count);
							}
						}
					} else {
						runtime_error("Mismatched argument count");
					}
					continue;
			}
		}
	}

	return !had_error;
}

void set_number_hook(CcmNumberHook hook)
{
	number_hook = hook;
}

void set_string_hook(CcmStringHook hook)
{
	string_hook = hook;
}

void set_symbol_hook(CcmSymbolHook hook)
{
	symbol_hook = hook;
}

void runtime_error(const char *message)
{
	had_error = true;
	ccm_log("Error evaluating script: %s\n", message);
}

bool eval_macro(const char *name, int name_length)
{
	pending_count = 0;
	had_error = false;

	// TODO: Why not make a synthetic Sentence?
	int arg_count;
	Sentence *top = expand_macro(name, name_length, &arg_count);
	if (top == NULL || arg_count != 0) {
		return false;
	}

	struct call_stack cs = {
		.call = NULL,
		.parent = NULL,
	};

	return sentence(top, cs, NULL);
}
