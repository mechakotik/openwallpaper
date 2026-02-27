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

## ✨ Features

- 🧩 **WASM scenes** - implement any kind of complex scene logic by just writing code
- 🚀 **High performance** - renderer written in C and SDL3 GPU, with Vulkan, DirectX and Metal backends
- 🕊️ **Lightweight runtime** - only 11 MB with all the dependencies bundled
- ⚙️ **Wallpaper Engine support** - automatically convert Wallpaper Engine `pkg` scenes into OpenWallpaper `owf` scenes
- 🎵 **Audio visualization** - make your scene react to the beat of currently playing music
- 🔋 **Battery friendly** - options to pause rendering on battery or when wallpaper is hidden

## 🚀 Quick start

Download, build and install `wallpaperd` host application:

```sh
git clone --depth=1 --recurse-submodules https://github.com/mechakotik/openwallpaper
cd openwallpaper/wallpaperd
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)
sudo cmake --install .
```

Run `fullscreen-shader` example from OpenWallpaper website ([more examples here](https://openwallpaper.org/examples.html)):

```sh
wget http://openwallpaper.org/fullscreen-shader.owf
wallpaperd fullscreen-shader.owf
```

## ⚙️ Wallpaper Engine support

You may convert Wallpaper Engine `pkg` scenes into OpenWallpaper `owf` scenes using wpe-compile. Instructions on how to do it are available on [wpe-compile page](https://openwallpaper.org/wpe-compile.html) at OpenWallpaper webiste.

Note that not all Wallpaper Engine features are supported, and conversion is not always 100% accurate. Though it can already convert many scenes without issues.

## 🧩 Make your own wallpaper

- [Developer guide](https://openwallpaper.org/overview.html) - step-by-step tutorial for making simple scenes
- [Examples](https://openwallpaper.org/examples.html) - minimal scene examples with source code
- [API reference](https://openwallpaper.org/openwallpaper_8h.html) - documentation for `openwallpaper.h` functionality

## 🤝 Contributing

Contributions are more than welcome! Areas where project needs help:

- Packaging for Linux distributions
- Porting wallpaperd features to more platforms (see [platform support](https://openwallpaper.org/wallpaperd.html))
- Improving wpe-compile accuracy (see [support status](https://openwallpaper.org/wpe-compile.html))
- Making new OpenWallpaper scenes and sharing them

