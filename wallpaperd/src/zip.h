#ifndef WD_ZIP_H
#define WD_ZIP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct wd_zip_state {
    struct zip* archive;
} wd_zip_state;

bool wd_init_zip(wd_zip_state* zip, const char* path);
bool wd_read_from_zip(wd_zip_state* zip, const char* path, uint8_t** result, size_t* size);
void wd_free_zip(wd_zip_state* zip);

#endif
