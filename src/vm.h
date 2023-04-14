#ifndef VM_H
#define VM_H

#include "common.h"
#include "value.h"

typedef enum {
	// State
	OP_GET,
	OP_SET,
	// Constants
	OP_LOAD_CONST,
	// Arithmetic
	OP_ADD,
	OP_SUBTRACT,
	OP_MODULO,
	// Vectors
	OP_GET_X,
	OP_GET_Y,
	OP_MAKE_VEC2,
	// Control flow
	OP_IF,
	// Booleans
	OP_LESS_THAN,
	OP_GREATER_THAN,
	// Drawing
	OP_LOAD_PAL,
	OP_FILL_RECT,
	// Stack manipulation
	OP_SWAP,
	OP_DUP,
	OP_POP,
	OP_PRINT,
	OP_RETURN,
} OpCode;

bool init_vm();
bool run_vm();
void close_vm();
bool instruction(uint8_t byte);
int constant(GvmConstant value);
bool locate_state(const char *name, int name_length, int *index);
bool define_state(GvmConstant value, const char *name);
bool set_state(GvmConstant value, const char *name);

#endif // VM_H
