#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>

// Function prototypes
void log_error(const char *message);
void log_info(const char *message);
void *safe_malloc(size_t size);

#endif // UTILS_H