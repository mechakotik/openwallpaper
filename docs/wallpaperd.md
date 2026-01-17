\page wallpaperd wallpaperd

OpenWallpaper scene renderer made with C and SDL3 GPU.

## Build and install

To build wallpaperd, you will need to install:

- C/C++ compiler
- CMake
- Git
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

To run the scene:

```sh
wallpaperd [OPTIONS] [WALLPAPER_PATH] [WALLPAPER_OPTIONS]
```

Available options:

- `--display=<display>` -- specify display to draw wallpaper on, ignored in window mode
- `--fps=<fps>` -- limit framerate to `<fps>`. When unspecified, VSync is used
- `--prefer-dgpu` -- prefer using discrete GPU for rendering on multi-GPU systems. By default the integrated GPU is used to save power
- `--pause-hidden` -- pause rendering when wallpaper is not visible, e.g. when fullscreen window is open
- `--pause-on-bat` -- pause rendering when laptop is on battery power
- `--audio-backend=<backend>` -- override default backend for audio visualization. Available values: `pipewire`, `pulse`, `portaudio`
- `--audio-source=<source>` -- override default input for audio visualization
- `--no-audio` -- disable audio visualization
- `--window` -- render to window instead of wallpaper
- `--list-displays` -- list available displays and exit
- `--help` -- print help message and exit

## Platform support

Wallpaper output:

- wlr-layer-shell (KDE Plasma Wayland, Hyprland, wlroots based compositors)

Audio visualizer (through [cava](https://github.com/karlstav/cava)):

- PipeWire
- PulseAudio
- PortAudio

`--pause-hidden` support:

- Hyprland

`--pause-on-bat` support:

- Linux

## Note on porting to other platforms

Rendering is implemented with SDL3 GPU, which has Vulkan, DirectX and Metal backends. So wallpaperd can be easily ported to any platform supporting one of these APIs. All you need to do is to wrap your platform's wallpaper surface in `SDL_Window`. Before trying to do this, you may test rendering by running wallpaperd in windowed mode (`--window` flag), this should just work without extra code.

