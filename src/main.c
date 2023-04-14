#include <stdlib.h>

#include "common.h"
#include "debug.h"
#include "parser.h"
#include "vm.h"

int main(int argc, char *argv[])
{
	const char *rom_path = "update.ccm";
	if (argc > 1) {
		rom_path = argv[1];
	}

	gvm_log("Welcome to Gavial VM!\n");
	if (!init_vm()) {
		return EXIT_FAILURE;
	}

	gvm_log("Loading from %s...\n", rom_path);
	if (!parse(rom_path)) {
		return EXIT_FAILURE;
	}

	disassemble();
	if (run_vm()) {
		return EXIT_SUCCESS;
	} else {
		return EXIT_FAILURE;
	}
}
