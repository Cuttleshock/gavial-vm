#ifndef VM_H
#define VM_H

#include "common.h"

#define INSTRUCTIONS_MAX 256

enum {
	OP_RETURN,
};

extern uint8_t instructions[INSTRUCTIONS_MAX];
extern size_t instruction_count;

bool instruction(uint8_t byte);

#endif // VM_H
