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
	char buf_x[32];
	char buf_y[32];

	switch (value.type) {
		case VAL_SCALAR:
			sprint_scalar(buf_x, value);
			success = fprintf(open_file, "#LOAD_SCALAR(`%s, \"%s\")\n", name, buf_x);
			break;
		case VAL_VEC2:
			sprint_vec2_x(buf_x, value);
			sprint_vec2_y(buf_y, value);
			success = fprintf(open_file, "#LOAD_VEC2(`%s, \"%s\", \"%s\")\n", name, buf_x, buf_y);
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
