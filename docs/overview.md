\page overview Overview and first steps

OpenWallpaper scene file format is `.owf`. It is a ZIP archive containing `scene.wasm` WebAssembly module and any kind of asset files you need. In `scene.wasm`, you must implement two functions:

- `void init()` -- called once when the scene is initializing, here you can load assets, create resources, etc.
- `void update(float delta)` -- called each frame and should update/redraw the scene

In module, you can call any `extern` function from `openwallpaper.h`. These functions are implemented by the host application and encapsulate all the platform-specific logic, so `.owf` is completely platform-independent.

## Minimal scene module

Let's make a basic scene module that does not draw anything and just prints "Hello, world" in console. In this guide we will write in C, but you can use any language that can be compiled to WebAssembly. First, let's get necessary tools:

- Install [wasi-sdk](https://github.com/WebAssembly/wasi-sdk/releases). In this tutorial we assume it's installed to /opt/wasi-sdk
- Install [wallpaperd](\ref wallpaperd)
- Install any ZIP archiving tool, if you somehow don't have one

Create `scene.c` file with the following content:

```c
#include <stdio.h>

__attribute__((export_name("init"))) void init() {
    printf("Hello, world!\n");
}

__attribute__((export_name("update"))) void update(float delta) {}
```

Compile it to WASM module with following command:

```sh
/opt/wasi-sdk/bin/clang scene.c -o scene.wasm -Wl,--allow-undefined
```

Compress the resulting `scene.wasm` module into ZIP archive with your preferred tool and name the archive e.g. `scene.owf`. Then, run it with wallpaperd:

```sh
wallpaperd scene.owf
```

You should see a black screen wallpaper and "Hello, world!" output in console.

## Using OpenWallpaper API

Let's get screen resolution using [ow_get_screen_size](\ref ow_get_screen_size) function from OpenWallpaper API. Download openwallpaper.h header and place it in your working directory. Write following code in `scene.c`:

```c
#include <stdio.h>
#include "openwallpaper.h"

__attribute__((export_name("init"))) void init() {
    uint32_t width, height;
    ow_get_screen_size(&width, &height);
    printf("width = %d, height = %d\n", width, height);
}

__attribute__((export_name("update"))) void update(float delta) {}
```

Rebuild and run the scene with wallpaperd, like before. You should see a black screen wallpaper and your screen resolution in console.

<div class="section_buttons">
 
| Previous          |                              Next |
|:------------------|----------------------------------:|
| [Home](\ref index) | [Drawing a triangle](\ref triangle) |
 
</div>

