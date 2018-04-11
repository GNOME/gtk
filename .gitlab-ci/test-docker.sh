#!/bin/bash

set -e

srcdir=$(pwd)

mkdir -p _ccache
export CCACHE_BASEDIR="$(pwd)"
export CCACHE_DIR="${CCACHE_BASEDIR}/_ccache"

ccache --zero-stats
ccache --show-stats
export CCACHE_DISABLE=true
meson \
        -Dx11-backend=true \
        -Dwayland-backend=true \
        -Dbroadway-backend=true \
        -Dvulkan=yes \
        _build $srcdir
unset CCACHE_DISABLE

cd _build

ninja
ccache --show-stats

xvfb-run -a -s "-screen 0 1024x768x24" \
    meson test \
        --print-errorlogs \
        --suite=gtk+ \
        --no-suite=gtk+:gsk \
        --no-suite=gtk+:a11y
