#!/bin/sh
set -e
cd "$(dirname "$0")" || exit 1

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

$WASM_CC scene.c -o scene.wasm -I../../include -Wl,--allow-undefined
glslc -fshader-stage=vertex vertex.glsl -o vertex.spv
glslc -fshader-stage=fragment fragment.glsl -o fragment.spv
rm -f visualizer.owf
zip visualizer.owf scene.wasm vertex.spv fragment.spv
rm -f scene.wasm vertex.spv fragment.spv
