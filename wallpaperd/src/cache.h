#ifndef WD_CACHE_H
#define WD_CACHE_H

#include <sds.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

sds wd_cache_get_namespace_dir(const char* namespace);
bool wd_cache_read_file(const char* path, uint8_t** data_out, size_t* size_out);
bool wd_cache_write_file(const char* path, const uint8_t* data, size_t size);
bool wd_cache_remove_file(const char* path);

#endif
