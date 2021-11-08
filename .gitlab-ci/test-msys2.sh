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
    base-devel \
    git \
    mingw-w64-$MSYS2_ARCH-toolchain \
    mingw-w64-$MSYS2_ARCH-ccache \
    mingw-w64-$MSYS2_ARCH-pkg-config \
    mingw-w64-$MSYS2_ARCH-gobject-introspection \
    mingw-w64-$MSYS2_ARCH-meson \
    mingw-w64-$MSYS2_ARCH-adwaita-icon-theme \
    mingw-w64-$MSYS2_ARCH-atk \
    mingw-w64-$MSYS2_ARCH-cairo \
    mingw-w64-$MSYS2_ARCH-gdk-pixbuf2 \
    mingw-w64-$MSYS2_ARCH-glib2 \
    mingw-w64-$MSYS2_ARCH-graphene \
    mingw-w64-$MSYS2_ARCH-json-glib \
    mingw-w64-$MSYS2_ARCH-libepoxy \
    mingw-w64-$MSYS2_ARCH-pango \
    mingw-w64-$MSYS2_ARCH-fribidi \
    mingw-w64-$MSYS2_ARCH-gst-plugins-bad \
    mingw-w64-$MSYS2_ARCH-shared-mime-info

mkdir -p _ccache
export CCACHE_BASEDIR="$(pwd)"
export CCACHE_DIR="${CCACHE_BASEDIR}/_ccache"

# https://gitlab.gnome.org/GNOME/gtk/-/issues/2243
# https://gitlab.gnome.org/GNOME/gtk/-/issues/3002

if ! pkg-config --atleast-version=2.66.0 glib-2.0; then
    git clone https://gitlab.gnome.org/GNOME/glib.git _glib
    meson setup _glib_build _glib
    meson compile -C _glib_build
    meson install -C _glib_build
fi
pkg-config --modversion glib-2.0

if ! pkg-config --atleast-version=1.49.3 pango; then
    git clone https://gitlab.gnome.org/GNOME/pango.git _pango
    meson setup _pango_build _pango
    meson compile -C _pango_build
    meson install -C _pango_build
fi
pkg-config --modversion pango

# Build
ccache --zero-stats
ccache --show-stats
export CCACHE_DISABLE=true
meson \
    -Dx11-backend=false \
    -Dwayland-backend=false \
    -Dwin32-backend=true \
    -Dvulkan=disabled \
    -Dintrospection=enabled \
    -Dgtk:werror=true \
    _build
unset CCACHE_DISABLE

ninja -C _build
ccache --show-stats
