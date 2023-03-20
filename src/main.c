#include <stdlib.h>

#include "common.h"
#include "debug.h"
#include "parser.h"
#include "vm.h"

int main(int argc, char *argv[])
{
	gvm_log("Welcome to Gavial VM!\n");
	if (!init_vm()) {
		return EXIT_FAILURE;
	}

	if (!parse()) {
		return EXIT_FAILURE;
	}

	disassemble();
	if (run_vm()) {
		return EXIT_SUCCESS;
	} else {
		return EXIT_FAILURE;
	}
}
