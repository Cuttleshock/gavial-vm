#include <stdlib.h>
#include <string.h>

#include "filesystem.h"
#include "vm.h"
#include "memory.h"
#include "parser.h"
#include "subsystems/subsystems.h"

#define VM_IMPL
#include "vm_internals.h"

#define INSTRUCTIONS_INITIAL_SIZE 256

struct VM vm;
char *queued_load_path = NULL;

static void runtime_error(const char *message)
{
	if (!vm.had_error) {
		gvm_error("%s\n", message);
		vm.had_error = true;
	}
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
	if (vm.stack_count >= 256) {
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
				GvmConstant val = pop();
				vm.state[index].current = val.as;
				break;
			}
			case OP_GET: {
				uint8_t index = BYTE();
				push((GvmConstant){ vm.state[index].current, vm.state[index].type });
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
			case OP_MODULO: {
				GvmConstant b = pop();
				GvmConstant a = peek();
				modify(SCAL(a.as.scalar % b.as.scalar));
				break;
			}
			case OP_GET_X:
				modify(SCAL(peek().as.vec2[0]));
				break;
			case OP_GET_Y:
				modify(SCAL(peek().as.vec2[1]));
				break;
			case OP_MAKE_VEC2:
				GvmConstant y = pop();
				GvmConstant x = peek();
				modify(VEC2(x.as.scalar, y.as.scalar));
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
				GvmConstant result = val_less_than(a, b);
				modify(result);
				break;
			}
			case OP_GREATER_THAN: {
				GvmConstant b = pop();
				GvmConstant a = peek();
				GvmConstant result = val_greater_than(a, b);
				modify(result);
				break;
			}
			case OP_BUTTON_PRESSED:
				if (button_pressed(BYTE())) {
					push(SCAL(1));
				} else {
					push(SCAL(0));
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
			case OP_FILL_RECT: {
				uint8_t palette = BYTE();
				uint8_t colour = BYTE();
				GvmConstant scale = pop();
				GvmConstant position = pop();
				if (!fill_rect(V2X(position), V2Y(position), V2X(scale), V2Y(scale), palette, colour)) {
					runtime_error("Failed to draw rectangle");
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
			case OP_PRINT:
				print_value(pop());
				gvm_log("\n");
				break;
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
	vm.had_error = false;

	vm.capacity = INSTRUCTIONS_INITIAL_SIZE;
	vm.instructions = gvm_malloc(vm.capacity);
	vm.count = 0;

	return vm.instructions != NULL;
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

// Return value: true if named state is defined
// If true, *index contains its location; else, the proper insertion location
// to maintain order
// TODO: Again, remove 'length' arg as soon as we clean up CCM strings
bool locate_state(const char *name, int name_length, int *index)
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
	state->init = value.as;
	state->current = value.as;
	state->type = value.type;
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
	} else if (vm.state[index].type != value.type) {
		return false;
	} else {
		vm.state[index].current = value.as;
		return true;
	}
}

// Returns: success
bool instruction(uint8_t byte)
{
	if (vm.count >= vm.capacity) {
		vm.instructions = gvm_realloc(vm.instructions, vm.capacity, 2 * vm.capacity);
		if (NULL == vm.instructions) {
			return false;
		}
	}

	vm.instructions[vm.count++] = byte;
	return true;
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

void close_vm()
{
	gvm_free(vm.instructions);
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
	int time_index; // TODO: very careful
	locate_state("Time", 4, &time_index);

	while (!loop_done) {
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
				if (!parse(rom_path)) {
					gvm_error("Error parsing updated ROM %s\n", rom_path);
					close_vm();
					vm = old_vm;
				} else {
					locate_state("Time", 4, &time_index);
					struct VM new_vm = vm;
					vm = old_vm;
					for (int i = 0; i < new_vm.state_count; ++i) {
						int index;
						const char *name = new_vm.state[i].name;
						if (locate_state(name, strlen(name), &index)) {
							if (vm.state[index].type == new_vm.state[i].type) {
								new_vm.state[i].current = vm.state[index].current;
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
		++vm.state[time_index].current.scalar;
		loop_done = update();
		input();
		draw(); // TODO: Unlink vsync from update logic
		loop_done = loop_done || window_should_close();
	}
	return !vm.had_error;
}
