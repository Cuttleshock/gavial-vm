#include <stdlib.h>
#include <string.h>

#include "filesystem.h"
#include "vm.h"
#include "memory.h"
#include "parser.h"
#include "serialise.h"
#include "subsystems/subsystems.h"

#define VM_IMPL
#include "vm_internals.h"

#define INSTRUCTIONS_INITIAL_SIZE 256

struct VM vm;
char *queued_load_path = NULL;
bool will_save_state = false;
bool will_reload = false;

static void runtime_error(const char *message)
{
	if (!vm.had_error) {
		gvm_error("%s\n", message);
		vm.had_error = true;
	}
}

// Returns: bitfield of the map at given position (as pixel coordinates)
static bool get_map_flags(GvmConstant position)
{
	int x = vec2_get_x(position);
	int y = vec2_get_y(position);
	if (x < 0 || vm.map_width * SPRITE_SZ <= x || y < 0 || vm.map_height * SPRITE_SZ <= y) {
		return 0;
	} else {
		int map_x = x / SPRITE_SZ;
		int map_y = y / SPRITE_SZ;
		uint16_t sprite = vm.map[map_x + vm.map_width * map_y];
		return vm.sprite_flags[sprite];
	}
}

// Helper: checks if the given quad is sliding against a map tile with flags
// matching the given bit. Checks for x-sliding if !transpose, else y-sliding.
static bool is_sliding_1d(GvmConstant position, GvmConstant size, GvmConstant velocity, uint8_t bit, bool transpose)
{
	GvmConstant (*get_x)(GvmConstant) = transpose ? val_vec2_get_y : val_vec2_get_x;
	GvmConstant (*get_y)(GvmConstant) = transpose ? val_vec2_get_x : val_vec2_get_y;

	// Horizontal sliding is determined by vertical movement and vice-versa
	GvmConstant v = get_y(velocity);
	GvmConstant x0 = get_x(position);
	GvmConstant y0 = get_y(position);
	GvmConstant x1 = add_vals(x0, get_x(size));
	GvmConstant y1 = add_vals(y0, get_y(size));

	GvmConstant zero = int_to_scalar(0);
	GvmConstant grid = int_to_scalar(SPRITE_SZ);

	if (val_less_than(v, zero)) {
		GvmConstant boundary = floor_val(y0, grid);
		if (val_equal(y0, boundary)) {
			GvmConstant map_y = subtract_vals(y0, grid);
			for (GvmConstant map_x = floor_val(x0, grid); val_less_than(map_x, x1); map_x = add_vals(map_x, grid)) {
				GvmConstant map_position = val_vec2_make(transpose ? map_y : map_x, transpose ? map_x : map_y);
				uint8_t flags = get_map_flags(map_position);
				if ((bit & flags) != 0) {
					return true;
				}
			}
			return false;
		} else {
			return false;
		}
	} else if (val_greater_than(v, zero)) {
		GvmConstant boundary = ceil_val(y1, grid);
		if (val_equal(y1, boundary)) {
			GvmConstant map_y = y1;
			for (GvmConstant map_x = floor_val(x0, grid); val_less_than(map_x, x1); map_x = add_vals(map_x, grid)) {
				GvmConstant map_position = val_vec2_make(transpose ? map_y : map_x, transpose ? map_x : map_y);
				uint8_t flags = get_map_flags(map_position);
				if ((bit & flags) != 0) {
					return true;
				}
			}
			return false;
		} else {
			return false;
		}
	} else {
		// Even if we're on a grid boundary, no vertical movement means no slide
		return false;
	}
}

static bool is_sliding_x(GvmConstant position, GvmConstant size, GvmConstant velocity, uint8_t bit)
{
	return is_sliding_1d(position, size, velocity, bit, false);
}

static bool is_sliding_y(GvmConstant position, GvmConstant size, GvmConstant velocity, uint8_t bit)
{
	return is_sliding_1d(position, size, velocity, bit, true);
}

