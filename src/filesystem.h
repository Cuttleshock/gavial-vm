#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <time.h>

#define MODIFY_ERROR -1

time_t last_modified(const char *path);
char *read_file(const char *path, int *out_length);
char *read_file_executable(const char *path, int *out_length);

#endif // FILESYSTEM_H
