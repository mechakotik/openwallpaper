#ifndef WD_ZIP_H
#define WD_ZIP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct wd_zip_state {
    struct zip* archive;
} wd_zip_state;

bool wd_init_zip(wd_zip_state* zip, const char* path);
bool wd_zip_get_file_size(wd_zip_state* zip, const char* path, size_t* size);
bool wd_zip_read_file(wd_zip_state* zip, const char* path, uint8_t* result);
void wd_free_zip(wd_zip_state* zip);

#endif