// Moves position to the next grid boundary
// Returns: time of impact (or very big value if it never hits one), and places
// position of impact in out_position
static GvmConstant move_to_boundary(GvmConstant position, GvmConstant size, GvmConstant velocity, GvmConstant *out_position)
{
	GvmConstant zero = int_to_scalar(0);
	GvmConstant grid = int_to_scalar(SPRITE_SZ);

	GvmConstant x0 = val_vec2_get_x(position);
	GvmConstant y0 = val_vec2_get_y(position);
	GvmConstant w = val_vec2_get_x(size);
	GvmConstant h = val_vec2_get_y(size);
	GvmConstant x1 = add_vals(x0, w);
	GvmConstant y1 = add_vals(y0, h);
	GvmConstant vx = val_vec2_get_x(velocity);
	GvmConstant vy = val_vec2_get_y(velocity);

	// Time to boundary along x-axis
	GvmConstant tx = int_to_scalar(INT32_MAX);
	GvmConstant pos_x = position;
	if (val_less_than(vx, zero)) {
		GvmConstant lead_x = floor_val_ex(x0, grid);
		tx = divide_vals(subtract_vals(lead_x, x0), vx);
		GvmConstant pos_x_y = add_vals(y0, multiply_vals(vy, tx));
		pos_x = val_vec2_make(lead_x, pos_x_y);
	} else if (val_greater_than(vx, zero)) {
		GvmConstant lead_x = ceil_val_ex(x1, grid);
		tx = divide_vals(subtract_vals(lead_x, x1), vx);
		GvmConstant pos_x_y = add_vals(y0, multiply_vals(vy, tx));
		pos_x = val_vec2_make(subtract_vals(lead_x, w), pos_x_y);
	}

	// Time to boundary along y-axis
	GvmConstant ty = int_to_scalar(INT32_MAX);
	GvmConstant pos_y = position;
	if (val_less_than(vy, zero)) {
		GvmConstant lead_y = floor_val_ex(y0, grid);
		ty = divide_vals(subtract_vals(lead_y, y0), vy);
		GvmConstant pos_y_x = add_vals(x0, multiply_vals(vx, ty));
		pos_y = val_vec2_make(pos_y_x, lead_y);
	} else if (val_greater_than(vy, zero)) {
		GvmConstant lead_y = ceil_val_ex(y1, grid);
		ty = divide_vals(subtract_vals(lead_y, y1), vy);
		GvmConstant pos_y_x = add_vals(x0, multiply_vals(vx, ty));
		pos_y = val_vec2_make(pos_y_x, subtract_vals(lead_y, h));
	}

	GvmConstant toi = val_less_than(tx, ty) ? tx : ty;
	// If we calculated position + toi * velocity, we might not end up exactly
	// on a grid boundary since division and multiplication are not necessarily
	// inverse operations. That's why we calculate pos_x and pos_y earlier.
	*out_position = val_less_than(tx, ty) ? pos_x : pos_y;
	return toi;
}

// Helper: moves x0 until x0 or (x0 + w) hits a grid boundary
// All arguments are scalars
// Returns: time of impact, and places position of impact in out_position
static GvmConstant slide_to_boundary_1d(GvmConstant x0, GvmConstant w, GvmConstant v, GvmConstant *out_position)
{
	GvmConstant x1 = add_vals(x0, w);
	GvmConstant zero = int_to_scalar(0);
	GvmConstant grid = int_to_scalar(SPRITE_SZ);

	if (val_less_than(v, zero)) {
		GvmConstant t0 = divide_vals(subtract_vals(floor_val_ex(x0, grid), x0), v);
		GvmConstant t1 = divide_vals(subtract_vals(floor_val_ex(x1, grid), x1), v);
		if (val_less_than(t0, t1)) {
			*out_position = floor_val_ex(x0, grid);
			return t0;
		} else {
			*out_position = subtract_vals(floor_val_ex(x1, grid), w);
			return t1;
		}
	} else if (val_greater_than(v, zero)) {
		GvmConstant t0 = divide_vals(subtract_vals(ceil_val_ex(x0, grid), x0), v);
		GvmConstant t1 = divide_vals(subtract_vals(ceil_val_ex(x1, grid), x1), v);
		if (val_less_than(t0, t1)) {
			*out_position = ceil_val_ex(x0, grid);
			return t0;
		} else {
			*out_position = subtract_vals(ceil_val_ex(x1, grid), w);
			return t1;
		}
	} else {
		*out_position = x0;
		return int_to_scalar(INT32_MAX);
	}
}

