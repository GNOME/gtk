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

# https://gitlab.gnome.org/GNOME/gtk/issues/2243
wget "https://gitlab.gnome.org/creiter/gitlab-ci-win32-runner-v2/raw/master/pango/mingw-w64-$MSYS2_ARCH-pango-1.44.7-1-any.pkg.tar.xz"
pacman --noconfirm -U "mingw-w64-$MSYS2_ARCH-pango-1.44.7-1-any.pkg.tar.xz"

mkdir -p _ccache
export CCACHE_BASEDIR="$(pwd)"
export CCACHE_DIR="${CCACHE_BASEDIR}/_ccache"

# Build
ccache --zero-stats
ccache --show-stats
export CCACHE_DISABLE=true
# FIXME: introspection disabled for now because of
# https://gitlab.gnome.org/GNOME/gobject-introspection/-/issues/340
meson \
    -Dx11-backend=false \
    -Dwayland-backend=false \
    -Dwin32-backend=true \
    -Dvulkan=no \
    -Dintrospection=false \
    --werror \
    _build
unset CCACHE_DISABLE

ninja -C _build
ccache --show-stats
