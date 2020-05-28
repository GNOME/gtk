#!/bin/bash

set +x
set +e

srcdir=$( pwd )
builddir=$1
backend=$2

case "${backend}" in
  x11)
    xvfb-run -a -s "-screen 0 1024x768x24" \
          meson test -C ${builddir} \
                --print-errorlogs \
                --setup=${backend} \
                --suite=gtk \
                --no-suite=gtk:a11y \
                --no-suite=gsk-compare-broadway

    # Store the exit code for the CI run, but always
    # generate the reports
    exit_code=$?
    ;;

  wayland)
    export XDG_RUNTIME_DIR="$(mktemp -p $(pwd) -d xdg-runtime-XXXXXX)"

    weston --backend=headless-backend.so --socket=wayland-5 --idle-time=0 &
    compositor=$!
    export WAYLAND_DISPLAY=wayland-5

    meson test -C ${builddir} \
                --print-errorlogs \
                --setup=${backend} \
                --suite=gtk \
                --no-suite=gtk:a11y \
                --no-suite=gsk-compare-broadway

    exit_code=$?
    kill ${compositor}
    ;;

  broadway)
    export XDG_RUNTIME_DIR="$(mktemp -p $(pwd) -d xdg-runtime-XXXXXX)"

    ${builddir}/gdk/broadway/gtk4-broadwayd :5 &
    server=$!
    export BROADWAY_DISPLAY=:5

    meson test -C ${builddir} \
                --print-errorlogs \
                --setup=${backend} \
                --suite=gtk \
                --no-suite=gtk:a11y \
                --no-suite=gsk-compare-opengl

    # don't let Broadway failures fail the run, for now
    exit_code=0
    kill ${server}
    ;;
esac

cd ${builddir}

$srcdir/.gitlab-ci/meson-junit-report.py \
        --project-name=gtk \
        --backend=${backend} \
        --job-id="${CI_JOB_NAME}" \
        --output=report-${backend}.xml \
        meson-logs/testlog-${backend}.json
$srcdir/.gitlab-ci/meson-html-report.py \
        --project-name=gtk \
        --backend=${backend} \
        --job-id="${CI_JOB_NAME}" \
        --reftest-output-dir="testsuite/reftests/output/${backend}" \
        --output=report-${backend}.html \
        meson-logs/testlog-${backend}.json

exit $exit_code