// All arguments are vec2s
// Returns: time of impact, and places position of impact in out_position
static GvmConstant slide_to_boundary_x(GvmConstant position, GvmConstant size, GvmConstant velocity, GvmConstant *out_position)
{
	GvmConstant out_1d;
	GvmConstant toi = slide_to_boundary_1d(val_vec2_get_x(position), val_vec2_get_x(size), val_vec2_get_x(velocity), &out_1d);
	*out_position = val_vec2_make(out_1d, val_vec2_get_y(position));
	return toi;
}

static GvmConstant slide_to_boundary_y(GvmConstant position, GvmConstant size, GvmConstant velocity, GvmConstant *out_position)
{
	GvmConstant out_1d;
	GvmConstant toi = slide_to_boundary_1d(val_vec2_get_y(position), val_vec2_get_y(size), val_vec2_get_y(velocity), &out_1d);
	*out_position = val_vec2_make(val_vec2_get_x(position), out_1d);
	return toi;
}

// Returns: position + t * [velocity.x, 0]
static GvmConstant slide_no_collide_x(GvmConstant position, GvmConstant velocity, GvmConstant t)
{
	GvmConstant displacement = multiply_vals(val_vec2_get_x(velocity), t);
	return val_vec2_make(add_vals(val_vec2_get_x(position), displacement), val_vec2_get_y(position));
}

static GvmConstant slide_no_collide_y(GvmConstant position, GvmConstant velocity, GvmConstant t)
{
	GvmConstant displacement = multiply_vals(val_vec2_get_y(velocity), t);
	return val_vec2_make(val_vec2_get_x(position), add_vals(val_vec2_get_y(position), displacement));
}

// Returns: position + t * velocity
static GvmConstant move_no_collide(GvmConstant position, GvmConstant velocity, GvmConstant t)
{
	GvmConstant displacement = multiply_vals(velocity, val_vec2_make(t, t));
	return add_vals(position, displacement);
}

static GvmConstant move_collide(GvmConstant position, GvmConstant size, GvmConstant velocity, uint8_t bit)
{
	GvmConstant t = int_to_scalar(1);
	GvmConstant zero = int_to_scalar(0);

	while (val_greater_than(t, zero)) {
		bool sliding_x = is_sliding_x(position, size, velocity, bit);
		bool sliding_y = is_sliding_y(position, size, velocity, bit);
		GvmConstant (*to_boundary)(GvmConstant, GvmConstant, GvmConstant, GvmConstant *) = NULL;
		GvmConstant (*no_collide)(GvmConstant, GvmConstant, GvmConstant) = NULL;

		if (sliding_x && sliding_y) {
			break;
		} else if (sliding_x) {
			to_boundary = slide_to_boundary_x;
			no_collide = slide_no_collide_x;
		} else if (sliding_y) {
			to_boundary = slide_to_boundary_y;
			no_collide = slide_no_collide_y;
		} else {
			to_boundary = move_to_boundary;
			no_collide = move_no_collide;
		}

		GvmConstant next_boundary;
		GvmConstant toi = to_boundary(position, size, velocity, &next_boundary);
		if (val_greater_than(toi, t)) {
			position = no_collide(position, velocity, t);
		} else {
			position = next_boundary;
		}
		t = subtract_vals(t, toi);
	}

	return position;
}

