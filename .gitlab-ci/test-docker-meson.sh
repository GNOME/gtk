#!/bin/bash

set -e

mkdir -p _ccache
export CCACHE_BASEDIR="$(pwd)"
export CCACHE_DIR="${CCACHE_BASEDIR}/_ccache"

export PATH="${HOME}/.local/bin:${PATH}"
python3 -m pip install --user meson==0.49.2

meson _build
ninja -C _build
