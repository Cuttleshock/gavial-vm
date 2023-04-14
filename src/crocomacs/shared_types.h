// Types needed both by internal implementation and by consumers

#ifndef CROCOMACS_TYPES_H
#define CROCOMACS_TYPES_H

#include <stddef.h> // size_t

typedef enum {
	CCM_NUMBER,
	CCM_STRING,
	CCM_SYMBOL,
} CcmValType;

typedef struct {
	CcmValType type;
	union {
		double number; // CCM_NUMBER
		struct {
			const char *chars; // crocomacs maintains ownership
			int length;
		} str; // CCM_STRING, CCM_SYMBOL
	} as;
} CcmValue;

typedef struct {
	CcmValue *values; // crocomacs maintains ownership
	int count;
	int capacity;
} CcmList;

// TODO: RETURN CcmList, not void
typedef void (*CcmHook)(CcmList vals[]);
typedef void (*CcmNumberHook)(double);
typedef void (*CcmStringHook)(const char *, int);
typedef void (*CcmSymbolHook)(const char *, int);

typedef void (*CcmLogger)(const char *restrict message, ...);
typedef void *(*CcmMalloc)(size_t);
typedef void *(*CcmRealloc)(void *, size_t);
typedef void (*CcmFree)(void *);

#endif // CROCOMACS_TYPES_H
