#include "error.h"
#include <stdarg.h>
#include <stdio.h>

#define ERROR_BUFFER_SIZE 1024

static char error_buffer[ERROR_BUFFER_SIZE];

void wd_set_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    int unused = vsnprintf(error_buffer, ERROR_BUFFER_SIZE, format, args);
    va_end(args);
}

const char* wd_get_last_error() {
    return error_buffer;
}
