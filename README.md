<h1 align="center">
    openwallpaper
</h1>

<div align="center">
    <p>
        <a href="https://github.com/mechakotik/openwallpaper/stargazers">
            <img src="https://img.shields.io/github/stars/mechakotik/openwallpaper?style=for-the-badge&logo=github&color=white">
        </a>
        <img src="https://img.shields.io/badge/c-00599C?style=for-the-badge&logo=c&logoColor=white">
        <img src="https://img.shields.io/badge/wasm-654FF0?style=for-the-badge&logo=webassembly&logoColor=white">
        <img src="https://img.shields.io/badge/wayland-E69A04?style=for-the-badge&logo=wayland&logoColor=white">
        <img src="https://img.shields.io/badge/go-00AED9?style=for-the-badge&logo=go&logoColor=white">
    </p>
    <p>
        <a href="https://openwallpaper.org">Website</a> •
        <a href="https://openwallpaper.org/wallpaperd.html">wallpaperd</a> •
        <a href="https://openwallpaper.org/wpe-compile.html">wpe-compile</a> •
        <a href="https://openwallpaper.org/examples.html">Examples</a>
    </p>
</div>

OpenWallpaper is a powerful open source platform for interactive live wallpapers. Heavily inspired by Wallpaper Engine, it steps away from restrictive declarative scene formats in favour of WebAssembly. In OpenWallpaper, a scene is a WASM module - meaning if you can code it, you can make it a wallpaper!

It runs on Linux Wayland with `wlr-layer-shell` protocol, supporting KDE Plasma Wayland, Hyprland and wlroots-based compositors. More platforms will be added soon.

With wpe-compile, you can convert Wallpaper Engine pkg scenes to OpenWallpaper owf scenes. So OpenWallpaper is both an independent platform and a Wallpaper Engine implementation for Linux.

## Build from source

You will need following dependencies:

- C/C++ compiler
- CMake
- Git
- wayland-scanner *(optional, for wlr-layer-shell output support)*
- libpipewire, libspa *(optional, for PipeWire audio visualizer)*
- libpulse-simple *(optional, for PulseAudio audio visualizer)*
- PortAudio *(optional, for PortAudio audio visualizer)*

For Wallpaper Engine support (wpe-compile):

- Go compiler
- glslc
- WASM C compiler, [wasi-sdk](https://github.com/WebAssembly/wasi-sdk/releases) recommended

Download, build and install `wallpaperd` and `wpe-compile`:

```sh
git clone --depth=1 --recurse-submodules https://github.com/mechakotik/openwallpaper
cd openwallpaper/wallpaperd
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)
sudo cmake --install .

cd ../wpe-compile
go build .
sudo cp wpe-compile /usr/local/bin/
```

## Usage

Run `fullscreen-shader` example from OpenWallpaper website ([more examples here](https://openwallpaper.org/examples.html)):

```sh
wget http://openwallpaper.org/fullscreen-shader.owf
wallpaperd fullscreen-shader.owf
```

Convert Wallpaper Engine `pkg` scene (can be found in `steamapps/workshop/content/431960/`) to OpenWallpaper `owf` scene and run it:

```sh
# replace with your Wallpaper Engine assets and WASM compiler path if needed
export WPE_COMPILE_ASSETS=~/.local/share/Steam/steamapps/common/wallpaper_engine/assets
export WPE_COMPILE_WASM_CC=/opt/wasi-sdk/bin/clang

wpe-compile scene.pkg scene.owf
wallpaperd scene.owf
```

## Make your own scene

- [Developer guide](https://openwallpaper.org/overview.html) - step-by-step tutorial for making simple scenes
- [Examples](https://openwallpaper.org/examples.html) - minimal scene examples with source code
- [API reference](https://openwallpaper.org/openwallpaper_8h.html) - documentation for `openwallpaper.h` functionality

## Contributing

Contributions are more than welcome! Areas where project needs help:

- Packaging for Linux distributions
- Porting wallpaperd features to more platforms (see [platform support](https://openwallpaper.org/wallpaperd.html))
- Improving wpe-compile accuracy (see [support status](https://openwallpaper.org/wpe-compile.html))
- Making new OpenWallpaper scenes and sharing them

