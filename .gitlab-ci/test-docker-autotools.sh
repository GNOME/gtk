#!/bin/bash

set -e

mkdir -p _ccache
export CCACHE_BASEDIR="$(pwd)"
export CCACHE_DIR="${CCACHE_BASEDIR}/_ccache"

mkdir _build
cd _build
../autogen.sh \
    --enable-cloudproviders \
    --enable-broadway-backend \
    --enable-wayland-backend \
    --enable-x11-backend \
    --enable-xinerama \
    --enable-gtk-doc
make -j8

if [ -n "${DO_DISTCHECK-}" ]; then
  make -j8 check SKIP_GDKTARGET="echo Not actually running tests for now"
  make -j8 distcheck SKIP_GDKTARGET="echo Not actually running tests for now"
fi
