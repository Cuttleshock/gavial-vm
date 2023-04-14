#include <string.h>

#include "common.h"
#include "crocomacs/crocomacs.h"
#include "filesystem.h"
#include "memory.h"
#include "parser.h"
#include "subsystems/subsystems.h"
#include "value.h"
#include "vm.h"

static int constant_count = 0;

static GvmState dummy_state[] = {
	{ "snakes", LIT_SCAL(0), LIT_SCAL(5), VAL_SCALAR },
	{ "badgers", LIT_VEC2(0, 0), LIT_VEC2(10, 20), VAL_VEC2 },
	{ "dogs", LIT_VEC4(0, 0, 0, 0), LIT_VEC4(3, 3, 2, 2), VAL_VEC4 },
	{ NULL, LIT_SCAL(0), LIT_SCAL(0), VAL_NONE },
};

static GvmState implicit_state[] = {
	{ "Camera", LIT_VEC2(0, 0), LIT_VEC2(0, 0), VAL_VEC2 },
	{ "Time", LIT_SCAL(0), LIT_SCAL(0), VAL_SCALAR },
	{ "Pal0", LIT_SCAL(0), LIT_SCAL(0), VAL_SCALAR },
};

static void parse_error(const char *message)
{
	gvm_error("%s\n", message);
}

// TODO: Read a line from the source file and transform to state
static GvmState consume_state()
{
	static int line = 0;
	return dummy_state[line++];
}

static bool parse_state()
{
	// Read all state declared in source file
	for (GvmState item = consume_state(); item.type != VAL_NONE; item = consume_state()) {
		if (!insert_state(item, strlen(item.name))) {
			parse_error("Failed to add state");
			return false;
		}
	}

	// Insert implicit state if not already set
	for (int i = 0; i < sizeof(implicit_state) / sizeof(implicit_state[0]); ++i) {
		GvmState implicit = implicit_state[i];
		GvmState *explicit = get_state(implicit.name, strlen(implicit.name));
		if (explicit != NULL) {
			if (explicit->type != implicit.type) {
				parse_error("State set with invalid type");
				return false;
			}
		} else {
			if (!insert_state(implicit, strlen(implicit.name))) {
				parse_error("Failed to add state");
				return false;
			}
		}
	}

	return true;
}

// TODO: Better to shape CcmRealloc like gvm_realloc()
static void *ccm_realloc_wrapper(void *ptr, size_t size)
{
	return gvm_realloc(ptr, 0, size);
}

static void hook_number(double d)
{
	constant(SCAL(d));
	instruction(OP_LOAD_CONST);
	instruction(constant_count++);
}

static void hook_string(const char *str, int length)
{
	gvm_log("STRING: %.*s\n", length, str);
}

static void hook_ADD(CcmList *)
{
	instruction(OP_ADD);
}

// TODO: Argument checking
static void hook_VEC2(CcmList lists[])
{
	int a = lists[0].values[0].as.number;
	int b = lists[1].values[0].as.number;
	constant(VEC2(a, b));
	instruction(OP_LOAD_CONST);
	instruction(constant_count++);
}

// TODO: Argument checking
static void hook_FILL_RECT(CcmList lists[])
{
	int pal = lists[0].values[0].as.number;
	int col = lists[1].values[0].as.number;
	instruction(OP_FILL_RECT);
	instruction(pal);
	instruction(col);
}

static void hook_RETURN(CcmList *)
{
	instruction(OP_RETURN);
}

static bool parse_update_impl(const char *src, int src_length)
{
#define TRY(name, arg_count) \
	if (!ccm_define_primitive(#name, sizeof(#name) - 1, arg_count, hook_ ## name)) return false;

	ccm_set_logger(gvm_error);
	ccm_set_allocators(gvm_malloc, ccm_realloc_wrapper, gvm_free);
	ccm_set_number_hook(hook_number);
	ccm_set_string_hook(hook_string);

	TRY(ADD, 0);
	TRY(VEC2, 2);
	TRY(FILL_RECT, 2);
	TRY(RETURN, 0);

	return ccm_execute(src, src_length);

#undef TRY
}

static bool parse_update(const char *src, int src_length)
{
	bool success = parse_update_impl(src, src_length);
	ccm_cleanup();
	constant_count = 0;
	return success;
}

static bool parse_palettes()
{
	// TODO: Error checking
	set_palette_colour(0, 0, 0.0, 0.0, 0.0); // black
	set_palette_colour(0, 1, 1.0, 0.0, 0.0); // red
	set_palette_colour(0, 2, 0.0, 1.0, 0.0); // green
	set_palette_colour(0, 3, 0.0, 0.0, 1.0); // blue
	bind_palette(0, 0);

	set_palette_colour(1, 0, 1.0, 1.0, 1.0); // white
	set_palette_colour(1, 1, 1.0, 0.0, 1.0); // magenta
	set_palette_colour(1, 2, 0.0, 1.0, 1.0); // cyan
	set_palette_colour(1, 3, 1.0, 1.0, 0.0); // brown or something
	bind_palette(1, 1);

	return true;
}

bool parse(const char *rom_path)
{
#define TRY(p) if (!p) return false;

	int src_length;
	char *src = read_file(rom_path, &src_length);
	if (NULL == src) {
		gvm_error("Could not read file: aborting\n");
		return false;
	}

	TRY(parse_state());
	TRY(parse_update(src, src_length));
	TRY(parse_palettes());

	gvm_free(src);

	return true;

#undef TRY
}
