#include <stdlib.h>

#include "common.h"
#include "debug.h"
#include "parser.h"
#include "subsystems/subsystems.h"
#include "vm.h"

const int WINDOW_WIDTH = 512;
const int WINDOW_HEIGHT = 512;

int main(int argc, char *argv[])
{
	const char *rom_path = "update.ccm";
	if (argc > 1) {
		rom_path = argv[1];
	}

	gvm_log("Welcome to Gavial VM!\n");

	if(!init_subsystems(WINDOW_WIDTH, WINDOW_HEIGHT, "Gavial VM")) {
		return EXIT_FAILURE;
	}

	if (!init_vm()) {
		close_subsystems();
		return EXIT_FAILURE;
	}

	gvm_log("Loading from %s...\n", rom_path);
	if (!parse(rom_path)) {
		close_vm();
		close_subsystems();
		return EXIT_FAILURE;
	}

#ifdef DEBUG
	disassemble();
#endif

	bool success = run_vm(rom_path);
	close_vm();
	close_subsystems();
	return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
