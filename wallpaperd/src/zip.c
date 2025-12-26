#include "zip.h"
#include <stdlib.h>
#include <string.h>
#include <zip.h>
#include "error.h"

bool wd_init_zip(wd_zip_state* zip, const char* path) {
    int error = 0;
    zip->archive = zip_open(path, ZIP_RDONLY, &error);
    if(zip->archive == NULL) {
        zip_error_t zip_error;
        zip_error_init_with_code(&zip_error, error);
        wd_set_error("zip_open for %s failed: %s", path, zip_error_strerror(&zip_error));
        zip_error_fini(&zip_error);
        return false;
    }

    return true;
}

bool wd_read_from_zip(wd_zip_state* zip, const char* path, uint8_t** result, size_t* size) {
    zip_stat_t stat = {0};
    memset(&stat, 0, sizeof(stat));
    bool has_stat = (zip_stat(zip->archive, path, 0, &stat) == 0);

    zip_file_t* file = zip_fopen(zip->archive, path, 0);
    if(file == NULL) {
        wd_set_error("zip_fopen for %s failed: %s", path, zip_strerror(zip->archive));
        return false;
    }

    uint8_t* buffer = NULL;
    zip_uint64_t total = 0;
    size_t capacity = 0;

    if(has_stat && (stat.valid & ZIP_STAT_SIZE) != 0) {
        capacity = (size_t)stat.size;
        buffer = malloc(capacity + 1);

        size_t offset = 0;
        while(offset < capacity) {
            zip_int64_t read = zip_fread(file, buffer + offset, capacity - offset);
            if(read < 0) {
                zip_error_t* file_error = zip_file_get_error(file);
                wd_set_error("zip_fread for %s failed: %s", path, zip_error_strerror(file_error));
                zip_fclose(file);
                return false;
            }
            if(read == 0) {
                break;
            }
            offset += read;
        }

        total = offset;
        buffer[total] = '\0';
    } else {
        // FIXME: still read file by chunks
        wd_set_error("decompressed file size is unknown for %s, unsupported for now", path);
        zip_fclose(file);
        return false;
    }

    zip_fclose(file);
    *result = buffer;
    *size = total;
    return true;
}

void wd_free_zip(wd_zip_state* zip) {
    if(zip->archive != NULL) {
        zip_close(zip->archive);
        zip->archive = NULL;
    }
}
