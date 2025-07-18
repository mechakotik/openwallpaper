#include <stdio.h>
#include "argparse.h"
#include "error.h"

static void print_help() {
    printf("Usage: wallpaperd [OPTIONS] [WALLPAPER_PATH] [WALLPAPER_OPTIONS]\n");
    printf("Interactive live wallpaper daemon\n\n");

    printf("  --backend=<backend>  set the backend to use\n");
    printf("  --fps=<fps>          set target framerate, default is 30\n");
    printf("\n");
    printf("  --list-backends      list available backends and exit\n");
    printf("  --help               display help and exit\n");
}

int main(int argc, char* argv[]) {
    wd_parse_args(argc, argv);

    if(wd_get_option("help") != NULL) {
        print_help();
        wd_free_args();
        return 0;
    }

    if(wd_get_option("list-backends") != NULL) {
        wd_free_args();
        return 0;
    }

    if(wd_get_wallpaer_path() == NULL) {
        WD_ERROR("no wallpaper path provided, see --help");
    }

    wd_free_args();
    return 0;
}
