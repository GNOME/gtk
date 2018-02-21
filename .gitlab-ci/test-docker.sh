#!/bin/bash

set -e

pwd
meson -Denable-x11-backend=true -Denable-wayland-backend=true \
    -Denable-broadway-backend=true -Denable-vulkan=yes _build_full
cd _build_full
ninja

xvfb-run -a -s "-screen 0 1024x768x24" \
    meson test \
        --print-errorlogs \
        --suite=gtk+ \
        --no-suite=gtk+:gdk \
        --no-suite=gtk+:gsk \
        --no-suite=gtk+:a11y
