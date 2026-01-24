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

OpenWallpaper is a powerful open source platform for interactive live wallpapers. In OpenWallpaper, a scene is a WebAssembly module that does rendering to screen and other platform-dependent stuff using standardized host-implemented API. This gives a next level of freedom to scene creators, while being simple and easily portable to many platforms.

The API is close to being stabilized, but breaking changes are still possible. Basic ecosystem is in active development phase. [wallpaperd](\ref wallpaperd) (renderer) works on KDE Plasma Wayland, Hyprland and wlroots based compositors. Many Wallpaper Engine pkg scenes may be converted into OpenWallpaper owf scenes with [wpe-compile](\ref wpe-compile), but conversion is not 100% accurate.

Ecosystem:

- [wallpaperd](\ref wallpaperd) -- OpenWallpaper scene renderer made with C and SDL3 GPU
- [wpe-compile](\ref wpe-compile) -- A tool to make OpenWallpaper scenes from Wallpaper Engine PKG scenes using code generation

Format documentation:

- [Examples](\ref examples)
- [Developer guide](\ref overview)
  1. [Overview and first steps](\ref overview)
  2. [Drawing a triangle](\ref triangle)
  3. [Uniforms](\ref uniforms)
  4. [Textures](\ref textures)
  5. [Interactivity](\ref interactivity)
- [API reference](\ref openwallpaper.h)

