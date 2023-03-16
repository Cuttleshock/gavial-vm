#ifndef VM_H
#define VM_H

#include "common.h"

enum {
	OP_RETURN,
};

bool init_vm();
bool instruction(uint8_t byte);
bool run_vm();

#endif // VM_H
