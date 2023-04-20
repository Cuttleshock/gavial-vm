#ifndef VM_IMPL
	#error "Implementation file: only for VM and debug code"
#endif

#ifndef VM_INTERNALS_H
#define VM_INTERNALS_H

typedef struct {
	char *name;
	GvmLiteral init;
	GvmLiteral current;
	GvmValType type;
} GvmState;

extern struct VM {
	uint8_t *instructions;
	uint32_t capacity;
	uint32_t count;
	GvmState state[256];
	uint32_t state_count;
	GvmConstant stack[256];
	uint32_t stack_count;
	GvmConstant constants[256];
	uint32_t constants_count;
	bool had_error;
} vm;

#endif // VM_INTERNALS_H
