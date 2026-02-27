#! /bin/sh

srcdir=${MESON_CURRENT_SOURCE_DIR:-./testsuite/headless}
builddir=${MESON_CURRENT_BUILD_DIR:-.}
outputdir=${builddir}/wayland

mkdir -p ${outputdir}


export GTK_A11Y=none

echo launching dbus session
dbus-run-session sh 2>${outputdir}/dbus-stderr.log <<EOF

export XDG_RUNTIME_DIR="$(mktemp -p $(pwd) -d xdg-runtime-XXXXXX)"

#echo DBUS_SESSION_BUS_ADDRESS=\$DBUS_SESSION_BUS_ADDRESS

echo launching mutter $(which mutter)
mutter --headless --virtual-monitor 1024x768 --no-x11 --wayland-display test3 env GDK_BACKEND=wayland $1 >&${outputdir}/mutter.log

exit \$status

EOF