static GvmConstant peek()
{
	if (vm.stack_count <= 0) {
		runtime_error("Stack underflow");
		return vm.stack[0];
	}

	return vm.stack[vm.stack_count - 1];
}

static GvmConstant pop()
{
	if (vm.stack_count <= 0) {
		runtime_error("Stack underflow");
		return vm.stack[0];
	}

	return vm.stack[--vm.stack_count];
}

static void modify(GvmConstant val)
{
	if (vm.stack_count <= 0) {
		runtime_error("Stack underflow");
		return;
	}

	vm.stack[vm.stack_count - 1] = val;
}

static void push(GvmConstant val)
{
	if (vm.stack_count >= sizeof(vm.stack) / sizeof(vm.stack[0])) {
		runtime_error("Stack overflow");
		return;
	}

	vm.stack[vm.stack_count++] = val;
}

// Return value: true if should quit
static bool update()
{
#define BYTE() (vm.instructions[i++])

	vm.stack_count = 0;

	for (uint32_t i = 0; i < vm.count && !vm.had_error; ) {
		switch (BYTE()) {
			case OP_SET: {
				uint8_t index = BYTE();
				vm.state[index].value = pop();
				break;
			}
			case OP_GET: {
				uint8_t index = BYTE();
				push(vm.state[index].value);
				break;
			}
			case OP_LOAD_CONST: {
				uint8_t index = BYTE();
				push(vm.constants[index]);
				break;
			}
			case OP_ADD: {
				GvmConstant b = pop();
				GvmConstant a = peek();
				GvmConstant result = add_vals(a, b);
				modify(result);
				break;
			}
			case OP_SUBTRACT: {
				GvmConstant b = pop();
				GvmConstant a = peek();
				GvmConstant result = subtract_vals(a, b);
				modify(result);
				break;
			}
			case OP_MULTIPLY: {
				GvmConstant b = pop();
				GvmConstant a = peek();
				GvmConstant result = multiply_vals(a, b);
				modify(result);
				break;
			}
			case OP_MODULO: {
				GvmConstant b = pop();
				GvmConstant a = peek();
				modify(val_modulus(a, b));
				break;
			}
			case OP_RAND: {
				uint8_t index = BYTE();
				GvmConstant *seed = &vm.state[index].value;
				push(rand_val(seed));
				break;
			}
			case OP_RAND_INT: {
				uint8_t index = BYTE();
				GvmConstant *seed = &vm.state[index].value;
				GvmConstant max = peek();
				modify(rand_int_val(max, seed));
				break;
			}
			case OP_GET_X:
				modify(val_vec2_get_x(peek()));
				break;
			case OP_GET_Y:
				modify(val_vec2_get_y(peek()));
				break;
			case OP_MAKE_VEC2:
				GvmConstant y = pop();
				GvmConstant x = peek();
				modify(val_vec2_make(x, y));
				break;
			case OP_NORMALIZE:
				// TODO: runtime_error on zero
				modify(val_vec2_normalize(peek()));
				break;
			case OP_JUMP_IF_FALSE: {
				GvmConstant condition = pop();
				uint32_t *target = (uint32_t *)&vm.instructions[i];
				if (!condition.as.scalar) {
					i = *target;
				} else {
					i += sizeof(*target);
				}
				break;
			}
			case OP_JUMP: {
				uint32_t *target = (uint32_t *)&vm.instructions[i];
				i = *target;
				break;
			}
			case OP_LESS_THAN: {
				GvmConstant b = pop();
				GvmConstant a = peek();
				GvmConstant result = int_to_scalar(val_less_than(a, b));
				modify(result);
				break;
			}
			case OP_GREATER_THAN: {
				GvmConstant b = pop();
				GvmConstant a = peek();
				GvmConstant result = int_to_scalar(val_greater_than(a, b));
				modify(result);
				break;
			}
			case OP_NOT: {
				GvmConstant a = peek();
				modify(val_falsify(a));
				break;
			}
			case OP_AND: {
				GvmConstant b = pop();
				GvmConstant a = peek();
				modify(val_and(a, b));
				break;
			}
			case OP_OR: {
				GvmConstant b = pop();
				GvmConstant a = peek();
				modify(val_or(a, b));
				break;
			}
			case OP_BUTTON_PRESSED:
				if (button_pressed(BYTE())) {
					push(double_to_scalar(1));
				} else {
					push(double_to_scalar(0));
				}
				break;
			case OP_LOAD_PAL: {
				uint8_t bind_point = BYTE();
				uint8_t target = BYTE();
				if (!bind_palette(bind_point, target)) {
					// TODO: This should be statically checkable
					runtime_error("Failed to load palette");
				}
				break;
			}
			case OP_CAM: {
				GvmConstant where = pop();
				set_camera(vec2_get_x(where), vec2_get_y(where));
				break;
			}
			case OP_MAP_WIDTH: {
				push(double_to_scalar(vm.map_width * SPRITE_SZ));
				break;
			}
			case OP_MAP_HEIGHT: {
				push(double_to_scalar(vm.map_height * SPRITE_SZ));
				break;
			}
			case OP_MAP_FLAG: {
				uint8_t bit = 1u << BYTE();
				GvmConstant where = pop();
				uint8_t flags = get_map_flags(where);
				push(int_to_scalar((bit & flags) ? 1 : 0));
				break;
			}
			case OP_MOVE_COLLIDE: {
				GvmConstant velocity = pop();
				GvmConstant size = pop();
				GvmConstant position = peek();
				uint8_t bit = 1u << BYTE();
				modify(move_collide(position, size, velocity, bit));
				break;
			}
			case OP_FILL_RECT: {
				uint8_t palette = BYTE();
				uint8_t colour = BYTE();
				GvmConstant scale = pop();
				GvmConstant position = pop();
				if (!fill_rect(vec2_get_x(position), vec2_get_y(position), vec2_get_x(scale), vec2_get_y(scale), palette, colour)) {
					runtime_error("Failed to draw rectangle");
				}
				break;
			}
			case OP_SPRITE: {
				uint8_t sheet_x = BYTE();
				uint8_t sheet_y = BYTE();
				uint8_t palette = BYTE();
				uint8_t h_flip = BYTE();
				uint8_t v_flip = BYTE();
				GvmConstant position = pop();
				if (!sprite(vec2_get_x(position), vec2_get_y(position), sheet_x, sheet_y, palette, h_flip, v_flip)) {
					runtime_error("Failed to draw sprite");
				}
				break;
			}
			case OP_SWAP: {
				GvmConstant b = pop();
				GvmConstant a = peek();
				modify(b);
				push(a);
				break;
			}
			case OP_DUP:
				push(peek());
				break;
			case OP_POP:
				pop();
				break;
			case OP_PRINT: {
				GvmConstant val = pop();
				print_value(val);
				gvm_log("\n");
				break;
			}
			case OP_RETURN:
				break;
			default: // Unreachable if our parser works correctly
				runtime_error("Unexpected opcode");
				return true;
		}
	}

	return vm.had_error;

#undef BYTE
}

