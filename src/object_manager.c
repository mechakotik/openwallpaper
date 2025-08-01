#include "object_manager.h"
#include "error.h"

#define BUCKET_SIZE_LOG2 10
#define MAX_BUCKETS 1024

struct {
    wd_object_type* type_buckets[MAX_BUCKETS];
    void** data_buckets[MAX_BUCKETS];
    uint32_t top;
} state;

ow_id wd_new_object(wd_object_type type, void* data) {
    if((state.top >> BUCKET_SIZE_LOG2) >= MAX_BUCKETS) {
        WD_ERROR("more that %d objects allocated", MAX_BUCKETS * (1 << BUCKET_SIZE_LOG2));
    }

    uint32_t bucket = state.top >> BUCKET_SIZE_LOG2;
    uint32_t index = state.top & (1 << BUCKET_SIZE_LOG2);
    if(index == 0) {
        state.type_buckets[bucket] = malloc((1 << BUCKET_SIZE_LOG2) * sizeof(ow_attribute_type*));
        state.data_buckets[bucket] = calloc(1 << BUCKET_SIZE_LOG2, sizeof(void*));
    }

    state.type_buckets[bucket][index] = type;
    state.data_buckets[bucket][index] = data;
    return state.top++;
}

void wd_get_object(ow_id object, wd_object_type* type, void** data) {
    if(object >= state.top) {
        WD_ERROR("object id out of range");
    }

    uint32_t bucket = object >> BUCKET_SIZE_LOG2;
    uint32_t index = object & (1 << BUCKET_SIZE_LOG2);
    *type = state.type_buckets[bucket][index];
    *data = state.data_buckets[bucket][index];
}

void wd_free_object(ow_id object) {
    if(object >= state.top) {
        return;
    }

    uint32_t bucket = object >> BUCKET_SIZE_LOG2;
    uint32_t index = object & (1 << BUCKET_SIZE_LOG2);
    state.data_buckets[bucket][index] = NULL;
}

void wd_free_object_manager() {
    for(ow_id i = 0; i < state.top; i++) {
        wd_free_object(i);
    }
    for(uint32_t i = 0; i < MAX_BUCKETS; i++) {
        if(state.type_buckets[i] != NULL) {
            free(state.type_buckets[i]);
            free(state.data_buckets[i]);
        } else {
            break;
        }
    }
}
