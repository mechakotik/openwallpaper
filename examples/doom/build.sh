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
if ! sha256sum -c ./hashes 2>&1 >/dev/null; then
    echo "1d7d43be501e67d927e415e0b8f3e29c3bf33075e859721816f652a526cac771 doom.wad" >./hashes
    curl -L -o ./doom.wad "https://archive.org/download/doom-wads/Doom%20%28v1.9%29%20%28Demo%29.zip/DOOM1.WAD"
    sha256sum -c ./hashes
fi

CFLAGS="-O3 -flto"
SRC_DOOM="dummy am_map doomdef doomstat dstrings d_event d_items d_iwad d_loop d_main d_mode d_net f_finale f_wipe g_game hu_lib hu_stuff info i_cdmus i_endoom i_joystick i_scale i_sound i_system i_timer memio m_argv m_bbox m_cheat m_config m_controls m_fixed m_menu m_misc m_random p_ceilng p_doors p_enemy p_floor p_inter p_lights p_map p_maputl p_mobj p_plats p_pspr p_saveg p_setup p_sight p_spec p_switch p_telept p_tick p_user r_bsp r_data r_draw r_main r_plane r_segs r_sky r_things sha1 sounds statdump st_lib st_stuff s_sound tables v_video wi_stuff w_checksum w_file w_main w_wad z_zone w_file_stdc i_input i_video doomgeneric mus2mid"
OBJ_DOOM=

for file in $SRC_DOOM; do
    src="./doomgeneric/doomgeneric/${file}.c"
    obj="./${file}.o"
    if ! test -e "$obj" || test "$src" -nt "$obj"; then
        call $WASM_CC $CFLAGS "$src" -c -o "$obj"
    fi
    OBJ_DOOM="$OBJ_DOOM $obj"
done

call $WASM_CC $CFLAGS ../scene.c $OBJ_DOOM -o ./scene.wasm -I../../../include -I./doomgeneric/doomgeneric/ -mexec-model=reactor -Wl,--allow-undefined

call glslc -fshader-stage=vertex ../vertex.glsl -o ./vertex.spv
call glslc -fshader-stage=fragment ../fragment.glsl -o ./fragment.spv

rm -rf ../scene
mkdir -p ../scene
cp scene.wasm vertex.spv fragment.spv doom.wad ../scene
