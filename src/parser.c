#include <string.h>

#include "common.h"
#include "crocomacs/crocomacs.h"
#include "filesystem.h"
#include "memory.h"
#include "parser.h"
#include "subsystems/subsystems.h"
#include "value.h"
#include "vm.h"

// Stack of instruction indices of unresolved jumps
static uint32_t jump_stack[256];
static int jump_count = 0;

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
	state_instruction(OP_GET, str, length);
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

static void hook_SET_PAL(CcmList lists[])
{
	int pal = lists[0].values[0].as.number;
	int col = lists[1].values[0].as.number;
	double r = lists[2].values[0].as.number;
	double g = lists[3].values[0].as.number;
	double b = lists[4].values[0].as.number;
	set_palette_colour(pal, col, r, g, b);
}

static void hook_BIND_PAL(CcmList lists[])
{
	int bind = lists[0].values[0].as.number;
	int pal = lists[1].values[0].as.number;
	bind_palette(bind, pal);
}

static void hook_SET(CcmList lists[])
{
	const char *name = lists[0].values[0].as.str.chars;
	int length = lists[0].values[0].as.str.length;
	state_instruction(OP_SET, name, length);
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

static void hook_JUMP_IF_FALSE(CcmList[])
{
	if (jump_count >= sizeof(jump_stack) / sizeof(jump_stack[0])) {
		ccm_runtime_error("Too deeply nested control flow");
	} else {
		jump(OP_JUMP_IF_FALSE, &jump_stack[jump_count++]);
	}
}

static void hook_JUMP_AND_POP(CcmList[])
{
	if (jump_count == 0) {
		ccm_runtime_error("Attempt to resolve nonexistent jump");
	} else {
		uint32_t new_jump;
		jump(OP_JUMP, &new_jump);
		resolve_jump(jump_stack[jump_count - 1]);
		jump_stack[jump_count - 1] = new_jump;
	}
}

static void hook_POP_JUMP(CcmList[])
{
	if (jump_count == 0) {
		ccm_runtime_error("Attempt to resolve nonexistent jump");
	} else {
		resolve_jump(jump_stack[--jump_count]);
	}
}

static void hook_PRESSED(CcmList lists[])
{
	int button = lists[0].values[0].as.number;
	instruction(OP_BUTTON_PRESSED);
	instruction(button);
}

static void hook_CAM(CcmList[])
{
	instruction(OP_CAM);
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

static void hook_SPRITE(CcmList lists[])
{
	int sprite_x = lists[0].values[0].as.number;
	int sprite_y = lists[1].values[0].as.number;
	int pal = lists[2].values[0].as.number;
	int h_flip = lists[3].values[0].as.number;
	int v_flip = lists[4].values[0].as.number;
	instruction(OP_SPRITE);
	instruction(sprite_x);
	instruction(sprite_y);
	instruction(pal);
	instruction(h_flip);
	instruction(v_flip);
}

static void hook_RETURN(CcmList *)
{
	instruction(OP_RETURN);
}

static void save_hook_LOAD_SCALAR(CcmList lists[])
{
	const char *name = lists[0].values[0].as.str.chars;
	int length = lists[0].values[0].as.str.length;
	int scalar = lists[1].values[0].as.number;
	set_state(SCAL(scalar), name, length);
}

static void save_hook_LOAD_VEC2(CcmList lists[])
{
	const char *name = lists[0].values[0].as.str.chars;
	int length = lists[0].values[0].as.str.length;
	int x = lists[1].values[0].as.number;
	int y = lists[2].values[0].as.number;
	set_state(VEC2(x, y), name, length);
}

static void save_hook_LOAD_VEC4(CcmList lists[])
{
	const char *name = lists[0].values[0].as.str.chars;
	int length = lists[0].values[0].as.str.length;
	int x = lists[1].values[0].as.number;
	int y = lists[2].values[0].as.number;
	int z = lists[3].values[0].as.number;
	int w = lists[4].values[0].as.number;
	set_state(VEC4(x, y, z, w), name, length);
}

static bool parse_update_impl(const char *src, int src_length, const char *predef_src, int predef_length)
{
#define TRY(name, arg_count) \
	if (!ccm_define_primitive(#name, sizeof(#name) - 1, arg_count, hook_ ## name)) return false

	ccm_set_logger(gvm_error);
	ccm_set_allocators(gvm_malloc, gvm_ccm_realloc_wrapper, gvm_free);
	ccm_set_number_hook(hook_number);
	ccm_set_string_hook(hook_string);
	ccm_set_symbol_hook(hook_symbol);

	TRY(DEFINE_SCALAR, 2);
	TRY(DEFINE_VEC2, 3);
	TRY(DEFINE_VEC4, 5);
	TRY(SET_PAL, 5);
	TRY(BIND_PAL, 2);
	TRY(SET, 1);
	TRY(ADD, 0);
	TRY(SUBTRACT, 0);
	TRY(MODULO, 0);
	TRY(VEC2, 2);
	TRY(PRESSED, 1);
	TRY(MAKE_VEC2, 0);
	TRY(GET_X, 0);
	TRY(GET_Y, 0);
	TRY(JUMP_IF_FALSE, 0);
	TRY(JUMP_AND_POP, 0);
	TRY(POP_JUMP, 0);
	TRY(CAM, 0);
	TRY(FILL_RECT, 2);
	TRY(SPRITE, 5);
	TRY(RETURN, 0);

	if (!ccm_execute(predef_src, predef_length)) {
		return false;
	}

	return ccm_execute(src, src_length);

#undef TRY
}

static bool parse_update(const char *src, int src_length)
{
	// TODO: Not sure if keeping them in a file is necessary/helpful
	int predef_length;
	char *predef_src = read_file_executable("predefs.ccm", &predef_length);
	if (NULL == predef_src) {
		gvm_error("Could not read predefs.ccm: aborting\n");
		return false;
	}

	bool success = parse_update_impl(src, src_length, predef_src, predef_length);
	ccm_cleanup();
	gvm_free(predef_src);
	return success;
}

// Returns: number of characters until newline or null terminator
static int line_length(const char *chars)
{
	const char *newline = strchr(chars, '\n');
	if (newline != NULL) {
		return newline - chars;
	} else {
		return strlen(chars);
	}
}

// Returns: pointer to just after the next newline in str, or NULL if none
static const char *next_line(const char *str)
{
	const char *line_end = strchr(str, '\n');
	if (line_end == NULL) {
		return NULL;
	} else {
		return ++line_end;
	}
}

// Searches 'src' for a full line matching 'line' exactly, terminated by a
// newline or the end of the string
// Returns: start of line, or NULL if not found
static const char *find_line(const char *str, const char *line)
{
	int length = strlen(line);
	const char *location = strstr(str, line);
	while (location != NULL && str != NULL) {
		char term = location[length];
		if ((term == '\n' || term == '\0') && (location == str || location[-1] == '\n')) {
			return location;
		}
		str = next_line(location);
		location = strstr(str, line);
	}

	return NULL;
}

static bool parse_sprite(const char *src, int src_length)
{
	const char *end = &src[src_length];
	int n_rows = 0;
	for (const char *tapehead = src; tapehead < end && tapehead != NULL; tapehead = next_line(tapehead)) {
		// Skip empty lines
		if (line_length(tapehead) == 0) {
			continue;
		}

		// Consume a row of sprites
		uint8_t render_data[SPRITE_SZ * SPRITE_SZ * SPRITE_COLS];
		for (int i = 0; i < SPRITE_SZ; ++i) {
			if (tapehead >= end || tapehead == NULL) {
				gvm_error("Incomplete row of sprites\n");
				return false;
			}

			if (line_length(tapehead) != SPRITE_SZ * SPRITE_COLS) {
				gvm_error("Improper line length in sprites\n");
				return false;
			}

			for (int j = 0; j < SPRITE_SZ * SPRITE_COLS; ++j) {
				char c = tapehead[j];
				uint8_t *target = &render_data[i * SPRITE_SZ * SPRITE_COLS + j];
				if (' ' == c) {
					*target = 0;
				} else if ('.' == c) {
					*target = 1;
				} else if ('o' == c) {
					*target = 2;
				} else {
					gvm_error("Unexpected character '%c'\n", c);
					return false;
				}
			}

			tapehead = next_line(tapehead);
		}

		// Give data to renderer
		if (!define_sprite_row(render_data, n_rows)) {
			return false;
		}

		++n_rows;
	}

	return true;
}

bool load_rom(const char *path)
{
	jump_count = 0;

	int src_length;
	char *src = read_file(path, &src_length);
	if (NULL == src) {
		gvm_error("Could not read %s: aborting\n", path);
		return false;
	}

	// TODO: Clean up/generalise once we have more sections
	const char header_update[] = "🐊 UPDATE";
	const char *line_update = find_line(src, header_update);
	if (line_update != src) { // required to be first line
		gvm_error("Expect '%s'\n", header_update);
		gvm_free(src);
		return false;
	}
	const char *chars_update = next_line(line_update);

	const char header_sprite[] = "🐊 SPRITE";
	const char *line_sprite = find_line(chars_update, header_sprite);
	if (line_sprite == NULL) {
		gvm_error("Expect '%s'\n", header_sprite);
		gvm_free(src);
		return false;
	}
	const char *chars_sprite = next_line(line_sprite);

	int length_update = line_sprite - chars_update;
	int length_sprite = &src[src_length] - chars_sprite;

	if (!parse_update(chars_update, length_update)) {
		gvm_free(src);
		return false;
	}

	if (!parse_sprite(chars_sprite, length_sprite)) {
		gvm_free(src);
		return false;
	}

	gvm_free(src);
	return true;
}

static bool load_state_impl(const char *src, int src_length)
{
#define TRY(name, arg_count) \
	if (!ccm_define_primitive(#name, sizeof(#name) - 1, arg_count, save_hook_ ## name)) return false

	ccm_set_logger(gvm_error);
	ccm_set_allocators(gvm_malloc, gvm_ccm_realloc_wrapper, gvm_free);

	TRY(LOAD_SCALAR, 2);
	TRY(LOAD_VEC2, 3);
	TRY(LOAD_VEC4, 5);

	return ccm_execute(src, src_length);

#undef TRY
}

bool load_state(const char *path)
{
	int src_length;
	char *src = read_file(path, &src_length);
	if (NULL == src) {
		gvm_error("Error reading file %s\n", path);
		return false;
	}

	bool success = load_state_impl(src, src_length);
	ccm_cleanup();
	gvm_free(src);
	return success;
}
