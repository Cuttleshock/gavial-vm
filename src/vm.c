#include <stdlib.h>

#include "vm.h"
#include "memory.h"

#define INSTRUCTIONS_INITIAL_SIZE 256

uint8_t *instructions;
size_t instruction_capacity;
size_t instruction_count;

bool init_vm()
{
	instructions = gvm_malloc(INSTRUCTIONS_INITIAL_SIZE);
	instruction_capacity = INSTRUCTIONS_INITIAL_SIZE;
	instruction_count = 0;
	if (!instructions) {
		return false;
	}

	return true;
}

void close_vm()
{
	gvm_free(instructions);
}

bool instruction(uint8_t byte)
{
	if (instruction_count >= instruction_capacity) {
		instructions = gvm_realloc(instructions, instruction_capacity, 2 * instruction_capacity);
		if (!instructions) {
			return false;
		}
	}

	instructions[instruction_count++] = byte;
	return true;
}
