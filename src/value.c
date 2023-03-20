#include "value.h"

// Adds two values, respecting the type of the first operand
// It is up to the caller to ensure matching operands (or not to care)
GvmConstant add_vals(GvmConstant a, GvmConstant b)
{
	switch (a.type) {
		case VAL_VEC2:
			return VEC2(a.as.vec2[0] + b.as.vec2[0], a.as.vec2[1] + b.as.vec2[1]);
		case VAL_VEC4:
			return VEC4(a.as.vec4[0] + b.as.vec4[0], a.as.vec4[1] + b.as.vec4[1], a.as.vec4[2] + b.as.vec4[2], a.as.vec4[3] + b.as.vec4[3]);
		case VAL_SCALAR:
		default:
			return SCAL(a.as.scalar + b.as.scalar);
	}
}

// Subtracts two values, respecting the type of the first operand
GvmConstant subtract_vals(GvmConstant a, GvmConstant b)
{
	switch (a.type) {
		case VAL_VEC2:
			return VEC2(a.as.vec2[0] - b.as.vec2[0], a.as.vec2[1] - b.as.vec2[1]);
		case VAL_VEC4:
			return VEC4(a.as.vec4[0] - b.as.vec4[0], a.as.vec4[1] - b.as.vec4[1], a.as.vec4[2] - b.as.vec4[2], a.as.vec4[3] - b.as.vec4[3]);
		case VAL_SCALAR:
		default:
			return SCAL(a.as.scalar - b.as.scalar);
	}
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
