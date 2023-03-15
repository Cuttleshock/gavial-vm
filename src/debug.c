#include <stdio.h>

#include "common.h"
#include "debug.h"

// Declare 'private' VM components so we don't have to expose them to non-debug code
extern uint8_t *instructions;
extern size_t instruction_count;

static void disassemble_instruction(uint8_t byte)
{
	gvm_log("%2x\n", byte);
}

void disassemble()
{
	for (int i = 0; i < instruction_count; ++i) {
		disassemble_instruction(instructions[i]);
	}
}
