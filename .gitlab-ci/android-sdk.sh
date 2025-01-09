#!/bin/bash

#
# Copyright (c) 2024  Florian "sp1rit" <sp1rit@disroot.org>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, see <http://www.gnu.org/licenses/>.
#
# Author: Florian "sp1rit" <sp1rit@disroot.org>
#

set -e
set -x

if [ -z $ANDROID_HOME -o -z $ANDROID_SDKVER ]; then
    echo "ANDROID_HOME and ANDROID_SDKVER env var must be set!"
    exit 1
fi

test -d ${HOME}/.android || mkdir ${HOME}/.android
# there are currently zero user repos
echo 'count=0' > ${HOME}/.android/repositories.cfg
cat <<EOF >> ${HOME}/.android/sites-settings.cfg
@version@=1
@disabled@https\://dl.google.com/android/repository/extras/intel/addon.xml=disabled
@disabled@https\://dl.google.com/android/repository/glass/addon.xml=disabled
@disabled@https\://dl.google.com/android/repository/sys-img/android/sys-img.xml=disabled
@disabled@https\://dl.google.com/android/repository/sys-img/android-tv/sys-img.xml=disabled
@disabled@https\://dl.google.com/android/repository/sys-img/android-wear/sys-img.xml=disabled
@disabled@https\://dl.google.com/android/repository/sys-img/google_apis/sys-img.xml=disabled
EOF

ANDROID_SDKMAJOR=`echo ${ANDROID_SDKVER} | awk -F '.' '{print $1}'`
ANDROID_NDKVER="27.2.12479018"

# accepted licenses

mkdir -p $ANDROID_HOME/licenses/

cat << EOF > $ANDROID_HOME/licenses/android-sdk-license

8933bad161af4178b1185d1a37fbf41ea5269c55

d56f5187479451eabf01fb78af6dfcb131a6481e

24333f8a63b6825ea9c5514f83c2829b004d1fee
EOF

cat <<EOF > $ANDROID_HOME/licenses/android-sdk-preview-license

84831b9409646a918e30573bab4c9c91346d8abd
EOF

cat <<EOF > $ANDROID_HOME/licenses/android-sdk-preview-license-old

79120722343a6f314e0719f863036c702b0e6b2a

84831b9409646a918e30573bab4c9c91346d8abd
EOF

cat <<EOF > $ANDROID_HOME/licenses/intel-android-extra-license

d975f751698a77b662f1254ddbeed3901e976f5a
EOF

SDKMANAGER="sdkmanager --sdk_root=${ANDROID_HOME}" #ANDROID_HOME may not contain a space :(
${SDKMANAGER} \
	"build-tools;${ANDROID_SDKVER}" \
	"ndk;${ANDROID_NDKVER}" \
	"platforms;android-${ANDROID_SDKMAJOR}"

tee "${ANDROID_HOME}/toolchain.cross" <<EOF
[constants]
toolchain='${ANDROID_HOME}/ndk/${ANDROID_NDKVER}/toolchains/llvm/prebuilt/`uname -s | tr "[:upper:]" "[:lower:]"`-`uname -m`/'
EOF
