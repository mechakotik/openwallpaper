#ifndef WD_RESOURCE_MANAGER_H
#define WD_RESOURCE_MANAGER_H

#include <openwallpaper.h>

typedef enum : uint8_t {
    WD_OBJECT_TEXTURE,
    WD_OBJECT_SHADER,
    WD_OBJECT_VERTEX_BUFFER,
    WD_OBJECT_INDEX_BUFFER,
    WD_OBJECT_VERTEX_LAYOUT,
    WD_OBJECT_PIPELINE,
} wd_object_type;

ow_id wd_new_object(wd_object_type type, void* data);
void wd_get_object(ow_id object, wd_object_type* type, void** data);
void wd_free_object(ow_id object);
void wd_free_object_manager();

#endif
