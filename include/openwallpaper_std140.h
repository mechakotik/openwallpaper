#ifndef OPENWALLPAPER_STD140_H
#define OPENWALLPAPER_STD140_H

/* C wrappers for GLSL types with std140 layout/alignment. Requires a compiler with
 * alignment specifier and anonymous structs/unions (C11 or popular extensions).
 */

#include <stdint.h>

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define OW_ALIGNAS(N) _Alignas(N)
#elif defined(_MSC_VER)
#define OW_ALIGNAS(N) __declspec(align(N))
#elif defined(__GNUC__) || defined(__clang__)
#define OW_ALIGNAS(N) __attribute__((aligned(N)))
#else
#error "no alignment specifier available for this compiler"
#endif

#define OW_STD140_ALIGNOF_SCALAR(T) (sizeof(T))
#define OW_STD140_ALIGNOF_VEC(T, N) ((N) == 1 ? sizeof(T) : ((N) == 2 ? 2 * sizeof(T) : 4 * sizeof(T)))

typedef union {
    float x;
    float r;
    float at[1];
    OW_ALIGNAS(sizeof(float)) char __align;
} glsl_float;

typedef union {
    double x;
    double r;
    double at[1];
    OW_ALIGNAS(sizeof(double)) char __align;
} glsl_double;

typedef union {
    int32_t x;
    int32_t r;
    int32_t at[1];
    OW_ALIGNAS(sizeof(int32_t)) char __align;
} glsl_int;

typedef union {
    uint32_t x;
    uint32_t r;
    uint32_t at[1];
    OW_ALIGNAS(sizeof(uint32_t)) char __align;
} glsl_uint;

typedef union {
    int32_t x;
    int32_t r;
    int32_t at[1];
    OW_ALIGNAS(sizeof(int32_t)) char __align;
} glsl_bool;

typedef union {
    struct {
        float x, y;
    };
    struct {
        float r, g;
    };
    float at[2];
    float __std140[2];
    OW_ALIGNAS(OW_STD140_ALIGNOF_VEC(float, 2)) char __align;
} glsl_vec2;

typedef union {
    struct {
        float x, y, z;
    };
    struct {
        float r, g, b;
    };
    float at[3];
    float __std140[4];
    OW_ALIGNAS(OW_STD140_ALIGNOF_VEC(float, 3)) char __align;
} glsl_vec3;

typedef union {
    struct {
        float x, y, z, w;
    };
    struct {
        float r, g, b, a;
    };
    float at[4];
    float __std140[4];
    OW_ALIGNAS(OW_STD140_ALIGNOF_VEC(float, 4)) char __align;
} glsl_vec4;

typedef union {
    struct {
        double x, y;
    };
    struct {
        double r, g;
    };
    double at[2];
    double __std140[2];
    OW_ALIGNAS(OW_STD140_ALIGNOF_VEC(double, 2)) char __align;
} glsl_dvec2;

typedef union {
    struct {
        double x, y, z;
    };
    struct {
        double r, g, b;
    };
    double at[3];
    double __std140[4];
    OW_ALIGNAS(OW_STD140_ALIGNOF_VEC(double, 3)) char __align;
} glsl_dvec3;

typedef union {
    struct {
        double x, y, z, w;
    };
    struct {
        double r, g, b, a;
    };
    double at[4];
    double __std140[4];
    OW_ALIGNAS(OW_STD140_ALIGNOF_VEC(double, 4)) char __align;
} glsl_dvec4;

typedef union {
    struct {
        int32_t x, y;
    };
    struct {
        int32_t r, g;
    };
    int32_t at[2];
    int32_t __std140[2];
    OW_ALIGNAS(OW_STD140_ALIGNOF_VEC(int32_t, 2)) char __align;
} glsl_ivec2;

typedef union {
    struct {
        int32_t x, y, z;
    };
    struct {
        int32_t r, g, b;
    };
    int32_t at[3];
    int32_t __std140[4];
    OW_ALIGNAS(OW_STD140_ALIGNOF_VEC(int32_t, 3)) char __align;
} glsl_ivec3;

typedef union {
    struct {
        int32_t x, y, z, w;
    };
    struct {
        int32_t r, g, b, a;
    };
    int32_t at[4];
    int32_t __std140[4];
    OW_ALIGNAS(OW_STD140_ALIGNOF_VEC(int32_t, 4)) char __align;
} glsl_ivec4;

typedef union {
    struct {
        uint32_t x, y;
    };
    struct {
        uint32_t r, g;
    };
    uint32_t at[2];
    uint32_t __std140[2];
    OW_ALIGNAS(OW_STD140_ALIGNOF_VEC(uint32_t, 2)) char __align;
} glsl_uvec2;

