#!/bin/sh

set -e
cd "$(dirname "$0")" || exit 1

./triangle/build.sh
./fullscreen-shader/build.sh
./image/build.sh
./rain/build.sh
./visualizer/build.sh
./doom/build.sh
