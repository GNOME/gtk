#!/bin/bash

set -e

if [[ "$MSYSTEM" == "MINGW32" ]]; then
    export MSYS2_ARCH="i686"
else
    export MSYS2_ARCH="x86_64"
fi

# Update everything
pacman --noconfirm -Suy

# Install the required packages
pacman --noconfirm -S --needed \
    mingw-w64-$MSYS2_ARCH-toolchain \
    mingw-w64-$MSYS2_ARCH-ccache \
    mingw-w64-$MSYS2_ARCH-pkgconf \
    mingw-w64-$MSYS2_ARCH-gobject-introspection \
    mingw-w64-$MSYS2_ARCH-adwaita-icon-theme \
    mingw-w64-$MSYS2_ARCH-atk \
    mingw-w64-$MSYS2_ARCH-cairo \
    mingw-w64-$MSYS2_ARCH-gdk-pixbuf2 \
    mingw-w64-$MSYS2_ARCH-glib2 \
    mingw-w64-$MSYS2_ARCH-json-glib \
    mingw-w64-$MSYS2_ARCH-libepoxy \
    mingw-w64-$MSYS2_ARCH-pango \
    mingw-w64-$MSYS2_ARCH-shared-mime-info \
    mingw-w64-$MSYS2_ARCH-meson \
    mingw-w64-$MSYS2_ARCH-ninja \
    mingw-w64-$MSYS2_ARCH-gtk-doc

# https://github.com/msys2/MINGW-packages/pull/6465
pacman --noconfirm -S --needed mingw-w64-$MSYS2_ARCH-brotli

mkdir -p _ccache
export CCACHE_BASEDIR="$(pwd)"
export CCACHE_DIR="${CCACHE_BASEDIR}/_ccache"

# Build
ccache --zero-stats
ccache --show-stats

meson \
    -Dman=true \
    -Dbroadway_backend=true \
    _build

ninja -C _build

ccache --show-stats
