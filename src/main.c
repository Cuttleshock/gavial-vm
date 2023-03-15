#include <stdio.h>

#include "debug.h"
#include "vm.h"

int main(int argc, char *argv[])
{
	printf("Welcome to Gavial VM!\n");
	if (!init_vm()) {
		return EXIT_FAILURE;
	}
	instruction(OP_RETURN);
	disassemble();
	close_vm();
	return EXIT_SUCCESS;
}
