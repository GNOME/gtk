#!/bin/bash

set -x
set +e

srcdir=$( pwd )
builddir=$1
setup=$2
suite=$3
nosuite="$4,failing,${setup}_failing,flaky,headless,gsk-compare-broadway"
multiplier=${MESON_TEST_TIMEOUT_MULTIPLIER:-1}
n_processes=${MESON_TEST_MAX_PROCESSES:-$(nproc)}

# Ignore memory leaks lower in dependencies
export LSAN_OPTIONS=suppressions=$srcdir/lsan.supp:print_suppressions=0:detect_leaks=0:allocator_may_return_null=1:symbolize=1
export ASAN_SYMBOLIZER_PATH=/usr/bin/llvm-symbolizer


case "${setup}" in
  x11*)
    dbus-run-session -- \
       xvfb-run -a -s "-screen 0 1024x768x24 -noreset" \
          meson test -C ${builddir} \
                --quiet \
                --timeout-multiplier "${multiplier}" \
                --num-processes "${n_processes}" \
                --print-errorlogs \
                --setup=${setup} \
                --suite=${suite//,/ --suite=} \
                --no-suite=${nosuite//,/ --no-suite=}

    # Store the exit code for the CI run, but always
    # generate the reports
    exit_code=$?
    ;;

  wayland*)
    export XDG_RUNTIME_DIR="$(mktemp -p $(pwd) -d xdg-runtime-XXXXXX)"

    dbus-run-session -- \
      mutter --headless --wayland --no-x11 --virtual-monitor 1024x768 -- \
          meson test -C ${builddir} \
                --quiet \
                --timeout-multiplier "${multiplier}" \
                --num-processes "${n_processes}" \
                --print-errorlogs \
                --setup=${setup} \
                --suite=${suite//,/ --suite=} \
                --no-suite=${nosuite//,/ --no-suite=}
    exit_code=$?
    ;;

  broadway*)
    export XDG_RUNTIME_DIR="$(mktemp -p $(pwd) -d xdg-runtime-XXXXXX)"

    ${builddir}/gdk/broadway/gtk4-broadwayd :5 &
    server=$!
    export BROADWAY_DISPLAY=:5

    dbus-run-session -- \
          meson test -C ${builddir} \
                --quiet \
                --timeout-multiplier "${multiplier}" \
                --num-processes "${n_processes}" \
                --print-errorlogs \
                --setup=${setup} \
                --suite=${suite//,/ --suite=} \
                --no-suite=${nosuite//,/ --no-suite=}

    kill ${server}
    ;;

  *)
    echo "Failed to add ${setup} to .gitlab-ci/run-tests.sh"
    exit 1
    ;;

esac

cd ${builddir}

$srcdir/.gitlab-ci/meson-html-report.py \
            --project-name=gtk \
            --backend="${setup}" \
            --job-id="${CI_JOB_NAME}" \
            --reftest-output-dir="testsuite/reftests/output/${setup}" \
            --output="report-${setup}.html" \
            "meson-logs/testlog-${setup}.json"

exit $exit_code
