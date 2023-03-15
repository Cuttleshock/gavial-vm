#include <stdio.h>

#include "common.h"
#include "debug.h"

extern uint8_t *instructions;
extern size_t instruction_count;

void disassemble_instruction(uint8_t byte)
{
	printf("%2x\n", byte);
}

void disassemble()
{
	for (int i = 0; i < instruction_count; ++i) {
		disassemble_instruction(instructions[i]);
	}
}
