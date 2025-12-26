#include "object_manager.h"
#include <stdlib.h>
#include "SDL3/SDL_gpu.h"
#include "error.h"
#include "state.h"

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

void wd_free_object(wd_state* state, uint32_t object) {
    if(object >= state->object_manager.top) {
        return;
    }

    uint32_t bucket = object >> WD_OBJECTMANAGER_BUCKET_SIZE_LOG2;
    uint32_t index = object & ((1 << WD_OBJECTMANAGER_BUCKET_SIZE_LOG2) - 1);
    void* data = state->object_manager.data_buckets[bucket][index];
    if(data == NULL) {
        return;
    }

    switch(state->object_manager.type_buckets[bucket][index]) {
        case WD_OBJECT_VERTEX_BUFFER:
        case WD_OBJECT_INDEX16_BUFFER:
        case WD_OBJECT_INDEX32_BUFFER:
            SDL_ReleaseGPUBuffer(state->output.gpu, (SDL_GPUBuffer*)data);
            break;
        case WD_OBJECT_TEXTURE:
            SDL_ReleaseGPUTexture(state->output.gpu, (SDL_GPUTexture*)data);
            break;
        case WD_OBJECT_SAMPLER:
            SDL_ReleaseGPUSampler(state->output.gpu, (SDL_GPUSampler*)data);
            break;
        case WD_OBJECT_VERTEX_SHADER:
        case WD_OBJECT_FRAGMENT_SHADER:
            SDL_ReleaseGPUShader(state->output.gpu, (SDL_GPUShader*)data);
            break;
        case WD_OBJECT_PIPELINE:
            SDL_ReleaseGPUGraphicsPipeline(state->output.gpu, (SDL_GPUGraphicsPipeline*)data);
            break;
        default:
            break;
    }

    state->object_manager.data_buckets[bucket][index] = NULL;
}

void wd_free_object_manager(wd_state* state) {
    for(uint32_t i = 0; i < state->object_manager.top; i++) {
        wd_free_object(state, i);
    }
    for(uint32_t i = 0; i < WD_OBJECTMANAGER_MAX_BUCKETS; i++) {
        if(state->object_manager.type_buckets[i] != NULL) {
            free(state->object_manager.type_buckets[i]);
            free(state->object_manager.data_buckets[i]);
        } else {
            break;
        }
    }
}
