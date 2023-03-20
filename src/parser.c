#include "common.h"
#include "parser.h"
#include "value.h"
#include "vm.h"

static bool parse_state()
{
#define TRY(p) if (!p) return false;

	TRY(insert_state((GvmState){ "snakes", SCAL(0), SCAL(5), VAL_SCALAR }, 6));
	TRY(insert_state((GvmState){ "badgers", VEC2(0, 0), VEC2(10, 20), VAL_VEC2 }, 7));
	TRY(insert_state((GvmState){ "dogs", VEC4(0, 0, 0, 0), VEC4(3, 3, 2, 2), VAL_VEC4 }, 4));
	return true;

#undef TRY
}

static bool parse_update()
{
	instruction(OP_LOAD_CONST);
	instruction(0);
	instruction(OP_LOAD_CONST);
	instruction(1);
	instruction(OP_ADD);
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
