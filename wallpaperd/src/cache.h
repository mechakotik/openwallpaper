#ifndef WD_CACHE_H
#define WD_CACHE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WD_MAX_PATH 4096

bool wd_cache_get_namespace_dir(const char* namespace, char* path_out, size_t path_out_size);
bool wd_cache_read_file(const char* path, uint8_t** data_out, size_t* size_out);
bool wd_cache_write_file(const char* path, const uint8_t* data, size_t size);
bool wd_cache_remove_file(const char* path);

#endif
