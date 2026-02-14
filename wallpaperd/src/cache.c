#include "cache.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include "malloc.h"

static bool get_cache_root_dir(char* path_out, size_t path_out_size) {
    const char* home = SDL_GetUserFolder(SDL_FOLDER_HOME);
    if(home == NULL && home[0] == '\0') {
        return false;
    }

    int ret = snprintf(path_out, path_out_size, "%s/.cache/wallpaperd", home);
    if(ret < 0 || (size_t)ret >= path_out_size) {
        return false;
    }
    if(!SDL_CreateDirectory(path_out)) {
        return false;
    }
    return true;
}

bool wd_cache_get_namespace_dir(const char* namespace, char* path_out, size_t path_out_size) {
    char root_dir[WD_MAX_PATH];
    if(!get_cache_root_dir(root_dir, sizeof(root_dir))) {
        return false;
    }
    int ret = snprintf(path_out, path_out_size, "%s/%s", root_dir, namespace);
    if(ret < 0 || (size_t)ret >= path_out_size) {
        return false;
    }
    return SDL_CreateDirectory(path_out);
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
