#include <string.h>

#include "common.h"
#include "parser.h"
#include "value.h"
#include "vm.h"

static GvmState dummy_state[] = {
	{ "snakes", LIT_SCAL(0), LIT_SCAL(5), VAL_SCALAR },
	{ "badgers", LIT_VEC2(0, 0), LIT_VEC2(10, 20), VAL_VEC2 },
	{ "dogs", LIT_VEC4(0, 0, 0, 0), LIT_VEC4(3, 3, 2, 2), VAL_VEC4 },
	{ NULL, LIT_SCAL(0), LIT_SCAL(0), VAL_NONE },
};

static GvmState implicit_state[] = {
	{ "Camera", LIT_VEC2(0, 0), LIT_VEC2(0, 0), VAL_VEC2 },
	{ "Time", LIT_SCAL(0), LIT_SCAL(0), VAL_SCALAR },
	{ "Pal0", LIT_SCAL(0), LIT_SCAL(0), VAL_SCALAR },
};

static void parse_error(const char *message)
{
	gvm_error("%s\n", message);
}

// TODO: Read a line from the source file and transform to state
static GvmState consume_state()
{
	static int line = 0;
	return dummy_state[line++];
}

static bool parse_state()
{
	// Read all state declared in source file
	for (GvmState item = consume_state(); item.type != VAL_NONE; item = consume_state()) {
		if (!insert_state(item, strlen(item.name))) {
			parse_error("Failed to add state");
			return false;
		}
	}

	// Insert implicit state if not already set
	for (int i = 0; i < sizeof(implicit_state) / sizeof(implicit_state[0]); ++i) {
		GvmState implicit = implicit_state[i];
		GvmState *explicit = get_state(implicit.name, strlen(implicit.name));
		if (explicit != NULL) {
			if (explicit->type != implicit.type) {
				parse_error("State set with invalid type");
				return false;
			}
		} else {
			if (!insert_state(implicit, strlen(implicit.name))) {
				parse_error("Failed to add state");
				return false;
			}
		}
	}

	return true;
}

static bool parse_update()
{
	constant(SCAL(3));
	instruction(OP_LOAD_CONST);
	instruction(0);
	constant(SCAL(5));
	instruction(OP_LOAD_CONST);
	instruction(1);
	instruction(OP_ADD);
	instruction(OP_PRINT);
	instruction(OP_CLEAR_SCREEN);
	instruction(2);
	instruction(3);
	instruction(OP_RETURN);

	return true;
}

bool parse()
{
#define TRY(p) if (!p) return false;

	TRY(parse_state());
	TRY(parse_update());
	return true;

#undef TRY
}
