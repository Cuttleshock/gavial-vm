#ifndef VALUE_H
#define VALUE_H

#include "common.h"

typedef int64_t FixedPoint;

typedef enum {
	VAL_SCALAR,
	VAL_VEC2,
} ValueType;

typedef struct {
	union {
		FixedPoint scalar;
		FixedPoint vec2[2];
	} as;
	ValueType type;
} GvmConstant;

GvmConstant add_vals(GvmConstant a, GvmConstant b);
GvmConstant subtract_vals(GvmConstant a, GvmConstant b);
GvmConstant multiply_vals(GvmConstant a, GvmConstant b);
GvmConstant divide_vals(GvmConstant a, GvmConstant b);
GvmConstant rand_val(GvmConstant *seed);
GvmConstant rand_int_val(GvmConstant max, GvmConstant *seed);
GvmConstant floor_val(GvmConstant from, GvmConstant snap);
GvmConstant ceil_val(GvmConstant from, GvmConstant snap);
GvmConstant floor_val_ex(GvmConstant from, GvmConstant snap);
GvmConstant ceil_val_ex(GvmConstant from, GvmConstant snap);
bool val_less_than(GvmConstant a, GvmConstant b);
bool val_greater_than(GvmConstant a, GvmConstant b);
bool val_equal(GvmConstant a, GvmConstant b);
GvmConstant val_modulus(GvmConstant a, GvmConstant b);
GvmConstant val_vec2_get_x(GvmConstant v);
GvmConstant val_vec2_get_y(GvmConstant v);
GvmConstant val_vec2_make(GvmConstant x, GvmConstant y);
GvmConstant val_vec2_normalize(GvmConstant v);
GvmConstant val_falsify(GvmConstant b);
GvmConstant val_and(GvmConstant a, GvmConstant b);
GvmConstant val_or(GvmConstant a, GvmConstant b);

void sprint_scalar(char *buffer, GvmConstant value);
void sprint_vec2_x(char *buffer, GvmConstant value);
void sprint_vec2_y(char *buffer, GvmConstant value);
void print_value(GvmConstant val);

int vec2_get_x(GvmConstant v);
int vec2_get_y(GvmConstant v);

GvmConstant scan_scalar(const char *str);
GvmConstant scan_vec2(const char *str_x, const char *str_y);
GvmConstant double_to_scalar(double x);
GvmConstant double_to_vec2(double x, double y);
GvmConstant int_to_scalar(int x);

#endif // VALUE_H
