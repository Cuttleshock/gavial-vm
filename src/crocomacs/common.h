#ifndef CROCOMACS_IMPL
	#error "Implementation file: do not include"
#endif

#ifndef CROCOMACS_COMMON_H
#define CROCOMACS_COMMON_H

#include "shared_types.h"

// Utilities - storage in eval.c
extern CcmLogger ccm_log;
extern CcmMalloc ccm_malloc;
extern CcmRealloc ccm_realloc;
extern CcmFree ccm_free;

#endif // CROCOMACS_COMMON_H
