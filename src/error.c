#include "error.h"
#include <stdarg.h>
#include <stdio.h>

#define ERROR_BUFFER_SIZE 1024

static char error_buffer[ERROR_BUFFER_SIZE];
static bool scene_error = false;

void wd_set_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(error_buffer, ERROR_BUFFER_SIZE, format, args);
    va_end(args);
    scene_error = false;
}

const char* wd_get_last_error() {
    return error_buffer;
}
