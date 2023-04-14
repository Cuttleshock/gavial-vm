#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "filesystem.h"
#include "memory.h"

// Resolves a path relative to the executable, writing to out_buf
// Returns: success
static bool make_absolute_path(const char *relative, char *out_buf, size_t bufsize)
{
	if (bufsize == 0) { // TODO: too paranoid?
		return false;
	}

	if (relative[0] == '/') {
		strncpy(out_buf, relative, bufsize);
		return (out_buf[bufsize - 1] != '\0');
	}

	// Write the executable's cwd to out_buf
	// TODO: Very platform-dependent
	ssize_t readlink_success = readlink("/proc/self/exe", out_buf, bufsize);
	if (readlink_success == -1 || out_buf[bufsize - 1] != '\0') {
		return false;
	}

	char *last_slash = strrchr(out_buf, '/');
	last_slash[1] = '\0';
	if (strlen(relative) + strlen(out_buf) + 1 > bufsize) {
		return false;
	}

	strcat(out_buf, relative);
	return true;
}

static char *read_file_impl(const char *absolute, int *out_length)
{
	// Open file
	FILE *file = fopen(absolute, "r");
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
	char *buffer = gvm_malloc(fsize + 1);
	if (buffer == NULL) {
		fclose(file);
		return NULL;
	}
	buffer[fsize] = '\0';
	rewind(file);

	// Copy into buffer
	size_t bytes_read = fread(buffer, 1, fsize + 1, file);
	if (ferror(file) != 0 || bytes_read != fsize || feof(file) == 0) {
		gvm_free(buffer);
		fclose(file);
		return NULL;
	}

	fclose(file);
	if (out_length != NULL) {
		*out_length = fsize;
	}
	return buffer;
}

// Opens path relative to cwd and reads to a buffer
// Returns: the buffer, and transfers ownership (or NULL if anything failed).
// On success, *out_length (if not NULL) contains the file's size in bytes.
char *read_file(const char *path, int *out_length)
{
	return read_file_impl(path, out_length);
}

// Opens path relative to running executable and reads to a buffer
// Returns: same as read_file
char *read_file_executable(const char *path, int *out_length)
{
	char full_path[256] = { 0 };

	bool filepath_success = make_absolute_path(path, full_path, sizeof(full_path));
	if (!filepath_success) {
		return NULL;
	}

	return read_file_impl(full_path, out_length);
}
