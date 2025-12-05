#include "object_manager.h"
#include <stdlib.h>
#include "error.h"

bool wd_new_object(wd_object_manager_state* state, wd_object_type type, void* data, uint32_t* result) {
    if((state->top >> WD_OBJECTMANAGER_BUCKET_SIZE_LOG2) >= WD_OBJECTMANAGER_MAX_BUCKETS) {
        wd_set_error(
            "more that %d objects allocated", WD_OBJECTMANAGER_MAX_BUCKETS * (1 << WD_OBJECTMANAGER_BUCKET_SIZE_LOG2));
        return false;
    }

    uint32_t bucket = state->top >> WD_OBJECTMANAGER_BUCKET_SIZE_LOG2;
    uint32_t index = state->top & ((1 << WD_OBJECTMANAGER_BUCKET_SIZE_LOG2) - 1);
    if(index == 0) {
        state->type_buckets[bucket] = malloc((1 << WD_OBJECTMANAGER_BUCKET_SIZE_LOG2) * sizeof(wd_object_type));
        state->data_buckets[bucket] = calloc(1 << WD_OBJECTMANAGER_BUCKET_SIZE_LOG2, sizeof(void*));
    }

    state->type_buckets[bucket][index] = type;
    state->data_buckets[bucket][index] = data;
    *result = state->top++;
    return true;
}

void wd_get_object(wd_object_manager_state* state, uint32_t object, wd_object_type* type, void** data) {
    if(object >= state->top) {
        *data = NULL;
        return;
    }

    uint32_t bucket = object >> WD_OBJECTMANAGER_BUCKET_SIZE_LOG2;
    uint32_t index = object & ((1 << WD_OBJECTMANAGER_BUCKET_SIZE_LOG2) - 1);
    *type = state->type_buckets[bucket][index];
    *data = state->data_buckets[bucket][index];
}

void wd_free_object(wd_object_manager_state* state, uint32_t object) {
    if(object >= state->top) {
        return;
    }

    uint32_t bucket = object >> WD_OBJECTMANAGER_BUCKET_SIZE_LOG2;
    uint32_t index = object & ((1 << WD_OBJECTMANAGER_BUCKET_SIZE_LOG2) - 1);
    state->data_buckets[bucket][index] = NULL;
}

void wd_free_object_manager(wd_object_manager_state* state) {
    for(uint32_t i = 0; i < state->top; i++) {
        wd_free_object(state, i);
    }
    for(uint32_t i = 0; i < WD_OBJECTMANAGER_MAX_BUCKETS; i++) {
        if(state->type_buckets[i] != NULL) {
            free(state->type_buckets[i]);
            free(state->data_buckets[i]);
        } else {
            break;
        }
    }
}
