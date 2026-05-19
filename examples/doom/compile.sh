#!/bin/sh
set -euo pipefail
cd "$(dirname "$0")"

mkdir -p ./build
cd ./build

if ! [ -n "$WASM_CC" ]; then
    echo "error: WASM_CC is not set"
    exit 1
fi

if ! command -v glslc >/dev/null 2>&1; then
    echo "error: glslc not found"
    exit 1
fi

if ! command -v zip >/dev/null 2>&1; then
    echo "error: zip not found"
    exit 1
fi

call() {
    echo ">" "$@"
    command "$@"
}

# Doomgeneric project including the doom source code
if ! test -e ./doomgeneric; then
    git clone --branch master --depth 1 "https://github.com/ozkl/doomgeneric.git"
    git -C ./doomgeneric apply ../../doomgeneric.patch
fi

# Doom game data
if ! test -e ./doom1.wad; then
    wget -O ./doom1.wad "https://distro.ibiblio.org/slitaz/sources/packages/d/doom1.wad"
fi

CFLAGS="-O2"
SRC_DOOM="dummy am_map doomdef doomstat dstrings d_event d_items d_iwad d_loop d_main d_mode d_net f_finale f_wipe g_game hu_lib hu_stuff info i_cdmus i_endoom i_joystick i_scale i_sound i_system i_timer memio m_argv m_bbox m_cheat m_config m_controls m_fixed m_menu m_misc m_random p_ceilng p_doors p_enemy p_floor p_inter p_lights p_map p_maputl p_mobj p_plats p_pspr p_saveg p_setup p_sight p_spec p_switch p_telept p_tick p_user r_bsp r_data r_draw r_main r_plane r_segs r_sky r_things sha1 sounds statdump st_lib st_stuff s_sound tables v_video wi_stuff w_checksum w_file w_main w_wad z_zone w_file_stdc i_input i_video doomgeneric mus2mid"
OBJ_DOOM=

if ! test -e ./wad.o || test ./doom1.wad -nt wad.o; then
    {
        echo "unsigned char wad_c[] = {"
        cat ./doom1.wad |
            basenc --base16 -w32 |
            tr "[:upper:]" "[:lower:]" |
            sed "s/../0x\0, /g"
        echo "};"
        echo "#include <stdint.h>"
        echo "void *wad_c_ptr = (void*)&wad_c[0];"
        echo "size_t wad_c_len = sizeof(wad_c);"
    } >./wad.c
    call $WASM_CC ./wad.c -c -o ./wad.o
fi

for file in $SRC_DOOM; do
    src="./doomgeneric/doomgeneric/${file}.c"
    obj="./${file}.o"
    if ! test -e "$obj" || test "$src" -nt "$obj"; then
        call $WASM_CC $CFLAGS "$src" -c -o "$obj"
    fi
    OBJ_DOOM="$OBJ_DOOM $obj"
done

call $WASM_CC $CFLAGS ../scene.c ./wad.o $OBJ_DOOM -o ./scene.wasm -I../../../include -I./doomgeneric/doomgeneric/ -Wl,--allow-undefined

call glslc -fshader-stage=vertex ../vertex.glsl -o ./vertex.spv
call glslc -fshader-stage=fragment ../fragment.glsl -o ./fragment.spv

rm -f ../doom.owf
zip ../doom.owf scene.wasm vertex.spv fragment.spv
