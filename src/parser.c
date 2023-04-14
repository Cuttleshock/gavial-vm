#include <string.h>

#include "common.h"
#include "crocomacs/crocomacs.h"
#include "filesystem.h"
#include "memory.h"
#include "parser.h"
#include "subsystems/subsystems.h"
#include "value.h"
#include "vm.h"

// TODO: Does not belong here - extend vm.h to provide it
static int constant_count = 0;

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

	TRY(DEFINE_SCALAR, 2);
	TRY(DEFINE_VEC2, 3);
	TRY(DEFINE_VEC4, 5);
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

	TRY(parse_update(src, src_length));
	TRY(parse_palettes());

	gvm_free(src);

	return true;

#undef TRY
}
