#include <stdint.h>
#include <string.h>

#define CROCOMACS_IMPL
#include "common.h"
#include "table.h"

#define LOAD_LIMIT 0.75

char *const TOMBSTONE = ""; // Unique sentinel value for deleted entries

struct callable {
	int arg_count;
	bool is_macro;
	union {
		CcmHook hook;
		Sentence expansion;
	} as;
};

struct entry {
	Word key;
	struct callable value;
};

struct table {
	int count;
	int capacity;
	struct entry *entries;
};

static struct table primitives_table = {
	.count = 0,
	.capacity = 0,
	.entries = NULL,
};

static struct table macros_table = {
	.count = 0,
	.capacity = 0,
	.entries = NULL,
};

void print_word(Word *w)
{
	switch (w->type) {
		case WORD_NUMBER:
			ccm_log("%g", w->as.number);
			return;
		case WORD_STRING:
			ccm_log("\"%.*s\"", w->as.str.length, w->as.str.chars);
			return;
		case WORD_SYMBOL:
			ccm_log("`%.*s", w->as.str.length, w->as.str.chars);
			return;
		case WORD_MACRO:
			ccm_log("%.*s", w->as.str.length, w->as.str.chars);
			return;
		case WORD_PRIMITIVE:
			ccm_log("#%.*s", w->as.str.length, w->as.str.chars);
			return;
		case WORD_PARAMETER:
			ccm_log("?%d", w->as.arg_index);
			return;
		case WORD_CALL:
			// Calls always have at least 1 arg
			ccm_log("(");
			for (int i = 0; i < w->as.call.arg_count - 1; ++i) {
				print_sentence(&w->as.call.args[i]);
				ccm_log(", ");
			}
			print_sentence(&w->as.call.args[w->as.call.arg_count - 1]); // no trailing comma
			ccm_log(")");
			return;
	}
}

void print_sentence(Sentence *s)
{
	if (s->word_count == 0) {
		return;
	}

	for (int i = 0; i < s->word_count - 1; ++i) {
		print_word(&s->words[i]);
		ccm_log(" ");
	}
	print_word(&s->words[s->word_count - 1]); // no trailing space
}

// djb2 algorithm - Daniel J. Bernstein, 1991. Appears to be in the public domain.
static uint64_t hash(const char *key, int key_length)
{
	uint64_t hash = 5381;

	for (int i = 0; i < key_length; ++i) {
		hash += (hash << 5) + key[i];
	}

	return hash;
}

// Returns: target insertion location (either exact match, or first empty slot after it)
static struct entry *table_find_location(struct table *table, const char *key, int key_length)
{
	if (table->capacity == 0) {
		return NULL;
	}

	struct entry *first_match = NULL;
	for (uint64_t i = hash(key, key_length) % table->capacity; true; i = (i + 1) % table->capacity) {
		const char *chars = table->entries[i].key.as.str.chars;
		int length = table->entries[i].key.as.str.length;
		if (chars == NULL) {
			if (first_match == NULL) {
				first_match = &table->entries[i];
			}
			return first_match;
		} else if (chars == TOMBSTONE) {
			if (first_match == NULL) {
				first_match = &table->entries[i];
			}
			continue;
		} else if (length != key_length) {
			continue;
		} else if (strncmp(chars, key, length) != 0) {
			continue;
		} else {
			return &table->entries[i];
		}
	}
}

static bool is_empty(struct entry *entry)
{
	return NULL == entry || NULL == entry->key.as.str.chars || TOMBSTONE == entry->key.as.str.chars;
}

// Returns: pointer to entry, or NULL if absent
static struct entry *table_get(struct table *table, const char *key, int key_length)
{
	struct entry *entry = table_find_location(table, key, key_length);
	if (is_empty(entry)) {
		return NULL;
	} else {
		return entry;
	}
}

