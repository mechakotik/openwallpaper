\page textures Textures

Textures are images stored in GPU memory.

## Creating and updating textures

To create a texture from pixel data, call `ow_create_texture` and fill the texture data with `ow_update_texture` in a copy pass:

```c
ow_texture_id texture = ow_create_texture(&(ow_texture_info){
    .width = 128,
    .height = 128,
    .format = OW_TEXTURE_RGBA8_UNORM,
});

ow_begin_copy_pass();
// assuming "pixels" store 128x128 RGBA8 pixel data
ow_update_texture(pixels, 128, &(ow_texture_update_destination){
    .texture = texture,
    .x = 0,
    .y = 0,
    .w = 128,
    .h = 128,
});
ow_end_copy_pass();
```

The most common scenario is to load texture from an image stored in a scene and archive. To do so, there is a shortcut function `ow_create_texture_from_image`. As this function does update under the hood, it can be called only in a copy pass.

```c
ow_begin_copy_pass();
ow_texture_id texture = ow_create_texture_from_image("image.png", &(ow_texture_info){
    .format = OW_TEXTURE_RGBA8_UNORM,
});
ow_end_copy_pass();
```

It supports PNG and WEBP image formats. Image loaded from a file is converted to a pixel format specified in `ow_texture_info` `format` field. `width` and `height` fields are filled automatically.

You can use `ow_update_texture` to update only specific sub-rectangle of a texture, see `ow_texture_update_destination`.

## Binding texture to draw call

After the texture is created, you can bind it to a draw call for use in shaders. To do so, you will need a sampler that tells the GPU how to read pixels from a texture. To create sampler, use `ow_create_sampler`:

```c
ow_sampler_id sampler = ow_create_sampler(&(ow_sampler_info){
    .min_filter = OW_FILTER_LINEAR,
    .mag_filter = OW_FILTER_LINEAR,
    .mip_filter = OW_FILTER_LINEAR,
    .wrap_x = OW_WRAP_CLAMP,
    .wrap_y = OW_WRAP_CLAMP,
});
```

Here we specify that we want to use linear filtering and clamp texture by X and Y when trying to sample texture outside of its bounds.

After we have sampler, we can bind texture to draw call. To do so, specify both texture and sampler in `ow_bindings_info` that is passed to `ow_render_geometry`:

```c
ow_bindings_info bindings = {
    ...
    .texture_bindings = (ow_texture_binding[]){
        {.slot = 0, .texture = texture, .sampler = sampler},
    },
    .texture_bindings_count = 1,
};
```

## Using texture in shaders

First we need to declare a texture with `uniform sampler2D`. Then we can use `texture` function to sample texture at given coordinates. In texture coordinate system, up left corner has (0, 0) coordinates, and down right corner has (1, 1) coordinates. Here is an example of a fragment shader that uses a texture:

```glsl
#version 460

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 frag_color;

layout(set = 2, binding = 0) uniform sampler2D u_texture;

void main() {
    frag_color = texture(u_texture, v_uv);
}
```

What to specify in `layout(set = ...)`? By convention:

- In vertex shaders, textures are bound to `set = 0`
- In fragment shaders, textures are bound to `set = 2`

## Rendering to texture

By default, render pass renders to the screen, but you can also set it up to use texture as a target. To do this, specify target texture ID in `ow_render_pass_info` `color_target` field.

```c
ow_begin_render_pass(&(ow_render_pass_info){
    .color_target = texture,
    .clear_color = true,
    .clear_color_rgba = {0, 0, 0, 1},
});
...
ow_end_render_pass();
```

All the draw calls in this pass must also specify format of target texture in their pipeline objects. To do so, specify `ow_pipeline_info` `color_target_format` field:

```c
pipeline = ow_create_pipeline(&(ow_pipeline_info){
    ...
    .color_target_format = OW_TEXTURE_RGBA8_UNORM,
});
```

By default, this field is set to `OW_TEXTURE_SWAPCHAIN`, which means the screen texture format. You may also `OW_TEXTURE_SWAPCHAIN` as a format in `ow_create_texture` to create textures of the same format as screen texture.

You can't render to a texture and bind the same texture in `uniform sampler2D` in the same draw call. If you want to apply some shader effects to the texture one after another, a typical pattern is ping-pong: create two temporary textures and render from first to second, then from second to first, etc.

## MIP mapping

TODO

## Anisotropic filtering

TODO

<div class="section_buttons">
 
| Previous          |                              Next |
|:------------------|----------------------------------:|
| [Drawing a triangle](\ref triangle) | [Interactivity](\ref interactivity) |
 
</div>

