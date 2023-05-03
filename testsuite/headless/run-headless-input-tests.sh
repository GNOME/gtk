#! /bin/sh

srcdir=${MESON_CURRENT_SOURCE_DIR:-./testsuite/headless}

dbus-run-session sh <<EOF

#echo DBUS_SESSION_BUS_ADDRESS=\$DBUS_SESSION_BUS_ADDRESS
#echo WAYLAND_DISPLAY=gtk-test

export GTK_A11Y=none
export GIO_USE_VFS=local

mutter --headless --virtual-monitor 1024x768 --no-x11 --wayland-display gtk-test2 >&mutter2.log &
pid=\$!

export WAYLAND_DISPLAY=gtk-test2
export GDK_BACKEND=wayland

python3 ${srcdir}/headless-input-tests.py
status=\$?

kill \$pid

exit \$status

EOF
