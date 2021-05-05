#!/bin/bash

set -eux -o pipefail

xcodebuild -version || :
xcodebuild -showsdks || :

system_profiler SPSoftwareDataType || :
