#ifndef WPE_COMPILE_DEFS_H
#define WPE_COMPILE_DEFS_H

#include <stdbool.h>
#include <stddef.h>
#include "openwallpaper.h"

typedef union {
    float at[2];
    struct {
        float x, y;
    };
    struct {
        float w, h;
    };
    struct {
        float r, g;
    };
} Vec2;

typedef union {
    float at[3];
    struct {
        float x, y, z;
    };
    struct {
        float r, g, b;
    };
} Vec3;

typedef union {
    float at[4];
    struct {
        float x, y, z, w;
    };
    struct {
        float r, g, b, a;
    };
} Vec4;

typedef struct {
    const char* name;
    const char* constant_name;
    const char* type;
    int array_size;
    float* default_value;
    bool default_set;
} UniformInfo;

typedef struct {
    const char* name;
    const char* type;
    int array_size;
} AttributeInfo;

typedef struct {
    const char* name;
    const char* default_texture;
} SamplerInfo;

typedef struct {
    int id;
    ow_vertex_shader_id vertex;
    ow_fragment_shader_id fragment;
    UniformInfo* vertex_uniforms;
    int num_vertex_uniforms;
    UniformInfo* fragment_uniforms;
    int num_fragment_uniforms;
    AttributeInfo* attributes;
    int num_attributes;
    SamplerInfo* samplers;
    int num_samplers;
} Shader;

typedef struct {
    const char* blending;
    int shader_id;
    Shader* shader;
    ow_texture_id* textures;
    ow_pipeline_id pipeline;
} Material;

typedef struct {
    Vec3 color;
    int color_blend_mode;
    float alpha;
    float brightness;
    bool fullscreen;
    bool passthrough;
    Material material;
} ImageObject;

typedef struct {
} ParticleObject;

typedef enum {
    OBJECTTYPE_IMAGE,
    OBJECTTYPE_PARTICLE,
    OBJECTTYPE_EMPTY,
} ObjectType;

typedef struct {
    int id;
    int parent;
    const char* name;
    Vec3 origin;
    Vec3 scale;
    Vec3 angles;
    Vec2 size;
    bool perspective;
    Vec2 parallax_depth;
    ObjectType type;
    union {
        ImageObject image;
        ParticleObject particle;
    };
} Object;

typedef struct {
    bool parallax;
    float parallax_amount;
    float parallax_delay;
    float parallax_mouse_influence;
    bool shake;
    float shake_amplitude;
    float shake_roughness;
    float shake_speed;
    Vec2 ortho;
    float zoom;
    float fov;
    float near_z;
    float far_z;
} SceneGeneral;

typedef struct {
    Object* objects;
    size_t num_objects;
    SceneGeneral general;
} Scene;

#ifdef SCENE
#include "scene.h"
#else
Shader* shaders;
int num_shaders;

Scene scene;
#endif

#endif
