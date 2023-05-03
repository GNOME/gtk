#! /bin/sh

srcdir=${MESON_CURRENT_SOURCE_DIR:-./testsuite/headless}

dbus-run-session sh <<EOF

# echo DBUS_SESSION_BUS_ADDRESS=\$DBUS_SESSION_BUS_ADDRESS
# echo WAYLAND_DISPLAY=gtk-test

export GTK_A11Y=none
export GIO_USE_VFS=local

mutter --headless --no-x11 --wayland-display gtk-test >&mutter.log &
pid=\$!

export WAYLAND_DISPLAY=gtk-test
export GDK_BACKEND=wayland

python ${srcdir}/headless-monitor-tests.py
status=\$?

kill \$pid

exit \$status

EOF