typedef union {
    struct {
        uint32_t x, y, z;
    };
    struct {
        uint32_t r, g, b;
    };
    uint32_t at[3];
    uint32_t __std140[4];
    OW_ALIGNAS(OW_STD140_ALIGNOF_VEC(uint32_t, 3)) char __align;
} glsl_uvec3;

typedef union {
    struct {
        uint32_t x, y, z, w;
    };
    struct {
        uint32_t r, g, b, a;
    };
    uint32_t at[4];
    uint32_t __std140[4];
    OW_ALIGNAS(OW_STD140_ALIGNOF_VEC(uint32_t, 4)) char __align;
} glsl_uvec4;

typedef union {
    struct {
        int32_t x, y;
    };
    struct {
        int32_t r, g;
    };
    int32_t at[2];
    int32_t __std140[2];
    OW_ALIGNAS(OW_STD140_ALIGNOF_VEC(int32_t, 2)) char __align;
} glsl_bvec2;

typedef union {
    struct {
        int32_t x, y, z;
    };
    struct {
        int32_t r, g, b;
    };
    int32_t at[3];
    int32_t __std140[4];
    OW_ALIGNAS(OW_STD140_ALIGNOF_VEC(int32_t, 3)) char __align;
} glsl_bvec3;

typedef union {
    struct {
        int32_t x, y, z, w;
    };
    struct {
        int32_t r, g, b, a;
    };
    int32_t at[4];
    int32_t __std140[4];
    OW_ALIGNAS(OW_STD140_ALIGNOF_VEC(int32_t, 4)) char __align;
} glsl_bvec4;

typedef union {
    glsl_vec4 col[2];
    float at[2][4];
} glsl_mat2x2;

typedef glsl_mat2x2 glsl_mat2;

typedef union {
    glsl_vec4 col[3];
    float at[3][4];
} glsl_mat3x3;

typedef glsl_mat3x3 glsl_mat3;

typedef union {
    glsl_vec4 col[4];
    float at[4][4];
} glsl_mat4x4;

typedef glsl_mat4x4 glsl_mat4;

typedef union {
    glsl_vec4 col[2];
    float at[2][4];
} glsl_mat2x3;

typedef union {
    glsl_vec4 col[2];
    float at[2][4];
} glsl_mat2x4;

typedef union {
    glsl_vec4 col[3];
    float at[3][4];
} glsl_mat3x2;

typedef union {
    glsl_vec4 col[3];
    float at[3][4];
} glsl_mat3x4;

typedef union {
    glsl_vec4 col[4];
    float at[4][4];
} glsl_mat4x2;

typedef union {
    glsl_vec4 col[4];
    float at[4][4];
} glsl_mat4x3;

typedef union {
    glsl_dvec4 col[2];
    double at[2][4];
} glsl_dmat2x2;

typedef glsl_dmat2x2 glsl_dmat2;

typedef union {
    glsl_dvec4 col[3];
    double at[3][4];
} glsl_dmat3x3;

typedef glsl_dmat3x3 glsl_dmat3;

typedef union {
    glsl_dvec4 col[4];
    double at[4][4];
} glsl_dmat4x4;

typedef glsl_dmat4x4 glsl_dmat4;

typedef union {
    glsl_dvec4 col[2];
    double at[2][4];
} glsl_dmat2x3;

typedef union {
    glsl_dvec4 col[2];
    double at[2][4];
} glsl_dmat2x4;

typedef union {
    glsl_dvec4 col[3];
    double at[3][4];
} glsl_dmat3x2;

typedef union {
    glsl_dvec4 col[3];
    double at[3][4];
} glsl_dmat3x4;

typedef union {
    glsl_dvec4 col[4];
    double at[4][4];
} glsl_dmat4x2;

typedef union {
    glsl_dvec4 col[4];
    double at[4][4];
} glsl_dmat4x3;

#define OW_DEFINE_ARR16(NAME, T)         \
    typedef struct {                     \
        union {                          \
            T v;                         \
            uint8_t __pad[16];           \
            OW_ALIGNAS(16) char __align; \
        };                               \
    } NAME;

#define OW_DEFINE_ARR32(NAME, T)         \
    typedef struct {                     \
        union {                          \
            T v;                         \
            uint8_t __pad[32];           \
            OW_ALIGNAS(32) char __align; \
        };                               \
    } NAME;

#define OW_DEFINE_ARRM(NAME, T, STRIDE_BYTES, ALIGN_BYTES) \
    typedef struct {                                       \
        union {                                            \
            T v;                                           \
            uint8_t __pad[(STRIDE_BYTES)];                 \
            OW_ALIGNAS(ALIGN_BYTES) char __align;          \
        };                                                 \
    } NAME;

