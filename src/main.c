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
	instruction(OP_RETURN);
	disassemble();
	if (run_vm()) {
		return EXIT_SUCCESS;
	} else {
		return EXIT_FAILURE;
	}
}