// Returns: success
static bool table_set(struct table *table, const char *key, int key_length, struct callable *value)
{
	if (table->count >= table->capacity * LOAD_LIMIT) { // Similar to parser::start_macro_def()
		// Allocate a new table
		struct table old_table = *table;
		table->capacity = (table->capacity == 0) ? 8 : 2 * table->capacity; // TODO: Magic number
		// table->count stays the same as we quietly move entries into it
		table->entries = ccm_malloc(table->capacity * sizeof(*table->entries));
		if (NULL == table->entries) {
			*table = old_table;
			return false;
		}
		// Initialise new table (TODO: careful - sure we don't need any more?)
		for (int i = 0; i < table->capacity; ++i) {
			table->entries[i].key.as.str.chars = NULL;
		}
		// Move all entries to the correct place in the new table
		for (int i = 0; i < old_table.capacity; ++i) {
			struct entry *from = &old_table.entries[i];
			if (!is_empty(from)) {
				struct entry *to = table_find_location(table, from->key.as.str.chars, from->key.as.str.length);
				*to = *from;
			}
		}
		// A free to match the malloc
		ccm_free(old_table.entries);
	}

	struct entry *entry = table_find_location(table, key, key_length);
	if (is_empty(entry)) {
		// Does not exist - copy key, set, increment count
		char *str = ccm_malloc(key_length);
		if (NULL == str) {
			return false;
		}
		memcpy(str, key, key_length);
		entry->key.type = WORD_STRING;
		entry->key.as.str.chars = str;
		entry->key.as.str.length = key_length;
		entry->value = *value;
		++table->count;
		return true;
	} else {
		// Free existing value, set
		if (entry->value.is_macro) {
			free_sentence(&entry->value.as.expansion);
		}
		entry->value = *value;
		return true;
	}
}

static void free_table(struct table *table)
{
	for (int i = 0; i < table->capacity; ++i) {
		if (!is_empty(&table->entries[i])) {
			free_word(&table->entries[i].key);
			if (table->entries[i].value.is_macro) {
				free_sentence(&table->entries[i].value.as.expansion);
			}
		}
	}
	table->count = 0;
	table->capacity = 0;
	ccm_free(table->entries);
	table->entries = NULL;
}

static void table_delete(struct table *table, const char *key, int key_length)
{
	struct entry *entry = table_get(table, key, key_length);
	if (entry != NULL) {
		free_word(&entry->key);
		entry->key.as.str.chars = TOMBSTONE;
		if (entry->value.is_macro) {
			free_sentence(&entry->value.as.expansion);
		}
		--table->count;
	}
}

void free_word(Word *word)
{
	switch (word->type) {
		case WORD_STRING:
		case WORD_MACRO:
		case WORD_PRIMITIVE:
		case WORD_SYMBOL:
			ccm_free(word->as.str.chars);
			return;
		case WORD_NUMBER:
		case WORD_PARAMETER:
			return;
		case WORD_CALL:
			for (int i = 0; i < word->as.call.arg_count; ++i) {
				free_sentence(&word->as.call.args[i]);
			}
			ccm_free(word->as.call.args);
			return;
	}
}

void free_sentence(Sentence *sentence)
{
	for (int i = 0; i < sentence->word_count; ++i) {
		free_word(&sentence->words[i]);
	}
	ccm_free(sentence->words);
}

// Defines named primitive, copying name
bool define_primitive(const char *name, int name_length, int arg_count, CcmHook hook)
{
	struct callable c;
	c.arg_count = arg_count;
	c.is_macro = false;
	c.as.hook = hook;
	return table_set(&primitives_table, name, name_length, &c);
}

void forget_primitives()
{
	free_table(&primitives_table);
}

// Returns NULL on failure; populates *out_arg_count on success
CcmHook get_primitive(const char *name, int name_length, int *out_arg_count)
{
	struct entry *entry = table_get(&primitives_table, name, name_length);
	if (entry == NULL) {
		return NULL;
	} else {
		*out_arg_count = entry->value.arg_count;
		return entry->value.as.hook;
	}
}

// Defines named macro, copying name and taking ownership of expansion on success
bool define_macro(const char *name, int name_length, int arg_count, Sentence *expansion)
{
#ifdef CCM_DEBUG_PRINT_DEFINITION
	ccm_log("%.*s (%d)\n", name_length, name, arg_count);
	print_sentence(expansion);
	ccm_log("\n");
#endif
	struct callable c;
	c.arg_count = arg_count;
	c.is_macro = true;
	c.as.expansion = *expansion;
	return table_set(&macros_table, name, name_length, &c);
}

// Undefines named macro, freeing associated memory
void forget_macro(const char *name, int name_length)
{
	table_delete(&macros_table, name, name_length);
}

void forget_macros()
{
	free_table(&macros_table);
}

// Returns NULL on failure; populates *out_arg_count on success
Sentence *expand_macro(const char *name, int name_length, int *out_arg_count)
{
	struct entry *entry = table_get(&macros_table, name, name_length);
	if (entry == NULL) {
		return NULL;
	} else {
		*out_arg_count = entry->value.arg_count;
		return &entry->value.as.expansion;
	}
}
