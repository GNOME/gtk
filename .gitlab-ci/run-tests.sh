#!/bin/bash

set -e

builddir=$1

xvfb-run -a -s "-screen 0 1024x768x24" \
        meson test -C ${builddir} \
                --print-errorlogs \
                --suite=gtk \
                --no-suite=gtk:a11y

# Store the exit code for the CI run, but always
# generate the reports
exit_code=$?

.gitlab-ci/meson-junit-report.py
        --project-name=gtk
        --job-id="${CI_JOB_NAME}"
        --output=${builddir}/report.xml
        ${builddir}/meson-logs/testlog.json
.gitlab-ci/meson-html-report.py
        --project-name=gtk
        --job-id="${CI_JOB_NAME}"
        --reftest-output-dir="${builddir}/testsuite/reftests/output"
        --output=${builddir}/report.html
        ${builddir}/meson-logs/testlog.json

exit $exit_code
