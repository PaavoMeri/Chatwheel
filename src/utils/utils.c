#include <stdio.h>
#include <stdlib.h>
#include "utils.h"

// Function to log error messages
void log_error(const char *message) {
    fprintf(stderr, "Error: %s\n", message);
}

// Function to log informational messages
void log_info(const char *message) {
    printf("Info: %s\n", message);
}

// Function to safely allocate memory
void* safe_malloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr) {
        log_error("Memory allocation failed");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

// Function to safely free memory
void safe_free(void *ptr) {
    if (ptr) {
        free(ptr);
        ptr = NULL;
    }
}