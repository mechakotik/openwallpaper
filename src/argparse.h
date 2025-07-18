#ifndef WD_ARGPARSE_H
#define WD_ARGPARSE_H

void wd_parse_args(int argc, char* argv[]);
void wd_free_args();

const char* wd_get_option(const char* name);
const char* wd_get_wallpaer_path();
const char* wd_get_wallpaper_option(const char* name);

#endif
