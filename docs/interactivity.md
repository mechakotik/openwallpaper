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

TODO

<div class="section_buttons">
 
| Previous          |                              Next |
|:------------------|----------------------------------:|
| [Textures](\ref textures) | |
 
</div>