OW_DEFINE_ARR16(glsl_array_float, glsl_float);
OW_DEFINE_ARR16(glsl_array_int, glsl_int);
OW_DEFINE_ARR16(glsl_array_uint, glsl_uint);
OW_DEFINE_ARR16(glsl_array_bool, glsl_bool);

OW_DEFINE_ARR16(glsl_array_vec2, glsl_vec2);
OW_DEFINE_ARR16(glsl_array_vec3, glsl_vec3);
OW_DEFINE_ARR16(glsl_array_vec4, glsl_vec4);

OW_DEFINE_ARR16(glsl_array_ivec2, glsl_ivec2);
OW_DEFINE_ARR16(glsl_array_ivec3, glsl_ivec3);
OW_DEFINE_ARR16(glsl_array_ivec4, glsl_ivec4);

OW_DEFINE_ARR16(glsl_array_uvec2, glsl_uvec2);
OW_DEFINE_ARR16(glsl_array_uvec3, glsl_uvec3);
OW_DEFINE_ARR16(glsl_array_uvec4, glsl_uvec4);

OW_DEFINE_ARR16(glsl_array_bvec2, glsl_bvec2);
OW_DEFINE_ARR16(glsl_array_bvec3, glsl_bvec3);
OW_DEFINE_ARR16(glsl_array_bvec4, glsl_bvec4);

OW_DEFINE_ARR32(glsl_array_double, glsl_double);
OW_DEFINE_ARR32(glsl_array_dvec2, glsl_dvec2);
OW_DEFINE_ARR32(glsl_array_dvec3, glsl_dvec3);
OW_DEFINE_ARR32(glsl_array_dvec4, glsl_dvec4);

OW_DEFINE_ARRM(glsl_array_mat2, glsl_mat2, 2 * 16, 16);
OW_DEFINE_ARRM(glsl_array_mat3, glsl_mat3, 3 * 16, 16);
OW_DEFINE_ARRM(glsl_array_mat4, glsl_mat4, 4 * 16, 16);

OW_DEFINE_ARRM(glsl_array_mat2x2, glsl_mat2x2, 2 * 16, 16);
OW_DEFINE_ARRM(glsl_array_mat2x3, glsl_mat2x3, 2 * 16, 16);
OW_DEFINE_ARRM(glsl_array_mat2x4, glsl_mat2x4, 2 * 16, 16);
OW_DEFINE_ARRM(glsl_array_mat3x2, glsl_mat3x2, 3 * 16, 16);
OW_DEFINE_ARRM(glsl_array_mat3x3, glsl_mat3x3, 3 * 16, 16);
OW_DEFINE_ARRM(glsl_array_mat3x4, glsl_mat3x4, 3 * 16, 16);
OW_DEFINE_ARRM(glsl_array_mat4x2, glsl_mat4x2, 4 * 16, 16);
OW_DEFINE_ARRM(glsl_array_mat4x3, glsl_mat4x3, 4 * 16, 16);
OW_DEFINE_ARRM(glsl_array_mat4x4, glsl_mat4x4, 4 * 16, 16);

OW_DEFINE_ARRM(glsl_array_dmat2, glsl_dmat2, 2 * 32, 32);
OW_DEFINE_ARRM(glsl_array_dmat3, glsl_dmat3, 3 * 32, 32);
OW_DEFINE_ARRM(glsl_array_dmat4, glsl_dmat4, 4 * 32, 32);

OW_DEFINE_ARRM(glsl_array_dmat2x2, glsl_dmat2x2, 2 * 32, 32);
OW_DEFINE_ARRM(glsl_array_dmat2x3, glsl_dmat2x3, 2 * 32, 32);
OW_DEFINE_ARRM(glsl_array_dmat2x4, glsl_dmat2x4, 2 * 32, 32);
OW_DEFINE_ARRM(glsl_array_dmat3x2, glsl_dmat3x2, 3 * 32, 32);
OW_DEFINE_ARRM(glsl_array_dmat3x3, glsl_dmat3x3, 3 * 32, 32);
OW_DEFINE_ARRM(glsl_array_dmat3x4, glsl_dmat3x4, 3 * 32, 32);
OW_DEFINE_ARRM(glsl_array_dmat4x2, glsl_dmat4x2, 4 * 32, 32);
OW_DEFINE_ARRM(glsl_array_dmat4x3, glsl_dmat4x3, 4 * 32, 32);
OW_DEFINE_ARRM(glsl_array_dmat4x4, glsl_dmat4x4, 4 * 32, 32);

#endif
