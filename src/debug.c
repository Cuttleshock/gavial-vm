#include <stdio.h>

#include "common.h"
#include "debug.h"
#include "value.h"
#include "vm.h"

#define VM_IMPL
#include "vm_internals.h"

static int disassemble_instruction(int i)
{
#define CASE(op) case op: gvm_log(#op "\n"); return ++i
#define CASE_BYTE(op) case op: gvm_log(#op " %02x\n", vm.instructions[++i]); return ++i
#define CASE_2BYTE(op) case op: \
	gvm_log(#op " %02x %02x\n", vm.instructions[i + 1], vm.instructions[i + 2]); \
	return i += 3
#define CASE_3BYTE(op) case op: \
	gvm_log(#op " %02x %02x %02x\n", vm.instructions[i + 1], vm.instructions[i + 2], vm.instructions[i + 3]); \
	return i += 4
#define CASE_5BYTE(op) case op: \
	gvm_log(#op " %02x %02x %02x %02x %02x\n", vm.instructions[i + 1], vm.instructions[i + 2], vm.instructions[i + 3], vm.instructions[i + 4], vm.instructions[i + 5]); \
	return i += 6
#define CASE_32BIT(op) case op: \
	gvm_log(#op " %d\n", *((uint32_t *)&vm.instructions[i + 1])); \
	return i += 5

	gvm_log("%4d ", i);
	OpCode instruction = vm.instructions[i];
	switch (instruction) {
		CASE_BYTE(OP_GET);
		CASE_BYTE(OP_SET);
		CASE_BYTE(OP_LOAD_CONST);
		CASE(OP_ADD);
		CASE(OP_SUBTRACT);
		CASE(OP_MULTIPLY);
		CASE(OP_MODULO);
		CASE(OP_RAND);
		CASE(OP_RAND_INT);
		CASE(OP_GET_X);
		CASE(OP_GET_Y);
		CASE(OP_MAKE_VEC2);
		CASE(OP_NORMALIZE);
		CASE_32BIT(OP_JUMP_IF_FALSE);
		CASE_32BIT(OP_JUMP);
		CASE(OP_LESS_THAN);
		CASE(OP_GREATER_THAN);
		CASE(OP_NOT);
		CASE(OP_AND);
		CASE(OP_OR);
		CASE_BYTE(OP_BUTTON_PRESSED);
		CASE_2BYTE(OP_LOAD_PAL);
		CASE(OP_CAM);
		CASE(OP_MAP_WIDTH);
		CASE(OP_MAP_HEIGHT);
		CASE_BYTE(OP_MAP_FLAG);
		CASE_BYTE(OP_MOVE_COLLIDE);
		CASE_2BYTE(OP_FILL_RECT);
		CASE_5BYTE(OP_SPRITE);
		CASE(OP_SWAP);
		CASE(OP_DUP);
		CASE(OP_POP);
		CASE(OP_PRINT);
		CASE(OP_RETURN);
		default:
			gvm_log("UNKNOWN INSTRUCTION 0x%2x\n", instruction);
			return ++i;
	}

#undef CASE_32BIT
#undef CASE_5BYTE
#undef CASE_3BYTE
#undef CASE_2BYTE
#undef CASE_BYTE
#undef CASE
}

void disassemble()
{
	gvm_log("🐊 State\n");
	for (int i = 0; i < vm.state_count; ++i) {
		gvm_log("%s: ", vm.state[i].name);
		print_value(vm.state[i].value);
		gvm_log("\n");
	}

	gvm_log("🐊 Update\n");
	for (int i = 0; i < vm.count; i = disassemble_instruction(i)) {}
}
