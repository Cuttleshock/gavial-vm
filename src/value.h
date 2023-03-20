#ifndef VALUE_H
#define VALUE_H

#include "common.h"

#define SCAL(x)          ((GvmConstant){ .as={ .scalar=(x) }, .type=VAL_SCALAR })
#define VEC2(x, y)       ((GvmConstant){ .as={ .vec2={ (uint16_t)(x), (uint16_t)(y) } }, .type=VAL_VEC2 })
#define VEC4(x, y, z, w) ((GvmConstant){ .as={ .vec4={ (uint8_t)(x), (uint8_t)(y), (uint8_t)(z), (uint8_t)(w) } }, .type=VAL_VEC4 })

typedef enum {
	VAL_SCALAR,
	VAL_VEC2,
	VAL_VEC4,
} GvmValType;

typedef union {
	int32_t scalar;
	int16_t vec2[2];
	int8_t vec4[4];
} GvmLiteral;

typedef struct {
	GvmLiteral as;
	GvmValType type;
} GvmConstant;

GvmConstant add_vals(GvmConstant a, GvmConstant b);
GvmConstant subtract_vals(GvmConstant a, GvmConstant b);
GvmConstant val_less_than(GvmConstant a, GvmConstant b);
GvmConstant val_greater_than(GvmConstant a, GvmConstant b);

#endif // VALUE_H
