#ifndef FILE_VALIDATION_H
#define FILE_VALIDATION_H

#include <stdbool.h>

bool is_forbidden_extension(const char *filename);
char* process_filename(const char *input_filename);

#endif