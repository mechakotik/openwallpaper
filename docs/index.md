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

OpenWallpaper is a powerful open source platform for interactive live wallpapers. It combines the performance of Wallpaper Engine scenes with the flexibility of web wallpapers. In OpenWallpaper, a scene is a WASM module -- meaning if you can code it, you can make it a wallpaper, platform-independent, with lightweight runtime and near-native performance.

The API is close to being stabilized, but breaking changes are still possible. [wallpaperd](\ref wallpaperd) (renderer) works on most Linux Wayland desktops, including KDE Plasma, Hyprland and wlroots based compositors. Many Wallpaper Engine pkg scenes may be converted into OpenWallpaper owf scenes with [wpe-compile](\ref wpe-compile), but conversion is not 100% accurate. There is a user-friendly GUI in early development.

Ecosystem:

- [wallpaperd](\ref wallpaperd) -- OpenWallpaper scene renderer made with C and SDL3 GPU
- [wpe-compile](\ref wpe-compile) -- A tool to make OpenWallpaper scenes from Wallpaper Engine pkg scenes using code generation

Format documentation:

- [Examples](\ref examples)
- [Developer guide](\ref overview)
  1. [Overview and first steps](\ref overview)
  2. [Drawing a triangle](\ref triangle)
  3. [Uniforms](\ref uniforms)
  4. [Textures](\ref textures)
  5. [Interactivity](\ref interactivity)
- [API reference](\ref openwallpaper.h)

