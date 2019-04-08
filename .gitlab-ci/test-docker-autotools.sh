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
    --enable-xinerama
make -j8