bool init_vm()
{
	vm.state_count = 0;
	vm.stack_count = 0;
	vm.constants_count = 0;
	for (int i = 0; i < sizeof(vm.sprite_flags) / sizeof(vm.sprite_flags[0]); ++i) {
		vm.sprite_flags[i] = 0;
	}
	vm.map = NULL;
	vm.had_error = false;

	vm.capacity = INSTRUCTIONS_INITIAL_SIZE;
	vm.instructions = gvm_malloc(vm.capacity);
	vm.count = 0;

	if (vm.instructions != NULL) {
		return true;
	} else {
		return false;
	}
}

bool queue_load(const char *path)
{
	if (NULL != queued_load_path) {
		gvm_free(queued_load_path);
		queued_load_path = NULL;
	}

	queued_load_path = gvm_malloc(strlen(path) + 1);
	if (NULL == queued_load_path) {
		gvm_error("Could not load save file (queueing failed)\n");
		return false;
	}

	strcpy(queued_load_path, path);
	return true;
}

void queue_save()
{
	will_save_state = true;
}

void queue_reload()
{
	will_reload = true;
}

// Return value: true if named state is defined
// If true, *index contains its location; else, the proper insertion location
// to maintain order
// TODO: Again, remove 'length' arg as soon as we clean up CCM strings
static bool locate_state(const char *name, int name_length, int *index)
{
	int low = 0;
	int high = vm.state_count - 1;
	int current = low + high / 2;
	for (; low <= high; current = (low + high) / 2) {
		int cmp = strncmp(name, vm.state[current].name, name_length);
		if (cmp < 0) {
			high = current - 1;
		} else if (cmp > 0) {
			low = current + 1;
		} else {
			*index = current;
			return true;
		}
	}
	*index = low;
	return false;
}

