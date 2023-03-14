#include "vm.h"

uint8_t instructions[INSTRUCTIONS_MAX];
size_t instruction_count;

bool instruction(uint8_t byte)
{
	if (instruction_count >= INSTRUCTIONS_MAX) {
		return false;
	} else {
		instructions[instruction_count++] = byte;
		return true;
	}
}
