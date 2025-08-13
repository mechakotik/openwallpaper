#ifndef WD_RESOURCE_MANAGER_H
#define WD_RESOURCE_MANAGER_H

#include <openwallpaper.h>

#define WD_OBJECTMANAGER_BUCKET_SIZE_LOG2 10
#define WD_OBJECTMANAGER_MAX_BUCKETS 1024

typedef enum : uint8_t {
    WD_OBJECT_TEXTURE,
    WD_OBJECT_SHADER,
    WD_OBJECT_VERTEX_BUFFER,
    WD_OBJECT_INDEX_BUFFER,
    WD_OBJECT_VERTEX_LAYOUT,
    WD_OBJECT_PIPELINE,
} wd_object_type;

typedef struct wd_object_manager_state {
    wd_object_type* type_buckets[WD_OBJECTMANAGER_MAX_BUCKETS];
    void** data_buckets[WD_OBJECTMANAGER_MAX_BUCKETS];
    uint32_t top;
} wd_object_manager_state;

bool wd_new_object(wd_object_manager_state* state, wd_object_type type, void* data, ow_id* result);
bool wd_get_object(wd_object_manager_state* state, ow_id object, wd_object_type* type, void** data);
void wd_free_object(wd_object_manager_state* state, ow_id object);
void wd_free_object_manager(wd_object_manager_state* state);

#endif
