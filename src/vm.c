#include <stdlib.h>
#include <string.h>

#include "vm.h"
#include "memory.h"
#include "subsystems/subsystems.h"

#define VM_IMPL
#include "vm_internals.h"

#define INSTRUCTIONS_INITIAL_SIZE 256

struct VM vm;

static GvmState auto_state[] = {
	{ "Camera", LIT_VEC2(0, 0), LIT_VEC2(0, 0), VAL_VEC2 },
	{ "Time", LIT_SCAL(0), LIT_SCAL(0), VAL_SCALAR },
};

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
			case OP_MODULO: {
				GvmConstant b = pop();
				GvmConstant a = peek();
				modify(SCAL(a.as.scalar % b.as.scalar));
				break;
			}
			case OP_GET_X:
				modify(SCAL(peek().as.vec2[0]));
				continue;
			case OP_GET_Y:
				modify(SCAL(peek().as.vec2[1]));
				continue;
			case OP_MAKE_VEC2:
				GvmConstant y = pop();
				GvmConstant x = peek();
				modify(VEC2(x.as.scalar, y.as.scalar));
				break;
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
			case OP_BUTTON_PRESSED:
				if (button_pressed(vm.instructions[++i])) {
					push(SCAL(1));
				} else {
					push(SCAL(0));
				}
				continue;
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

// TODO: We may be able to get away without init() and close(): statically
// allocate instructions at first, but grow them dynamically when needed...
bool init_vm()
{
	vm.instructions = gvm_malloc(INSTRUCTIONS_INITIAL_SIZE);
	vm.capacity = INSTRUCTIONS_INITIAL_SIZE;
	vm.count = 0;

	if (vm.instructions == NULL) {
		return false;
	}

	// Copy stack state to heap
	for (int i = 0; i < sizeof(auto_state) / sizeof(auto_state[0]); ++i) {
		GvmConstant constant = { auto_state[i].init, auto_state[i].type };
		if (!define_state(constant, auto_state[i].name)) {
			close_vm(); // TODO: careful if we change the implementation of close_vm()
			return false;
		}
	}

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

// Adds a value to the constant table
// Returns: its index (good for OP_LOAD_CONST), or -1 on failure
int constant(GvmConstant value)
{
	if (vm.constants_count >= 256) {
		return -1;
	}

	vm.constants[vm.constants_count] = value;
	return vm.constants_count++;
}

// TODO: ... then free() them if needed after run_vm()
void close_vm()
{
	gvm_free(vm.instructions);
	for (int i = 0; i < vm.state_count; ++i) {
		gvm_free(vm.state[i].name);
	}
}

// Return value: false if an error occurred preventing execution
bool run_vm()
{
	bool loop_done = false;
	int time_index; // TODO: very careful
	locate_state("Time", 4, &time_index);

	while (!loop_done) {
		input();
		++vm.state[time_index].current.scalar;
		loop_done = update();
		draw(); // TODO: Unlink vsync from update logic
		loop_done = loop_done || window_should_close();
	}
	return !vm.had_error;
}
