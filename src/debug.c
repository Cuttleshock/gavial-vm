#include <stdio.h>

#include "common.h"
#include "debug.h"
#include "vm.h"

// Declare 'private' VM components so we don't have to expose them to non-debug code
extern struct VM vm;

static void disassemble_instruction(uint8_t byte)
{
	gvm_log("%2x\n", byte);
}

void disassemble()
{
	for (int i = 0; i < vm.count; ++i) {
		disassemble_instruction(vm.instructions[i]);
	}
}
