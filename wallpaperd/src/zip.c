#include "zip.h"
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

bool wd_zip_get_file_size(wd_zip_state* zip, const char* path, size_t* size) {
    zip_stat_t stat = {0};
    memset(&stat, 0, sizeof(stat));
    if(zip_stat(zip->archive, path, 0, &stat) != 0) {
        wd_set_error("zip_stat for %s failed: %s", path, zip_strerror(zip->archive));
        return false;
    }

    if((stat.valid & ZIP_STAT_SIZE) == 0) {
        // TODO: handle this?
        wd_set_error("decompressed size of %s is unknown, unsupported for now", path);
        return false;
    }

    if(stat.size >= SIZE_MAX) {
        wd_set_error("decompressed size of %s is too large", path);
        return false;
    }

    *size = (size_t)stat.size;
    return true;
}

bool wd_zip_read_file(wd_zip_state* zip, const char* path, uint8_t* result) {
    size_t size = 0;
    if(!wd_zip_get_file_size(zip, path, &size)) {
        return false;
    }
    if(size == 0) {
        return true;
    }

    zip_file_t* file = zip_fopen(zip->archive, path, 0);
    if(file == NULL) {
        wd_set_error("zip_fopen for %s failed: %s", path, zip_strerror(zip->archive));
        return false;
    }

    size_t offset = 0;
    while(offset < size) {
        zip_int64_t read = zip_fread(file, result + offset, size - offset);
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

    if(offset != size) {
        wd_set_error(
            "zip_fread for %s failed: unexpected EOF (read %zu bytes, expected %zu bytes)", path, offset, size);
        zip_fclose(file);
        return false;
    }

    zip_fclose(file);
    return true;
}

void wd_free_zip(wd_zip_state* zip) {
    if(zip->archive != NULL) {
        zip_close(zip->archive);
        zip->archive = NULL;
    }
}
