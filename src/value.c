#include <stdio.h>

#include "value.h"

// Adds two values, respecting the type of the first operand
// It is up to the caller to ensure matching operands (or not to care)
GvmConstant add_vals(GvmConstant a, GvmConstant b)
{
	switch (a.type) {
		case VAL_SCALAR:
			return SCAL(a.as.scalar + b.as.scalar);
		case VAL_VEC2:
			return VEC2(a.as.vec2[0] + b.as.vec2[0], a.as.vec2[1] + b.as.vec2[1]);
	}

	return SCAL(0); // Unreachable
}

// Subtracts two values, respecting the type of the first operand
GvmConstant subtract_vals(GvmConstant a, GvmConstant b)
{
	switch (a.type) {
		case VAL_SCALAR:
			return SCAL(a.as.scalar - b.as.scalar);
		case VAL_VEC2:
			return VEC2(a.as.vec2[0] - b.as.vec2[0], a.as.vec2[1] - b.as.vec2[1]);
	}

	return SCAL(0); // Unreachable
}

// Compares two values, treating them as scalars
// Returns scalar 0 or 1
GvmConstant val_less_than(GvmConstant a, GvmConstant b)
{
	return SCAL(a.as.scalar < b.as.scalar ? 1 : 0);
}

// Compares two values, treating them as scalars
// Returns scalar 0 or 1
GvmConstant val_greater_than(GvmConstant a, GvmConstant b)
{
	return SCAL(a.as.scalar > b.as.scalar ? 1 : 0);
}

void print_value(GvmConstant val)
{
	switch (val.type) {
		case VAL_SCALAR:
			printf("%d", val.as.scalar);
			return;
		case VAL_VEC2:
			printf("(%d, %d)", val.as.vec2[0], val.as.vec2[1]);
			return;
	}
}
