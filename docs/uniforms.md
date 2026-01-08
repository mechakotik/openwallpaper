\page uniforms Uniforms

So far we have only used vertex attributes as shader input data. Uniforms are another kind of input data that act like global variables in shaders. They are common for all vertex/fragment shader passes and can be updated quickly without beginning a copy pass.

Uniforms are declared in GLSL using the `uniform` keyword like this:

```glsl
layout(std140, set = 1, binding = 0) uniform uniforms_t {
    vec4 transform;
    vec2 position;
    float size;
    vec4 color;
};
```

Contrary to vertex attributes, you don't need to provide data types and offsets to the API. Instead, you just push raw uniform data before draw call. The data that is being pushed must respect std140 layout conventions.

Typically you want to make a struct equivalent of uniform block type to fill the data conveniently. As C structs are not compatible with std140 layout, sometimes you need to reorder uniforms or add offset fields to align data properly. Here is a C struct equivalent of `uniforms_t` block described earlier:

```c
typedef struct {
    float transform[4];
    float position[2];
    float size;
    float _offset; // vec4 must be aligned to 16 bytes
    float color[4];
} uniforms_t;
```

To push uniform data, call `ow_push_vertex_uniform_data` or `ow_push_fragment_uniform_data` before draw call, specifying the binding index and data to push.

```c
uniforms_t uniforms = {
    .transform = {1.0f, 1.0f, 1.0f, 1.0f},
    .position = {0.0f, 0.0f},
    .size = 1.0f,
    .color = {1.0f, 1.0f, 1.0f, 1.0f},
};

ow_push_vertex_uniform_data(0, &uniforms, sizeof(uniforms));
```

What to specify in `layout(std140, set = ...)`? By convention:

- In vertex shaders, uniform blocks are bound to `set = 1`
- In fragment shaders, uniform blocks are bound to `set = 3`

<div class="section_buttons">
 
| Previous          |                              Next |
|:------------------|----------------------------------:|
| [Drawing a triangle](\ref triangle) | [Textures](\ref textures) |
 
</div>