// Insert variable in state list, keeping it sorted by name. Copies name to heap.
// Returns true on successful insertion
bool define_state(GvmConstant value, const char *name)
{
	if (vm.state_count >= 256) {
		return false;
	}

	int length = strlen(name);
	int index;
	bool found = locate_state(name, length, &index);
	if (found) {
		return false;
	}

	char *own_name = gvm_malloc(length + 1);
	if (NULL == own_name) {
		return false;
	}

	memcpy(own_name, name, length);
	own_name[length] = '\0';

	// Move all items one to the right
	for (int i = vm.state_count; i > index; --i) {
		vm.state[i] = vm.state[i - 1];
	}

	GvmState *state = &vm.state[index];
	state->name = own_name;
	state->value = value;
	++vm.state_count;

	return true;
}

// Returns: success
bool set_state(GvmConstant value, const char *name, int length)
{
	int index;
	bool found = locate_state(name, length, &index);
	if (!found) {
		return false;
	} else if (vm.state[index].value.type != value.type) {
		return false;
	} else {
		vm.state[index].value = value;
		return true;
	}
}

// Returns: success
bool instruction(uint8_t byte)
{
	if (vm.count >= vm.capacity) {
		uint32_t old_capacity = vm.capacity;
		vm.capacity = 2 * old_capacity;
		vm.instructions = gvm_realloc(vm.instructions, old_capacity, vm.capacity);
		if (NULL == vm.instructions) {
			return false;
		}
	}

	vm.instructions[vm.count++] = byte;
	return true;
}

// Returns type of named state if it exists (as checked by state_instruction or
// some other method), else undefined
ValueType state_type(const char *name, int length)
{
	int index;
	bool found = locate_state(name, length, &index);
	if (!found) {
		return VAL_SCALAR;
	} else {
		return vm.state[index].value.type;
	}
}

bool state_instruction(uint8_t byte, const char *name, int length)
{
	int index;
	bool found = locate_state(name, length, &index);
	if (!found) {
		return false;
	} else {
		instruction(byte);
		instruction(index);
		return true;
	}
}

// Returns: success. On success, out_index is a valid argument for resolve_jump().
// TODO: May be better to explicitly bit-shift to form jumps, or even to give
// them a dedicated separate array - not nice to read misaligned int32s
bool jump(uint8_t byte, uint32_t *out_index)
{
#define TRY(op) if (!instruction(op)) return false

	TRY(byte);
	*out_index = vm.count;
	// (Carefully) leave four bytes for resolved operand
	TRY(0);
	TRY(0);
	TRY(0);
	TRY(0);
	return true;

#undef TRY
}

bool resolve_jump(uint32_t index)
{
	if (index >= vm.count) {
		return false;
	}

	uint32_t *target = (uint32_t *)&vm.instructions[index];
	*target = vm.count;
	return true;
}

// Adds a value to the constant table and emits instruction to load it
// Returns: success
bool constant(GvmConstant value)
{
	if (vm.constants_count >= 256) {
		return false;
	}

	if (!instruction(OP_LOAD_CONST)) {
		return false;
	}

	vm.constants[vm.constants_count] = value;
	return instruction(vm.constants_count++);
}

