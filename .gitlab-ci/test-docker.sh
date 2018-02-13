#!/bin/bash

set -e

meson -Denable-x11-backend=true -Denable-wayland-backend=true \
    -Denable-broadway-backend=true -Denable-vulkan=yes _build_full
cd _build_full
ninja
