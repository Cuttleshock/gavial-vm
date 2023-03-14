#include <stdio.h>

#include "debug.h"
#include "vm.h"

int main(int argc, char *argv[])
{
	printf("Welcome to Gavial VM!\n");
	instruction(OP_RETURN);
	disassemble();
	return 0;
}
