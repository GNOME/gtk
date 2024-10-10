#!/bin/bash

set -e

# Update everything
pacman --noconfirm -Suy

# Install the required packages
pacman --noconfirm -S --needed \
    base-devel \
    git \
    ${MINGW_PACKAGE_PREFIX}-cc \
    ${MINGW_PACKAGE_PREFIX}-ccache \
    ${MINGW_PACKAGE_PREFIX}-pkgconf \
    ${MINGW_PACKAGE_PREFIX}-gobject-introspection \
    ${MINGW_PACKAGE_PREFIX}-meson \
    ${MINGW_PACKAGE_PREFIX}-adwaita-icon-theme \
    ${MINGW_PACKAGE_PREFIX}-atk \
    ${MINGW_PACKAGE_PREFIX}-cairo \
    ${MINGW_PACKAGE_PREFIX}-gdk-pixbuf2 \
    ${MINGW_PACKAGE_PREFIX}-glib2 \
    ${MINGW_PACKAGE_PREFIX}-graphene \
    ${MINGW_PACKAGE_PREFIX}-json-glib \
    ${MINGW_PACKAGE_PREFIX}-libepoxy \
    ${MINGW_PACKAGE_PREFIX}-pango \
    ${MINGW_PACKAGE_PREFIX}-fribidi \
    ${MINGW_PACKAGE_PREFIX}-gst-plugins-bad-libs \
    ${MINGW_PACKAGE_PREFIX}-shared-mime-info \
    ${MINGW_PACKAGE_PREFIX}-python-gobject \
    ${MINGW_PACKAGE_PREFIX}-shaderc \
    ${MINGW_PACKAGE_PREFIX}-vulkan \
    ${MINGW_PACKAGE_PREFIX}-vulkan-headers

mkdir -p _ccache
export CCACHE_BASEDIR="$(pwd)"
export CCACHE_DIR="${CCACHE_BASEDIR}/_ccache"

# Build
ccache --zero-stats
ccache --show-stats
export CCACHE_DISABLE=true
meson setup \
    -Dx11-backend=false \
    -Dwayland-backend=false \
    -Dwin32-backend=true \
    -Dintrospection=enabled \
    -Dgtk:werror=true \
    _build
unset CCACHE_DISABLE

ninja -C _build
ccache --show-stats

tar zcf _build/gtkdll.tar.gz _build/gtk/libgtk*.dll
