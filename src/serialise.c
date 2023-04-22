#include <stdio.h>

#include "common.h"
#include "filesystem.h"
#include "serialise.h"

static FILE *open_file = NULL;

// Returns: success
// If successful, end_serialise() must be called later on
// TODO: It would be nice to log the name of the file we're saving to
bool begin_serialise(const char *base_path)
{
	open_file = open_unique(base_path, ".save");
	return (open_file != NULL);
}

// Returns: success
// Failure does not obviate the need to end_serialise()
bool serialise(const char *name, GvmConstant value)
{
	if (open_file == NULL) {
		return false;
	}

	int success = -1;

	switch (value.type) {
		case VAL_SCALAR:
			success = fprintf(open_file, "#LOAD_SCALAR(`%s, %d)\n", name, value.as.scalar);
			break;
		case VAL_VEC2:
			success = fprintf(open_file, "#LOAD_VEC2(`%s, %d, %d)\n", name, V2X(value), V2Y(value));
			break;
		case VAL_VEC4:
			success = fprintf(open_file, "#LOAD_VEC4(`%s, %d, %d, %d, %d)\n", name, V4X(value), V4Y(value), V4Z(value), V4W(value));
			break;
	}

	// TODO: On failure, it makes sense to close the handle and delete the file
	return (success > 0);
}

// Returns: success
// If unsuccessful, the state being written was likely not saved
bool end_serialise()
{
	FILE *old_file = open_file;
	open_file = NULL;
	return (fclose(old_file) == 0);
}
