#!/bin/bash

set -e

srcdir=$(pwd)

mkdir -p _ccache
export CCACHE_BASEDIR="$(pwd)"
export CCACHE_DIR="${CCACHE_BASEDIR}/_ccache"

export CCACHE_DISABLE=1
meson \
        -Dx11-backend=true \
        -Dwayland-backend=true \
        -Dbroadway-backend=true \
        -Dvulkan=yes \
        _build $srcdir
unset CCACHE_DISABLE

cd _build

ninja

xvfb-run -a -s "-screen 0 1024x768x24" \
    meson test \
        --print-errorlogs \
        --suite=gtk+ \
        --no-suite=gtk+:gsk \
        --no-suite=gtk+:a11y
