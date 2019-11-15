#!/bin/bash

set -e

mkdir -p _ccache
export CCACHE_BASEDIR="$(pwd)"
export CCACHE_DIR="${CCACHE_BASEDIR}/_ccache"

export PATH="${HOME}/.local/bin:${PATH}"
python3 -m pip install --user meson==0.49.2

meson \
    -Dgtk_doc=true \
    -Dman=true \
    -Dinstalled_tests=true \
    -Dbroadway_backend=true \
    -Dxinerama=yes \
    -Dprint_backends="file,lpr,test,cloudprint,cups" \
    _build

cd _build
ninja

xvfb-run -a -s "-screen 0 1024x768x24" \
    meson test \
        --timeout-multiplier 4 \
        --print-errorlogs \
        --suite=gtk+-3.0 \
        --no-suite=gtk+-3.0:a11y

ninja gail-libgail-util3-doc gdk3-doc gtk3-doc
