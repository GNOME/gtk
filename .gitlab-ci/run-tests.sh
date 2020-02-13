#!/bin/bash

set +x
set +e

srcdir=$( pwd )
builddir=$1

export GDK_BACKEND=x11
xvfb-run -a -s "-screen 0 1024x768x24" \
        dbus-run-session -- \
                meson test -C ${builddir} \
                        --print-errorlogs \
                        --suite=gtk \
                        --no-suite=gtk:a11y

# Store the exit code for the CI run, but always
# generate the reports
exit_code=$?

cd ${builddir}

$srcdir/.gitlab-ci/meson-junit-report.py \
        --project-name=gtk \
        --job-id="${CI_JOB_NAME}" \
        --output=report.xml \
        meson-logs/testlog.json
$srcdir/.gitlab-ci/meson-html-report.py \
        --project-name=gtk \
        --job-id="${CI_JOB_NAME}" \
        --reftest-output-dir="testsuite/reftests/output" \
        --output=report.html \
        meson-logs/testlog.json

exit $exit_code
