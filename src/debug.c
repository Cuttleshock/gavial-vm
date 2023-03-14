#include <stdio.h>

#include "debug.h"
#include "vm.h"

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
