\page wallpaperd wallpaperd

OpenWallpaper scene renderer made with C and SDL3 GPU.

## Build and install

Install C/C++ compiler, CMake and Git, then run:

```sh
git clone --recurse-submodules https://github.com/mechakotik/openwallpaper
cd openwallpaper/wallpaperd
mkdir build && cd build
cmake .. -DWD_WLROOTS=ON
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

- wlr-layer-shell (Plasma Wayland, Hyprland, wlroots based compositors)

Audio visualizer (through [cava](https://github.com/karlstav/cava)):

- PipeWire
- PulseAudio
- PortAudio

`--pause-hidden` support:

- Hyprland

`--pause-on-bat` support:

- Linux

