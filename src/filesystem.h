#ifndef FILESYSTEM_H
#define FILESYSTEM_H

char *read_file(const char *path, int *out_length);
char *read_file_executable(const char *path, int *out_length);

#endif // FILESYSTEM_H
