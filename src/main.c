#include <stdlib.h>

#include "common.h"
#include "debug.h"
#include "vm.h"

int main(int argc, char *argv[])
{
	gvm_log("Welcome to Gavial VM!\n");
	if (!init_vm()) {
		return EXIT_FAILURE;
	}

	instruction(OP_LOAD_CONST);
	instruction(0);
	instruction(OP_LOAD_CONST);
	instruction(1);
	instruction(OP_ADD);
	instruction(OP_CLEAR_SCREEN);
	instruction(2);
	instruction(3);
	instruction(OP_RETURN);

	disassemble();
	if (run_vm()) {
		return EXIT_SUCCESS;
	} else {
		return EXIT_FAILURE;
	}
}
