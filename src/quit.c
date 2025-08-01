#include "quit.h"
#include <stdlib.h>
#include "argparse.h"
#include "object_manager.h"
#include "output.h"

void wd_quit(int exit_code) {
    wd_free_args();
    wd_free_output();
    wd_free_object_manager();
    exit(exit_code);
}
