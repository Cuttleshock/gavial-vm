#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "filesystem.h"
#include "memory.h"

// Writes the absolute filepath to buf, given a filepath relative to the executable.
// Returns: success.
static bool make_absolute_path(const char *relative, char *out_buf, size_t bufsize)
{
	if (bufsize == 0) { // TODO: too paranoid?
		return false;
	}

	// Write the executable's cwd to out_buf
	ssize_t readlink_success = readlink("/proc/self/exe", out_buf, bufsize);
	if (readlink_success == -1 || out_buf[bufsize - 1] != 0) {
		return false;
	}

	char *last_slash = strrchr(out_buf, '/');
	last_slash[1] = 0;
	if (strlen(relative) + strlen(out_buf) + 1 > bufsize) {
		return false;
	}

	strcat(out_buf, relative);
	return true;
}

// Given a relative filepath, reads a file to a buffer.
// Returns: the buffer, and transfers ownership (or NULL if anything failed)
char *read_file(const char *path)
{
	char full_path[256] = { 0 };
	char *buffer = NULL;

	bool filepath_success = make_absolute_path(path, full_path, 256);
	if (!filepath_success) {
		return NULL;
	}

	// Open file
	FILE *file = fopen(full_path, "r");
	if (file == NULL) {
		return NULL;
	}

	// Determine file size
	int fseek_err = fseek(file, 0, SEEK_END); // TODO: fseeko(), ftello() safer
	if (fseek_err != 0) {
		fclose(file);
		return NULL;
	}

	long fsize = ftell(file);
	buffer = gvm_malloc(fsize + 1);
	buffer[fsize] = 0;
	rewind(file);

	// Copy into buffer
	size_t bytes_read = fread(buffer, 1, fsize + 1, file);
	if (ferror(file) != 0 || bytes_read != fsize || feof(file) == 0) {
		gvm_free(buffer);
		fclose(file);
		return NULL;
	}

	fclose(file);
	return buffer;
}
