#! /bin/sh

export XDG_RUNTIME_DIR="$(mktemp -p $(pwd) -d xdg-runtime-XXXXXX)"
export GTK_DEBUG=general-info

dbus-run-session -- \
  mutter --headless --wayland --no-x11 --virtual-monitor 1024x768 -- \
    $1/demos/gtk-demo/gtk4-demo --autoquit >gtk-general-info.md
