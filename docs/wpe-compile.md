\page wpe-compile wpe-compile

A tool to make OpenWallpaper scenes from Wallpaper Engine PKG scenes using code generation. It supports basic Wallpaper Engine subset used by most scenes (no SceneScript and 3D).

## Build

To build and use wpe-compile, you will need to install:

- Go compiler
- glslc
- WASM C compiler, [wasi-sdk](https://github.com/WebAssembly/wasi-sdk/releases) recommended
- Git

After you have installed all the dependencies, run the following commands to build wpe-compile:

```sh
git clone --recurse-submodules https://github.com/mechakotik/openwallpaper
cd openwallpaper/wpe-compile
go build .
```

## Usage

Assuming you have Steam Wallpaper Engine installed. Get PKG scenes from `SteamLibrary/steamapps/workshop/content/431960`. You will also need Wallpaper Engine assets directory, it is located in `SteamLibrary/steamapps/common/wallpaper_engine/assets`.

Usage example:

```sh
export WPE_COMPILE_ASSETS=/path/to/assets
export WPE_COMPILE_WASM_CC=/path/to/wasi-sdk/bin/clang
wpe-compile /path/to/scene.pkg /path/to/result.owf
```

Available wpe-compile options:

- `--keep-sources` - keep intermediate C and GLSL sources, which are not needed for rendering but are useful for debugging
- `--particles=<true|false>` - enable/disable particles, disabled by default because of inaccurate implementation

Generated OWF scenes have the following runtime options that you can set when running with wallpaperd:

- `--scale-mode=<stretch|aspect-fit|aspect-crop>` - controls how the scene is fitted in screen when its aspect ratio does not match screen aspect ratio, defaults to `aspect-crop`

## Support status

This is currently an early WIP, and scene behaviour will probably be different from what you get in Wallpaper Engine. Accurate reverse engineering involves a lot of work, so contributions are welcome!

You can check out this list to see what is not yet implemented or can be improved. (*) means that the feature is not implemented completely, or the behavior is not fully accurate to Wallpaper Engine.

- [x] Layer
  - [x] Image
  - [x] Fullscreen
  - [x] Effects (*)

- [x] Texture
  - [x] Image
  - [x] Spritesheet
    - [ ] Frame blending
  - [ ] Video texture

- [x] Camera
  - [x] Orthographic
  - [x] Perspective (*)
  - [x] Parallax (*)
  - [ ] Shake
  - [ ] Fade

- [x] Particles
  - [x] Renderer
    - [x] Sprite
    - [ ] Sprite Trail
    - [ ] Rope
    - [ ] Rope Trail
  - [x] Emitters
    - [x] Sphere random
    - [ ] Box random
    - [ ] Layer image
  - [x] Initializers
    - [x] Lifetime random
    - [x] Size random
    - [x] Color random
    - [ ] HSV color random
    - [ ] Color list
    - [x] Alpha random
    - [x] Velocity random
    - [ ] Inherit control point velocity
    - [x] Turbulent velocity random (*)
    - [x] Rotation random
    - [ ] Position offset random
    - [x] Angular velocity random
    - [ ] Position around control point
    - [ ] Position between control points
    - [ ] Remap initial value
    - [ ] Inherit value from event
  - [x] Operators
    - [x] Movement
    - [x] Angular movement
    - [x] Alpha fade
    - [x] Size change
    - [x] Color change
    - [ ] Alpha change
    - [x] Oscillate position
    - [ ] Oscillate alpha
    - [ ] Oscillate size
    - [ ] Control point force
    - [ ] Maintain distance to control point
    - [ ] Maintain distance between control points
    - [ ] Reduce movement near control point
    - [ ] Turbulence
    - [ ] Vortex
    - [ ] Boids
    - [ ] Cap velocity
    - [ ] Remap value
    - [ ] Inherit value from event
    - [ ] Collision operators
    - [ ] Operator blending
  - [ ] Children
  - [ ] Control points

- [ ] Timeline animations
- [ ] Puppet warp
- [ ] Lighting and reflections
- [ ] Sound
- [ ] User properties
- [ ] 3D (not planned because of immense complexity)
- [ ] SceneScript (not planned because of immense complexity)

## Credits

This would not have been possible to implement so fast without [RePKG](https://github.com/notscuffed/repkg), [wallpaper-scene-renderer](https://github.com/catsout/wallpaper-scene-renderer) and [linux-wallpaperengine](https://github.com/Almamu/linux-wallpaperengine) which have done some reverse engineering of Wallpaper Engine previously and were used as reference.

