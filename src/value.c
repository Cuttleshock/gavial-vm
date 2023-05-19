#include <stdio.h>
#include <stdlib.h>

#include "value.h"

// Fixed-point in range [-10e8 + 1, 10e8 - 1] with resolution of 10e-6
// This requires 47 bits (plus the sign bit).
// Int addition/subtraction will never overflow, as the absolute magnitude can
// only reach 48 bits before we clamp back down.
// Double multiplication/division is lossless, as doubles have 52 bits of
// precision at all magnitudes.
#define FP_DEN 1000000l               // denominator of epsilon
#define FP_EPS (1.0 / (double)FP_DEN) // resolution and epsilon
#define FP_MAX (99999999l * FP_DEN)   // greatest magnitude

#define SCX(constant) ((constant).as.scalar)
#define V2X(constant) ((constant).as.vec2[0])
#define V2Y(constant) ((constant).as.vec2[1])

static GvmConstant scalar(FixedPoint x)
{
	GvmConstant c;
	c.type = VAL_SCALAR;
	c.as.scalar = x;
	return c;
}

static GvmConstant vec2(FixedPoint x, FixedPoint y)
{
	GvmConstant c;
	c.type = VAL_VEC2;
	c.as.vec2[0] = x;
	c.as.vec2[1] = y;
	return c;
}

static FixedPoint double_to_fixed(double d)
{
	// TODO: Round, don't floor
	return d * FP_DEN;
}

static FixedPoint fixed_add(FixedPoint a, FixedPoint b)
{
	FixedPoint res = a + b;
	if (res > FP_MAX) {
		return FP_MAX;
	} else if (res < -FP_MAX) {
		return -FP_MAX;
	} else {
		return res;
	}
}

static FixedPoint fixed_subtract(FixedPoint a, FixedPoint b)
{
	return fixed_add(a, -b);
}

// TODO: Avoid visiting float land
static FixedPoint fixed_multiply(FixedPoint a, FixedPoint b)
{
	double a_d = a * FP_EPS;
	double b_d = b * FP_EPS;
	double res = a_d * b_d;
	return double_to_fixed(res);
}

// Adds two values, respecting the type of the first operand
// It is up to the caller to ensure matching operands (or not to care)
GvmConstant add_vals(GvmConstant a, GvmConstant b)
{
	switch (a.type) {
		case VAL_SCALAR:
			return scalar(fixed_add(SCX(a), SCX(b)));
		case VAL_VEC2:
			return vec2(fixed_add(V2X(a), V2X(b)), fixed_add(V2Y(a), V2Y(b)));
	}

	return scalar(0); // Unreachable
}

// Subtracts two values, respecting the type of the first operand
GvmConstant subtract_vals(GvmConstant a, GvmConstant b)
{
	switch (a.type) {
		case VAL_SCALAR:
			return scalar(fixed_subtract(SCX(a), SCX(b)));
		case VAL_VEC2:
			return vec2(fixed_subtract(V2X(a), V2X(b)), fixed_subtract(V2Y(a), V2Y(b)));
	}

	return scalar(0); // Unreachable
}

// Multiplies two values - for vec2, element-wise multiplication
GvmConstant multiply_vals(GvmConstant a, GvmConstant b)
{
	switch (a.type) {
		case VAL_SCALAR:
			return scalar(fixed_multiply(SCX(a), SCX(b)));
		case VAL_VEC2:
			return vec2(fixed_multiply(V2X(a), V2X(b)), fixed_multiply(V2Y(a), V2Y(b)));
	}

	return scalar(0); // Unreachable
}

// Compares two values, treating them as scalars
// Returns scalar 0 or 1
GvmConstant val_less_than(GvmConstant a, GvmConstant b)
{
	return scalar(SCX(a) < SCX(b) ? 1 : 0);
}

// Compares two values, treating them as scalars
// Returns scalar 0 or 1
GvmConstant val_greater_than(GvmConstant a, GvmConstant b)
{
	return scalar(SCX(a) > SCX(b) ? 1 : 0);
}

