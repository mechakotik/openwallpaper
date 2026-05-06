\page wallpaperd wallpaperd

Live wallpaper app made with C and SDL3 GPU, supporting OpenWallpaper scenes as well as video wallpapers.

## Build and install

To build wallpaperd, you will need to install:

- C/C++ compiler
- CMake
- Git
- mpv (*optional, for video and GIF wallpapers*)
- LLVM 17-20 *(optional, for scene AOT compilation)*
- wayland-scanner *(optional, for wlr-layer-shell output support)*
- libpipewire, libspa *(optional, for PipeWire audio visualizer)*
- libpulse-simple *(optional, for PulseAudio audio visualizer)*
- PortAudio *(optional, for PortAudio audio visualizer)*

After you have installed all the dependencies, run the following commands to build and install wallpaperd:

```sh
git clone --depth=1 --recurse-submodules https://github.com/mechakotik/openwallpaper
cd openwallpaper/wallpaperd
mkdir build && cd build
cmake ..
cmake --build . -j$(nproc)
sudo cmake --install .
```

## Usage

To run scene wallpaper:

```sh
wallpaperd [OPTIONS] [SCENE_PATH] [SCENE_OPTIONS]
```

To run video wallpaper:

```sh
wallpaperd [OPTIONS] [VIDEO_PATH]
```

Common options:

- `--display=<display>` -- specify display to draw wallpaper on, ignored in window mode. wallpaperd will render to this display if it's connected, unload all the data and sleep when it's disconnected
- `--pause-hidden` -- pause rendering when wallpaper is not visible, e.g. when fullscreen window is open
- `--pause-on-bat` -- pause rendering when laptop is on battery power
- `--window` -- render to window instead of wallpaper (note: does not require Wayland to work)
- `--list-displays` -- list available displays and exit
- `--help` -- print help message and exit

Scene-specific options:

- `--fps=<fps>` -- limit framerate to `<fps>`. When unspecified, VSync is used
- `--speed=<speed>` - set speed multiplier for scene, e.g. 2 will make scene run twice as fast
- `--prefer-dgpu` -- prefer using discrete GPU for rendering on multi-GPU systems. By default the integrated GPU is used to save power
- `--audio-backend=<backend>` -- override default backend for audio visualization. Available values: `pipewire`, `pulse`, `portaudio`
- `--audio-source=<source>` -- override default input for audio visualization
- `--no-audio` -- disable audio visualization

Video-specific options:

- `--scale-mode=<aspect-crop|aspect-fit|stretch>` -- controls how video is fitted to screen, if screen and video aspect ratios differ
- `--filter=<filter>` -- set video filter, see `mpv --scale-help` for available filter values

## Platform support

Wallpaper output:

- [wlr-layer-shell](https://wayland.app/protocols/wlr-layer-shell-unstable-v1#compositor-support)

Audio visualizer (through [cava](https://github.com/karlstav/cava)):

- PipeWire
- PulseAudio
- PortAudio

`--pause-hidden` support:

- Hyprland (fullscreen and tiled window detection)
- Lock detection through [hyprland-lock-notify](https://wayland.app/protocols/wlr-layer-shell-unstable-v1#compositor-support)

`--pause-on-bat` support:

- Linux

If some of the above features are not supported for your DE, consider contributing implementations for them!

