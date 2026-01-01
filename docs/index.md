OpenWallpaper is a powerful open source platform for interactive live wallpapers. It is somewhat similar to Wallpaper Engine, but while Wallpaper Engine uses complex declarative format for scenes, OpenWallpaper is centered on scripting: a scene is a WebAssembly module that renders to screen using host-implemented graphics API. This gives a next level of freedom to scene creators, while being simple and easily portable to many platforms.

Currently it's in early development stage, the API is unstable and unfinished, so it's not ready for any kind of development on top of it. However, you may want to play around with wallpaperd and wpe-compile which are already pretty usable. It's also your chance to help make the API better with breaking changes! 😁

Ecosystem:

- [wallpaperd](@ref wallpaperd) -- OpenWallpaper scene renderer made with C and SDL3 GPU
- [wpe-compile](@ref wpe-compile) -- A tool to make OpenWallpaper scenes from Wallpaper Engine PKG scenes using code generation.

Format documentation:

- [API reference](@ref openwallpaper.h)