// Treating a and b as scalars, takes their modulus
// If either is non-integer, return value is unspecified
GvmConstant val_modulus(GvmConstant a, GvmConstant b)
{
	int a_whole = SCX(a) / FP_DEN;
	int b_whole = SCX(b) / FP_DEN;
	return scalar(a_whole % b_whole * FP_DEN);
}

GvmConstant val_vec2_get_x(GvmConstant v)
{
	return scalar(V2X(v));
}

GvmConstant val_vec2_get_y(GvmConstant v)
{
	return scalar(V2Y(v));
}

GvmConstant val_vec2_make(GvmConstant x, GvmConstant y)
{
	return vec2(SCX(x), SCX(y));
}

// Like ! operator - converts scalar b to scalar 0 or 1
GvmConstant val_falsify(GvmConstant b)
{
	return scalar(!SCX(b));
}

GvmConstant val_and(GvmConstant a, GvmConstant b)
{
	if (SCX(a) != 0 && SCX(b) != 0) {
		return scalar(1);
	} else {
		return scalar(0);
	}
}

GvmConstant val_or(GvmConstant a, GvmConstant b)
{
	if (SCX(a) != 0 || SCX(b) != 0) {
		return scalar(1);
	} else {
		return scalar(0);
	}
}

static void sprint_fixed(char *buffer, FixedPoint n)
{
	int64_t whole = n / FP_DEN;
	int64_t frac = ((n >= 0) ? n : -n) % FP_DEN;
	int offset = sprintf(buffer, "%ld", whole);
	if (frac != 0) {
		offset += sprintf(buffer + offset, ".%06ld", frac);
		for (char *ptr = buffer + offset; *ptr == '0'; --ptr) {
			*ptr = '\0';
		}
	}
}

// Fearlessly places human-readable serialisation in buffer
// Buffer must be at least 32 bytes long
void sprint_scalar(char *buffer, GvmConstant value)
{
	sprint_fixed(buffer, SCX(value));
}

void sprint_vec2_x(char *buffer, GvmConstant value)
{
	sprint_fixed(buffer, V2X(value));
}

void sprint_vec2_y(char *buffer, GvmConstant value)
{
	sprint_fixed(buffer, V2Y(value));
}

// TODO: Should this live in debug?
void print_value(GvmConstant val)
{
	char buf_x[32];
	char buf_y[32];

	switch (val.type) {
		case VAL_SCALAR:
			sprint_scalar(buf_x, val);
			gvm_log("%s", buf_x);
			break;
		case VAL_VEC2:
			sprint_vec2_x(buf_x, val);
			sprint_vec2_y(buf_y, val);
			gvm_log("(%s, %s)", buf_x, buf_y);
			break;
	}
}

// Returns the integer part of v.x (truncating towards zero)
int vec2_get_x(GvmConstant v)
{
	return V2X(v) / FP_DEN;
}

int vec2_get_y(GvmConstant v)
{
	return V2Y(v) / FP_DEN;
}

static FixedPoint scan_fixed(const char *str)
{
	char *end = NULL;
	int64_t whole = strtol(str, &end, 10);
	if (whole <= -FP_MAX / FP_DEN) {
		return -FP_MAX;
	} else if (FP_MAX / FP_DEN <= whole) {
		return FP_MAX;
	} else if (end != NULL && *end == '.') {
		int64_t frac = strtol(end + 1, NULL, 10);
		if (whole < 0) {
			frac *= -1;
		}
		return whole * FP_DEN + frac;
	} else {
		return whole * FP_DEN;
	}
}

// Deserialises str to scalar
// Silently returns zero on error; clamps if out of range
GvmConstant scan_scalar(const char *str)
{
	return scalar(scan_fixed(str));
}

GvmConstant scan_vec2(const char *str_x, const char *str_y)
{
	return vec2(scan_fixed(str_x), scan_fixed(str_y));
}

// Converts double to scalar, clamping if out of range
GvmConstant double_to_scalar(double x)
{
	return scalar(double_to_fixed(x));
}

GvmConstant double_to_vec2(double x, double y)
{
	return vec2(double_to_fixed(x), double_to_fixed(y));
}
