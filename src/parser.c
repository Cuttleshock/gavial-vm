#include <string.h>

#include "common.h"
#include "crocomacs/crocomacs.h"
#include "filesystem.h"
#include "memory.h"
#include "parser.h"
#include "subsystems/subsystems.h"
#include "value.h"
#include "vm.h"

// TODO: Better to shape CcmRealloc like gvm_realloc()
static void *gvm_ccm_realloc_wrapper(void *ptr, size_t size)
{
	return gvm_realloc(ptr, 0, size);
}

static void hook_number(double d)
{
	constant(SCAL(d));
}

static void hook_string(const char *str, int length)
{
	gvm_log("STRING: %.*s\n", length, str);
}

static void hook_symbol(const char *str, int length)
{
	int index;
	bool found = locate_state(str, length, &index);
	if (!found) {
		ccm_runtime_error("Undefined variable");
	} else {
		instruction(OP_GET);
		instruction(index);
	}
}

static void hook_DEFINE_SCALAR(CcmList lists[])
{
	const char *name = lists[0].values[0].as.str.chars;
	int length = lists[0].values[0].as.str.length;
	GvmConstant value = SCAL(lists[1].values[0].as.number);
	// TODO: Not bothered to error-check this because we should null-terminate
	// CcmValue strings anyway
	char *name_copy = gvm_malloc(length + 1);
	memcpy(name_copy, name, length);
	name_copy[length] = '\0';
	define_state(value, name_copy);
	gvm_free(name_copy);
}

static void hook_DEFINE_VEC2(CcmList lists[])
{
	const char *name = lists[0].values[0].as.str.chars;
	int length = lists[0].values[0].as.str.length;
	GvmConstant value = VEC2(lists[1].values[0].as.number, lists[2].values[0].as.number);
	char *name_copy = gvm_malloc(length + 1);
	memcpy(name_copy, name, length);
	name_copy[length] = '\0';
	define_state(value, name_copy);
	gvm_free(name_copy);
}

static void hook_DEFINE_VEC4(CcmList lists[])
{
	const char *name = lists[0].values[0].as.str.chars;
	int length = lists[0].values[0].as.str.length;
	GvmConstant value = VEC4(
		lists[1].values[0].as.number,
		lists[2].values[0].as.number,
		lists[3].values[0].as.number,
		lists[4].values[0].as.number
	);
	char *name_copy = gvm_malloc(length + 1);
	memcpy(name_copy, name, length);
	name_copy[length] = '\0';
	define_state(value, name_copy);
	gvm_free(name_copy);
}

static void hook_SET(CcmList lists[])
{
	const char *name = lists[0].values[0].as.str.chars;
	int length = lists[0].values[0].as.str.length;
	int index;
	bool found = locate_state(name, length, &index);
	if (!found) {
		ccm_runtime_error("Undefined variable");
	} else {
		instruction(OP_SET);
		instruction(index);
	}
}

static void hook_MODULO(CcmList *)
{
	instruction(OP_MODULO);
}

static void hook_ADD(CcmList *)
{
	instruction(OP_ADD);
}

static void hook_SUBTRACT(CcmList *)
{
	instruction(OP_SUBTRACT);
}

// TODO: Argument checking
static void hook_VEC2(CcmList lists[])
{
	int a = lists[0].values[0].as.number;
	int b = lists[1].values[0].as.number;
	constant(VEC2(a, b)); // TODO: ... and error checking
}

static void hook_MAKE_VEC2(CcmList lists[])
{
	instruction(OP_MAKE_VEC2);
}

static void hook_GET_X(CcmList lists[])
{
	instruction(OP_GET_X);
}

static void hook_GET_Y(CcmList lists[])
{
	instruction(OP_GET_Y);
}

static void hook_PRESSED(CcmList lists[])
{
	int button = lists[0].values[0].as.number;
	instruction(OP_BUTTON_PRESSED);
	instruction(button);
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

static bool parse_update_impl(const char *src, int src_length, const char *predef_src, int predef_length)
{
#define TRY(name, arg_count) \
	if (!ccm_define_primitive(#name, sizeof(#name) - 1, arg_count, hook_ ## name)) return false;

	ccm_set_logger(gvm_error);
	ccm_set_allocators(gvm_malloc, gvm_ccm_realloc_wrapper, gvm_free);
	ccm_set_number_hook(hook_number);
	ccm_set_string_hook(hook_string);
	ccm_set_symbol_hook(hook_symbol);

	TRY(DEFINE_SCALAR, 2);
	TRY(DEFINE_VEC2, 3);
	TRY(DEFINE_VEC4, 5);
	TRY(SET, 1);
	TRY(ADD, 0);
	TRY(SUBTRACT, 0);
	TRY(MODULO, 0);
	TRY(VEC2, 2);
	TRY(PRESSED, 1);
	TRY(MAKE_VEC2, 0);
	TRY(GET_X, 0);
	TRY(GET_Y, 0);
	TRY(FILL_RECT, 2);
	TRY(RETURN, 0);

	if (!ccm_execute(predef_src, predef_length)) {
		return false;
	}

	return ccm_execute(src, src_length);

#undef TRY
}

static bool parse_update(const char *rom_path)
{
	// TODO: Not sure if keeping them in a file is necessary/helpful
	int predef_length;
	char *predef_src = read_file_executable("predefs.ccm", &predef_length);
	if (NULL == predef_src) {
		gvm_error("Could not read predefs.ccm: aborting\n");
		return false;
	}

	int src_length;
	char *src = read_file(rom_path, &src_length);
	if (NULL == src) {
		gvm_error("Could not read %s: aborting\n", rom_path);
		gvm_free(predef_src);
		return false;
	}

	bool success = parse_update_impl(src, src_length, predef_src, predef_length);
	ccm_cleanup();
	gvm_free(src);
	gvm_free(predef_src);
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

	TRY(parse_update(rom_path));
	TRY(parse_palettes());

	return true;

#undef TRY
}
