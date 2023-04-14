#include <stdio.h>

#include "common.h"
#include "debug.h"
#include "value.h"
#include "vm.h"

#define VM_IMPL
#include "vm_internals.h"

static int disassemble_instruction(int i)
{
#define CASE(op) case op: gvm_log(#op "\n"); return ++i;
#define CASE_BYTE(op) case op: gvm_log(#op " %2x\n", vm.instructions[++i]); return ++i;
#define CASE_2BYTE(op) case op: \
	gvm_log(#op " %2x %2x\n", vm.instructions[i + 1], vm.instructions[i + 2]); \
	return ++i;

	OpCode instruction = vm.instructions[i];
	switch (instruction) {
		CASE_BYTE(OP_GET);
		CASE_BYTE(OP_SET);
		CASE_BYTE(OP_LOAD_CONST);
		CASE(OP_ADD);
		CASE(OP_SUBTRACT);
		CASE(OP_MODULO);
		CASE(OP_GET_X);
		CASE(OP_GET_Y);
		CASE(OP_MAKE_VEC2);
		CASE_BYTE(OP_IF);
		CASE(OP_LESS_THAN);
		CASE(OP_GREATER_THAN);
		CASE_2BYTE(OP_LOAD_PAL);
		CASE_2BYTE(OP_FILL_RECT);
		CASE(OP_SWAP);
		CASE(OP_DUP);
		CASE(OP_POP);
		CASE(OP_PRINT);
		CASE(OP_RETURN);
		default:
			gvm_log("UNKNOWN INSTRUCTION %2x\n", instruction);
			return ++i;
	}

#undef CASE_2BYTE
#undef CASE_BYTE
#undef CASE
}

void disassemble()
{
	printf("🐊 State\n");
	for (int i = 0; i < vm.state_count; ++i) {
		printf("%s: ", vm.state[i].name);
		print_value(CONSTANT(vm.state[i].init, vm.state[i].type));
		printf(" -> ");
		print_value(CONSTANT(vm.state[i].current, vm.state[i].type));
		printf("\n");
	}

	printf("🐊 Update\n");
	for (int i = 0; i < vm.count; i = disassemble_instruction(i)) {}
}
