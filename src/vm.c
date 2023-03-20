#include <stdlib.h>
#include <string.h>

#include "vm.h"
#include "memory.h"
#include "subsystems.h"

#define INSTRUCTIONS_INITIAL_SIZE 256

struct VM vm;

bool init_vm()
{
	vm.instructions = gvm_malloc(INSTRUCTIONS_INITIAL_SIZE);
	vm.capacity = INSTRUCTIONS_INITIAL_SIZE;
	vm.count = 0;
	if (!vm.instructions) {
		return false;
	}

	if (!init_subsystems()) {
		gvm_free(vm.instructions);
		return false;
	}

	return true;
}

static void close_vm()
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

// Return value: true if should quit
static bool update()
{
	for (int i = 0; i < vm.count; ++i) {
		switch (vm.instructions[i]) {
			case OP_SET:
			case OP_GET:
			case OP_LOAD_CONST:
			case OP_ADD:
			case OP_SUBTRACT:
			case OP_GET_X:
			case OP_GET_Y:
			case OP_IF:
			case OP_LESS_THAN:
			case OP_GREATER_THAN:
			case OP_LOAD_PAL:
			case OP_CLEAR_SCREEN:
			case OP_FILL_RECT:
			case OP_SWAP:
			case OP_DUP:
			case OP_POP:
			case OP_RETURN:
				return false;
			default: // Unreachable if our parser works correctly
				gvm_error("Unexpected opcode %2x\n", vm.instructions[i]);
				return true;
		}
	}
	return false;
}

// Return value: false if an error occurred preventing execution
bool run_vm()
{
	bool loop_done = false;

	while (!loop_done) {
		loop_done = update();
		loop_done = loop_done || run_subsystems();
	}

	close_vm();
	close_subsystems();
	return true;
}
