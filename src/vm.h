#ifndef VM_H
#define VM_H

#include "common.h"

enum {
	OP_RETURN,
};

bool init_vm();
void close_vm();
bool instruction(uint8_t byte);

#endif // VM_H
