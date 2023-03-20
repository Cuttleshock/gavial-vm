#ifndef VALUE_H
#define VALUE_H

#include "common.h"

#define SCAL(x)          ((GvmLiteral){ .scalar=(x) })
#define VEC2(x, y)       ((GvmLiteral){ .vec2={ (uint16_t)(x), (uint16_t)(y) } })
#define VEC4(x, y, z, w) ((GvmLiteral){ .vec4={ (uint8_t)(x), (uint8_t)(y), (uint8_t)(z), (uint8_t)(w) } })

typedef enum {
	VAL_SCALAR,
	VAL_VEC2,
	VAL_VEC4,
} GvmValType;

typedef union {
	uint32_t scalar;
	uint16_t vec2[2];
	uint8_t vec4[4];
} GvmLiteral;

#endif // VALUE_H
