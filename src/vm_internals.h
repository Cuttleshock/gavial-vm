#ifndef VM_IMPL
	#error "Implementation file: only for VM and debug code"
#endif

#ifndef VM_INTERNALS_H
#define VM_INTERNALS_H

typedef struct {
	char *name;
	GvmConstant value;
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
	// TODO: We don't need sprite_flags. Saving flags directly in 'map' is more
	// space-efficient, faster and simpler at runtime.
	uint8_t sprite_flags[SPRITE_COLS * SPRITE_ROWS];
	uint16_t *map;
	uint32_t map_width;
	uint32_t map_height;
	bool had_error;
} vm;

#endif // VM_INTERNALS_H
