#ifndef VALUE_H
#define VALUE_H

#include "common.h"

#define LIT_SCAL(x)          ((GvmLiteral){ .scalar=(x) })
#define LIT_VEC2(x, y)       ((GvmLiteral){ .vec2={ (uint16_t)(x), (uint16_t)(y) } })

#define CONSTANT(lit, _type)   ((GvmConstant){ .as=(lit), .type=(_type) })
#define SCAL(x)                CONSTANT(LIT_SCAL(x), VAL_SCALAR)
#define VEC2(x, y)             CONSTANT(LIT_VEC2(x, y), VAL_VEC2)

#define V2X(constant) ((constant).as.vec2[0])
#define V2Y(constant) ((constant).as.vec2[1])

typedef enum {
	VAL_SCALAR,
	VAL_VEC2,
} GvmValType;

typedef union {
	int32_t scalar;
	int16_t vec2[2];
} GvmLiteral;

typedef struct {
	GvmLiteral as;
	GvmValType type;
} GvmConstant;

GvmConstant add_vals(GvmConstant a, GvmConstant b);
GvmConstant subtract_vals(GvmConstant a, GvmConstant b);
GvmConstant val_less_than(GvmConstant a, GvmConstant b);
GvmConstant val_greater_than(GvmConstant a, GvmConstant b);

void print_value(GvmConstant val);

#endif // VALUE_H
