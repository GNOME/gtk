#!/bin/bash

set -e

mkdir -p _ccache
export CCACHE_BASEDIR="$(pwd)"
export CCACHE_DIR="${CCACHE_BASEDIR}/_ccache"

export PATH="${HOME}/.local/bin:${PATH}"
python3 -m pip install --user meson==0.60

meson \
    -Dinstalled_tests=true \
    -Dbroadway_backend=true \
    -Dx11_backend=true \
    -Dwayland_backend=true \
    -Dxinerama=yes \
    -Dprint_backends="file,lpr,test,cups" \
    ${EXTRA_MESON_FLAGS:-} \
    _build

cd _build
ninja

if [ -n "${DO_TEST-}" ]; then
  # Meson < 0.57 can't exclude suites in a test_setup() so we have to
  # explicitly leave out the failing and flaky suites.
  xvfb-run -a -s "-screen 0 1024x768x24" \
    meson test \
        --timeout-multiplier 4 \
        --print-errorlogs \
        --suite=gtk+-3.0 \
        --no-suite=flaky \
        --no-suite=failing

  # We run the flaky and failing tests to get them reported in the CI logs,
  # but if they fail (which we expect they often will), that isn't an error.
  xvfb-run -a -s "-screen 0 1024x768x24" \
    meson test \
        --timeout-multiplier 4 \
        --print-errorlogs \
        --suite=flaky \
        --suite=failing \
    || true
fi

if [ -n "${DO_DISTCHECK-}" ]; then
  meson dist --no-tests
fi
