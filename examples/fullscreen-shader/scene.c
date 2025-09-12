#include <openwallpaper.h>

static struct {
    ow_id vertex_shader;
    ow_id fragment_shader;
    ow_id pipeline;
    float time;
} state = {0};

typedef struct {
    float resolution[2];
    float time;
} UniformData;

__attribute__((export_name("init"))) void init() {
    state.vertex_shader = ow_create_shader_from_file("vertex.spv", OW_SHADER_VERTEX);
    state.fragment_shader = ow_create_shader_from_file("fragment.spv", OW_SHADER_FRAGMENT);

    ow_pipeline_info pipe = {0};
    pipe.vertex_shader = state.vertex_shader;
    pipe.fragment_shader = state.fragment_shader;
    pipe.topology = OW_TOPOLOGY_TRIANGLES;
    state.pipeline = ow_create_pipeline(&pipe);
}

__attribute__((export_name("update"))) void update(float delta) {
    state.time += delta;

    uint32_t screen_width, screen_height;
    ow_get_screen_size(&screen_width, &screen_height);

    UniformData data = {0};
    data.resolution[0] = (float)screen_width;
    data.resolution[1] = (float)screen_height;
    data.time = state.time;

    ow_pass_info pass = {0};
    pass.clear_color = true;
    pass.clear_color_rgba[0] = 0.0f;
    pass.clear_color_rgba[1] = 0.0f;
    pass.clear_color_rgba[2] = 0.0f;
    pass.clear_color_rgba[3] = 1.0f;

    ow_begin_render_pass(&pass);
    ow_push_uniform_data(OW_SHADER_FRAGMENT, 0, &data, sizeof(data));

    ow_bindings_info bindings = {0};
    ow_render_geometry(state.pipeline, &bindings, 0, 3, 1);

    ow_end_render_pass();
}
