\htmlonly
<div style="display:flex; justify-content:center;">
    <div style="width:100%; max-width:640px; aspect-ratio:16/9;">
        <iframe
            src="demo.mp4"
            style="width:100%; height:100%; border:0;"
            allowfullscreen>
        </iframe>
    </div>
</div>
\endhtmlonly

OpenWallpaper is an open platform for live wallpapers, inspired by Wallpaper Engine and aiming to become a viable open source alternative to it. It supports KDE Plasma Wayland, Hyprland and wlroots-based compositors through wlr-layer-shell protocol.

- **OpenWallpaper scenes** -- like Wallpaper Engine scenes, but better! Instead of using limited declarative format, a scene is a WebAssembly module - meaning if you can code it, you can make it a wallpaper, while having lightweight runtime and native performance with AOT compilation

- **Wallpaper Engine scenes** -- can be converted to OpenWallpaper scenes with wpe-compile, so OpenWallpaper is also an open source Wallpaper Engine implementation

- **Video and GIF wallpapers** -- supported through libmpv

- **Pause hidden** -- try to detect when wallpaper is covered by another window and can't be visible, and pause rendering, saving CPU and GPU resources (DE-dependent, see [platform support](https://openwallpaper.org/wallpaperd.html))

- **Multiple monitors** -- render to specific display when it's connected, and sleep without wasting resources when it's not

- **Audio visualization** -- scene API exposes audio spectrum data through cava

Ecosystem:

- [wallpaperd](\ref wallpaperd) -- Live wallpaper app, supporting OpenWallpaper scenes as well as video wallpapers
- [wpe-compile](\ref wpe-compile) -- A tool to make OpenWallpaper scenes from Wallpaper Engine pkg scenes using code generation

Scene development:

- [Examples](\ref examples)
- [Developer guide](\ref overview)
  1. [Overview and first steps](\ref overview)
  2. [Drawing a triangle](\ref triangle)
  3. [Uniforms](\ref uniforms)
  4. [Textures](\ref textures)
  5. [Interactivity](\ref interactivity)
- [API reference](\ref openwallpaper.h)

