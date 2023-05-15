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
	OP_JUMP_IF_FALSE,
	OP_JUMP,
	// Booleans
	OP_LESS_THAN,
	OP_GREATER_THAN,
	OP_NOT,
	OP_AND, // does not short-circuit
	// Input
	OP_BUTTON_PRESSED,
	// Drawing
	OP_LOAD_PAL,
	OP_CAM,
	OP_MAP_WIDTH,
	OP_MAP_HEIGHT,
	OP_MAP_FLAG,
	OP_FILL_RECT,
	OP_SPRITE,
	// Stack manipulation
	OP_SWAP,
	OP_DUP,
	OP_POP,
	OP_PRINT,
	OP_RETURN,
} OpCode;

bool init_vm();
bool queue_load(const char *path);
void queue_save();
void queue_reload();
bool run_vm(const char *rom_path);
void close_vm();
void set_sprite_flags(uint8_t flags, int index);
bool set_introspection_map(const uint8_t (*map)[4], int width, int height);
bool set_state(GvmConstant value, const char *name, int length);
bool instruction(uint8_t byte);
bool state_instruction(uint8_t byte, const char *name, int length);
bool jump(uint8_t byte, uint32_t *out_index);
bool resolve_jump(uint32_t index);
bool constant(GvmConstant value);
bool define_state(GvmConstant value, const char *name);

#endif // VM_H
