#include <SDL3/SDL.h>
#include <stdio.h>
#include "argparse.h"
#include "error.h"
#include "object_manager.h"
#include "output.h"

static void print_help() {
    printf("Usage: wallpaperd [OPTIONS] [WALLPAPER_PATH] [WALLPAPER_OPTIONS]\n");
    printf("Interactive live wallpaper daemon\n\n");

    printf("  --output=<output>    set the output to use\n");
    printf("  --fps=<fps>          set target framerate, default is 30\n");
    printf("\n");
    printf("  --list-outputs       list available outputs and exit\n");
    printf("  --help               display help and exit\n");
}

typedef struct Vertex {
    float x, y, z;
    float r, g, b, a;
} Vertex;

static Vertex vertices[] = {
    {0, 0.5, 0, 1, 0, 0, 1},
    {-0.5, -0.5, 0, 0, 1, 0, 1},
    {0.5, -0.5, 0, 0, 0, 1, 1},
};

int main(int argc, char* argv[]) {
    wd_parse_args(argc, argv);

    if(wd_get_option("help") != NULL) {
        print_help();
        wd_quit(0);
    }

    if(wd_get_option("list-outputs") != NULL) {
        wd_list_outputs();
        wd_quit(0);
    }

    if(wd_get_wallpaper_path() == NULL) {
        WD_ERROR("no wallpaper path provided, see --help");
    }

    wd_init_output();
    SDL_Window* window = wd_get_output_window();

    wd_quit(0);
}
