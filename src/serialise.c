#include <stdio.h>

#include "common.h"
#include "serialise.h"

// static FILE *open_file = NULL;

// Returns: success. If successful, end_serialise() must be called later on.
bool begin_serialise(const char *base_path)
{
	gvm_log("Opening %s to serialise\n", base_path);
	return true;
}

// Returns: success. Failure does not obviate the need to end_serialise().
bool serialise(const char *name, GvmConstant value)
{
	gvm_log("Serialising %s ", name);
	switch (value.type) {
		case VAL_SCALAR:
			gvm_log("(type SCALAR, value %d)\n", value.as.scalar);
			return true;
		case VAL_VEC2:
			gvm_log("(type VEC2, value (%d, %d))\n", V2X(value), V2Y(value));
			return true;
		case VAL_VEC4:
			gvm_log("(type VEC4, value (%d, %d, %d, %d))\n", V4X(value), V4Y(value), V4Z(value), V4W(value));
			return true;
		default: // Unreachable
			gvm_log("(unknown type %d)\n", value.type);
			return false;
	}
}

void end_serialise()
{
	gvm_log("Closing file handle\n");
}
