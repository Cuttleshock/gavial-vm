#ifndef SERIALISER_H
#define SERIALISER_H

#include "value.h"

bool begin_serialise(const char *base_path);
bool serialise(const char *name, GvmConstant value);
bool end_serialise();

#endif // SERIALISER_H
