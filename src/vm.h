#ifndef VM_H
#define VM_H

#include "common.h"

typedef enum {
	// State
	OP_GET,
	OP_SET,
	// Constants
	OP_LOAD_CONST,
	// Arithmetic
	OP_ADD,
	OP_SUBTRACT,
	// Vectors
	OP_GET_X,
	OP_GET_Y,
	// Control flow
	OP_IF,
	// Booleans
	OP_LESS_THAN,
	OP_GREATER_THAN,
	// Drawing
	OP_LOAD_PAL,
	OP_CLEAR_SCREEN,
	OP_FILL_RECT,
	// Stack manipulation
	OP_SWAP,
	OP_DUP,
	OP_POP,
	OP_RETURN,
};

struct VM {
	uint8_t *instructions;
	size_t capacity;
	size_t count;
};

bool init_vm();
bool instruction(uint8_t byte);
bool run_vm();

#endif // VM_H
