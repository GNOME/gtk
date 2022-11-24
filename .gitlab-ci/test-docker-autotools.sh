#!/bin/bash

set -e

mkdir -p _ccache
export CCACHE_BASEDIR="$(pwd)"
export CCACHE_DIR="${CCACHE_BASEDIR}/_ccache"
export N_PROCS=$(($(nproc) - 1))

EXTRA_CONFIGURE_OPT=""

# Only enable documentation when distchecking, since it's required
if [ -n "${DO_DISTCHECK-}" ]; then
    EXTRA_CONFIGURE_OPTS="${EXTRA_CONFIGURE_OPTS} --enable-gtk-doc"
fi

NOCONFIGURE=1 ./autogen.sh

mkdir _build
cd _build

../configure \
    --enable-cloudproviders \
    --enable-broadway-backend \
    --enable-wayland-backend \
    --enable-x11-backend \
    --enable-xinerama \
    ${EXTRA_CONFIGURE_OPTS}

make -j${N_PROCS}

if [ -n "${DO_DISTCHECK-}" ]; then
  make -j${N_PROCS} check SKIP_GDKTARGET="echo Not actually running tests for now"
  xvfb-run -a -s "-screen 0 1024x768x24" \
    make -j${N_PROCS} distcheck SKIP_GDKTARGET="echo Not actually running tests for now"
fi
