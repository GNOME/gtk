#!/bin/bash

set +x
set +e

srcdir=$( pwd )
builddir=$1
backend=$2

# Ignore memory leaks lower in dependencies
export LSAN_OPTIONS=suppressions=$srcdir/lsan.supp:print_suppressions=0:verbosity=1:log_threads=1
export G_SLICE=always-malloc

case "${backend}" in
  x11)
    xvfb-run -a -s "-screen 0 1024x768x24 -noreset" \
          meson test -C ${builddir} \
                --timeout-multiplier "${MESON_TEST_TIMEOUT_MULTIPLIER}" \
                --print-errorlogs \
                --setup=${backend} \
                --suite=gtk \
                --no-suite=failing \
                --no-suite=flaky \
                --no-suite=gsk-compare-broadway

    # Store the exit code for the CI run, but always
    # generate the reports
    exit_code=$?

    xvfb-run -a -s "-screen 0 1024x768x24 -noreset" \
          meson test -C ${builddir} \
                --timeout-multiplier "${MESON_TEST_TIMEOUT_MULTIPLIER}" \
                --print-errorlogs \
                --setup=${backend}_unstable \
                --suite=flaky \
                --suite=failing || true
    ;;

  wayland)
    export XDG_RUNTIME_DIR="$(mktemp -p $(pwd) -d xdg-runtime-XXXXXX)"

    weston --backend=headless-backend.so --socket=wayland-5 --idle-time=0 &
    compositor=$!
    export WAYLAND_DISPLAY=wayland-5

    meson test -C ${builddir} \
                --timeout-multiplier "${MESON_TEST_TIMEOUT_MULTIPLIER}" \
                --print-errorlogs \
                --setup=${backend} \
                --suite=gtk \
                --no-suite=failing \
                --no-suite=flaky \
                --no-suite=gsk-compare-broadway
    exit_code=$?

    meson test -C ${builddir} \
                --timeout-multiplier "${MESON_TEST_TIMEOUT_MULTIPLIER}" \
                --print-errorlogs \
                --setup=${backend}_unstable \
                --suite=flaky \
                --suite=failing || true

    kill ${compositor}
    ;;

  waylandgles)
    export XDG_RUNTIME_DIR="$(mktemp -p $(pwd) -d xdg-runtime-XXXXXX)"

    weston --backend=headless-backend.so --socket=wayland-6 --idle-time=0 &
    compositor=$!
    export WAYLAND_DISPLAY=wayland-6

    meson test -C ${builddir} \
                --timeout-multiplier "${MESON_TEST_TIMEOUT_MULTIPLIER}" \
                --print-errorlogs \
                --setup=${backend} \
                --suite=gtk \
                --no-suite=failing \
                --no-suite=flaky \
                --no-suite=gsk-compare-broadway
    exit_code=$?

    meson test -C ${builddir} \
                --timeout-multiplier "${MESON_TEST_TIMEOUT_MULTIPLIER}" \
                --print-errorlogs \
                --setup=${backend}_unstable \
                --suite=flaky \
                --suite=failing || true

    kill ${compositor}
    ;;

  macos)
    meson test -C ${builddir} \
                --timeout-multiplier "${MESON_TEST_TIMEOUT_MULTIPLIER}" \
                --print-errorlogs \
                --setup=${backend} \
                --suite=gtk \
                --no-suite=gsk-compare-opengl
    exit_code=$?
    ;;


  broadway)
    export XDG_RUNTIME_DIR="$(mktemp -p $(pwd) -d xdg-runtime-XXXXXX)"

    ${builddir}/gdk/broadway/gtk4-broadwayd :5 &
    server=$!
    export BROADWAY_DISPLAY=:5

    meson test -C ${builddir} \
                --timeout-multiplier "${MESON_TEST_TIMEOUT_MULTIPLIER}" \
                --print-errorlogs \
                --setup=${backend} \
                --suite=gtk \
                --no-suite=failing \
                --no-suite=flaky \
                --no-suite=gsk-compare-opengl

    # don't let Broadway failures fail the run, for now
    exit_code=0

    meson test -C ${builddir} \
                --timeout-multiplier "${MESON_TEST_TIMEOUT_MULTIPLIER}" \
                --print-errorlogs \
                --setup=${backend}_unstable \
                --suite=flaky \
                --suite=failing || true

    kill ${server}
    ;;

  *)
    echo "Failed to add ${backend} to .gitlab-ci/run-tests.sh"
    exit 1
    ;;

esac

cd ${builddir}

for suffix in "" "_unstable"; do
    $srcdir/.gitlab-ci/meson-junit-report.py \
            --project-name=gtk \
            --backend="${backend}${suffix}" \
            --job-id="${CI_JOB_NAME}" \
            --output="report-${backend}${suffix}.xml" \
            "meson-logs/testlog-${backend}${suffix}.json"
    $srcdir/.gitlab-ci/meson-html-report.py \
            --project-name=gtk \
            --backend="${backend}${suffix}" \
            --job-id="${CI_JOB_NAME}" \
            --reftest-output-dir="testsuite/reftests/output/${backend}${suffix}" \
            --output="report-${backend}${suffix}.html" \
            "meson-logs/testlog-${backend}${suffix}.json"
done

exit $exit_code
