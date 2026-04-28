#include "cache.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include "malloc.h"

static sds get_cache_root_dir(void) {
    const char* home = SDL_GetUserFolder(SDL_FOLDER_HOME);
    if(home == NULL || home[0] == '\0') {
        return NULL;
    }

    sds path = sdscatprintf(sdsempty(), "%s/.cache/wallpaperd", home);
    if(!SDL_CreateDirectory(path)) {
        sdsfree(path);
        return NULL;
    }
    return path;
}

sds wd_cache_get_namespace_dir(const char* namespace) {
    sds path = get_cache_root_dir();
    if(path == NULL) {
        return NULL;
    }
    path = sdscatprintf(path, "/%s", namespace);
    if(!SDL_CreateDirectory(path)) {
        sdsfree(path);
        return NULL;
    }
    return path;
}

bool wd_cache_read_file(const char* path, uint8_t** data_out, size_t* size_out) {
    size_t loaded_size = 0;
    void* loaded = SDL_LoadFile(path, &loaded_size);
    if(loaded == NULL) {
        return false;
    }

    uint8_t* data = wd_malloc(loaded_size + 1);
    memcpy(data, loaded, loaded_size);
    data[loaded_size] = '\0';
    SDL_free(loaded);

    *data_out = data;
    *size_out = loaded_size;
    return true;
}

bool wd_cache_write_file(const char* path, const uint8_t* data, size_t size) {
    uint8_t empty = 0;
    const void* source = data;
    if(source == NULL && size == 0) {
        source = &empty;
    }
    if(source == NULL) {
        return false;
    }
    return SDL_SaveFile(path, source, size);
}

bool wd_cache_remove_file(const char* path) {
    SDL_PathInfo info;
    if(!SDL_GetPathInfo(path, &info)) {
        return true;
    }
    return SDL_RemovePath(path);
}