void set_sprite_flags(uint8_t flags, int index)
{
	vm.sprite_flags[index] = flags;
}

bool set_introspection_map(const uint8_t (*map)[4], int width, int height)
{
	vm.map_width = width;
	vm.map_height = height;
	vm.map = gvm_malloc(width * height * sizeof(*vm.map));
	if (NULL == vm.map) {
		return false;
	}

	for (int i = 0; i < width * height; ++i) {
		vm.map[i] = map[i][0] + map[i][1] * SPRITE_COLS;
	}

	return true;
}

void close_vm()
{
	gvm_free(vm.instructions);
	gvm_free(vm.map);
	for (int i = 0; i < vm.state_count; ++i) {
		gvm_free(vm.state[i].name);
	}
}

// Return value: false if an error occurred preventing execution
bool run_vm(const char *rom_path)
{
	time_t rom_timestamp = last_modified(rom_path);
	if (rom_timestamp == MODIFY_ERROR) {
		gvm_error("Cannot watch ROM %s for updates\n", rom_path);
	}
	bool loop_done = false;

	while (!loop_done) {
		// Check for queued save
		if (will_save_state) {
			will_save_state = false;
			if (!begin_serialise(rom_path)) {
				gvm_error("Could not open file to save state\n");
			} else {
				for (int i = 0; i < vm.state_count; ++i) {
					if (!serialise(vm.state[i].name, vm.state[i].value)) {
						gvm_error("Error serialising state\n");
						break;
					}
				}
				if (!end_serialise()) {
					gvm_error("Could not flush serialised state\n");
				}
			}
		}

		// Check for queued reload
		if (will_reload) {
			will_reload = false;
			struct VM old_vm = vm;
			init_vm();
			if (!load_rom(rom_path)) {
				gvm_error("Error reloading ROM %s\n", rom_path);
				close_vm();
				vm = old_vm;
			} else {
				struct VM new_vm = vm;
				vm = old_vm;
				close_vm();
				vm = new_vm;
			}
			input();
		}

		// Re-parse ROM if appropriate
		if (rom_timestamp != MODIFY_ERROR) {
			time_t new_timestamp = last_modified(rom_path);
			if (new_timestamp == MODIFY_ERROR) {
				gvm_error("Cannot watch ROM %s for updates\n", rom_path);
				rom_timestamp = new_timestamp;
			} else if (new_timestamp != rom_timestamp) {
				gvm_log("ROM %s modified: reparsing\n", rom_path);
				rom_timestamp = new_timestamp;
				struct VM old_vm = vm;
				init_vm();
				if (!load_rom(rom_path)) {
					gvm_error("Error parsing updated ROM %s\n", rom_path);
					close_vm();
					vm = old_vm;
				} else {
					// Recover old state
					struct VM new_vm = vm;
					vm = old_vm;
					for (int i = 0; i < new_vm.state_count; ++i) {
						int index;
						const char *name = new_vm.state[i].name;
						if (locate_state(name, strlen(name), &index)) {
							if (vm.state[index].value.type == new_vm.state[i].value.type) {
								new_vm.state[i].value = vm.state[index].value;
							}
						}
					}
					close_vm();
					vm = new_vm;
				}
				input();
			}
		}

		// Check for queued load
		if (NULL != queued_load_path) {
			gvm_log("Loading save %s...\n", queued_load_path);
			if (!load_state(queued_load_path)) {
				gvm_error("Error loading save file\n");
			}
			gvm_free(queued_load_path);
			queued_load_path = NULL;
			// TODO: Freeze to aid in buffered input
			input();
		}

		// Run VM loop
		// TODO: lock_input()?
		loop_done = update();
		input();
		draw(); // TODO: Unlink vsync from update logic
		loop_done = loop_done || window_should_close();
	}
	return !vm.had_error;
}
