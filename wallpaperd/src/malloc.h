#ifndef WD_MALLOC_H
#define WD_MALLOC_H

#include <stdlib.h>

static inline void* wd_malloc(size_t size) {
    void* ptr = malloc(size);
    if(ptr == NULL) {
        exit(2);
    }
    return ptr;
}

static inline void* wd_calloc(size_t nmemb, size_t size) {
    void* ptr = calloc(nmemb, size);
    if(ptr == NULL) {
        exit(2);
    }
    return ptr;
}

#endif
