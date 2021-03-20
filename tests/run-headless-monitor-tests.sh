#! /bin/sh

builddir=$(pwd)/build

dbus-run-session sh <<EOF

# echo DBUS_SESSION_BUS_ADDRESS=\$DBUS_SESSION_BUS_ADDRESS
# echo WAYLAND_DISPLAY=gtk-test

mutter --headless --no-x11 --wayland-display gtk-test >&mutter.log &
pid=\$!

export WAYLAND_DISPLAY=gtk-test
export GDK_BACKEND=wayland

export GI_TYPELIB_PATH=$builddir/gtk:/usr/lib64/girepository-1.0
export LD_PRELOAD=$builddir/gtk/libgtk-4.so

python tests/headless-monitor-tests.py
status=\$?

kill \$pid

exit \$status

EOF
