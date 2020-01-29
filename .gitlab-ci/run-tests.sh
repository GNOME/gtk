#!/bin/bash

set -e

builddir=$1

cd ${builddir}
xvfb-run -a -s "-screen 0 1024x768x24" \
        meson test --print-errorlogs \
                --suite=gtk \
                --no-suite=gtk:a11y
