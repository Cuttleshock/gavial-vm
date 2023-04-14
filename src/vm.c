#include <stdlib.h>
#include <string.h>

#include "vm.h"
#include "memory.h"
#include "subsystems/subsystems.h"

#define INSTRUCTIONS_INITIAL_SIZE 256

struct VM vm;

// TODO: We may be able to get away without init() and close(): statically
// allocate instructions at first, but grow them dynamically when needed...
bool init_vm()
{
	vm.instructions = gvm_malloc(INSTRUCTIONS_INITIAL_SIZE);
	vm.capacity = INSTRUCTIONS_INITIAL_SIZE;
	vm.count = 0;
	return (vm.instructions != NULL);
}

// TODO: ... then free() them if needed after run_vm()
void close_vm()
{
	gvm_free(vm.instructions);
}

// Binary search for a named variable in VM's state list
// If not found, index is where it should be inserted to maintain order
static struct location {
	bool found;
	int index;
} locate_state(const char *name, int length)
{
	int low = 0;
	int high = vm.state_count - 1;
	int current = low + high / 2;
	for (; low <= high; current = (low + high) / 2) {
		int cmp = strncmp(name, vm.state[current].name, length);
		if (cmp < 0) {
			high = current - 1;
		} else if (cmp > 0) {
			low = current + 1;
		} else {
			return (struct location){ true, current };
		}
	}
	return (struct location){ false, low };
}

// Insert variable in state list, keeping it sorted by name
// Returns true on successful insertion
bool insert_state(GvmState item, int length)
{
	if (vm.state_count >= 256) {
		return false;
	}

	struct location loc = locate_state(item.name, length);
	if (loc.found) {
		return false;
	}

	// Move all items one to the right
	for (int i = vm.state_count; i > loc.index; --i) {
		vm.state[i] = vm.state[i - 1];
	}
	vm.state[loc.index] = item;
	++vm.state_count;
	return true;
}

// Binary search for a named variable in VM's state list
GvmState *get_state(const char *name, int length)
{
	struct location loc = locate_state(name, length);
	if (loc.found) {
		return &vm.state[loc.index];
	} else {
		return NULL;
	}
}

bool instruction(uint8_t byte)
{
	if (vm.count >= vm.capacity) {
		vm.instructions = gvm_realloc(vm.instructions, vm.capacity, 2 * vm.capacity);
		if (!vm.instructions) {
			return false;
		}
	}

	vm.instructions[vm.count++] = byte;
	return true;
}

bool constant(GvmConstant value)
{
	if (vm.constants_count >= 256) {
		return false;
	}

	vm.constants[vm.constants_count++] = value;
	return true;
}

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
	vm.stack_count = 0;

	for (int i = 0; i < vm.count && !vm.had_error; ++i) {
		switch (vm.instructions[i]) {
			case OP_SET: {
				uint8_t index = vm.instructions[++i];
				GvmConstant val = pop();
				vm.state[index].current = val.as;
				continue;
			}
			case OP_GET: {
				uint8_t index = vm.instructions[++i];
				push((GvmConstant){ vm.state[index].current, vm.state[index].type });
				continue;
			}
			case OP_LOAD_CONST: {
				uint8_t index = vm.instructions[++i];
				push(vm.constants[index]);
				continue;
			}
			case OP_ADD: {
				GvmConstant b = pop();
				GvmConstant a = peek();
				GvmConstant result = add_vals(a, b);
				modify(result);
				continue;
			}
			case OP_SUBTRACT: {
				GvmConstant b = pop();
				GvmConstant a = peek();
				GvmConstant result = subtract_vals(a, b);
				modify(result);
				continue;
			}
			case OP_GET_X:
				modify(SCAL(peek().as.vec2[0]));
				continue;
			case OP_GET_Y:
				modify(SCAL(peek().as.vec2[1]));
				continue;
			case OP_IF: // TODO
				runtime_error("OP_IF: Unimplemented");
				continue;
			case OP_LESS_THAN: {
				GvmConstant b = pop();
				GvmConstant a = peek();
				GvmConstant result = val_less_than(a, b);
				modify(result);
				continue;
			}
			case OP_GREATER_THAN: {
				GvmConstant b = pop();
				GvmConstant a = peek();
				GvmConstant result = val_greater_than(a, b);
				modify(result);
				continue;
			}
			case OP_LOAD_PAL: {
				uint8_t bind_point = vm.instructions[++i];
				uint8_t target = vm.instructions[++i];
				if (!bind_palette(bind_point, target)) {
					// TODO: This should be statically checkable
					runtime_error("Failed to load palette");
				}
				continue;
			}
			case OP_FILL_RECT: {
				uint8_t palette = vm.instructions[++i];
				uint8_t colour = vm.instructions[++i];
				GvmConstant scale = pop();
				GvmConstant position = pop();
				if (!fill_rect(V2X(position), V2Y(position), V2X(scale), V2Y(scale), palette, colour)) {
					runtime_error("Failed to draw rectangle");
				}
				continue;
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
}

// Return value: false if an error occurred preventing execution
bool run_vm()
{
	bool loop_done = false;

	while (!loop_done) {
		input();
		loop_done = update();
		draw(); // TODO: Unlink vsync from update logic
		loop_done = loop_done || window_should_close();
	}
	return true;
}
