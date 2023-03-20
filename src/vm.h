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
} OpCode;

typedef struct {
	const char *name;
	GvmLiteral init;
	GvmLiteral current;
	GvmValType type;
} GvmState;

struct VM {
	uint8_t *instructions;
	size_t capacity;
	size_t count;
	GvmState state[256];
	size_t state_count;
	GvmConstant stack[256];
	size_t stack_count;
	GvmConstant constants[256];
	size_t constants_count;
	bool had_error;
};

bool init_vm();
bool instruction(uint8_t byte);
bool run_vm();
bool insert_state(GvmState item, int length);
GvmState *get_state(const char *name, int length);

#endif // VM_H
