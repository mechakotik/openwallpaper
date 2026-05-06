\page interactivity Interactivity

Here are some OpenWallpaper features to add interactivity to your scene.

## Mouse state

To get mouse cursor position and pressed buttons, use `ow_get_mouse_state` function. It stores cursor position in `x, y: float` and returns a mask of currently pressed buttons. See enum `ow_mouse_button` for available buttons.

```c
float x, y;
uint32_t buttons = ow_get_mouse_state(&x, &y);
if(buttons & OW_BUTTON_LEFT) {
    // left mouse button is pressed
}
```

## Audio spectrum

To get audio spectrum data for visualization, use `ow_get_audio_spectrum` function. You need to specify a number of bars to get and a buffer to store them. `ow_get_audio_spectrum` will fill the buffer with normalized floats from 0 to 1.

```c
float spectrum[16];
ow_get_audio_spectrum(spectrum, 16);
```

You may see an example of `ow_get_audio_spectrum` usage in [visualizer example](\ref examples).

## Scene options

This feature allows to change the behavior of the scene at runtime. Scene options are specified in wallpaperd CLI arguments after the scene path like this:

```
wallpaperd scene.owf --key=value
```

To get a value of a scene option, use `ow_get_option` function. It will return option value as null-terminated string. If option is not specified, it will return `NULL`. If the option is specified but has no value, it will return an empty string `""`.

```c
const char* value = ow_get_option("key");
if(value != NULL) {
    // do something
}
```

<div class="section_buttons">
 
| Previous          |                              Next |
|:------------------|----------------------------------:|
| [Textures](\ref textures) | |
 
</div>

