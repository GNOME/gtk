# Using GTK with Wayland {#gtk-wayland}

The GDK Wayland backend provides support for running GTK applications
under a Wayland compositor. To run your application in this way, select
the Wayland backend by setting `GDK_BACKEND=wayland`.

On UNIX, the Wayland backend is enabled by default, so you don't need to
do anything special when compiling it, and everything should "just work."

## Wayland-specific environment variables {#wayland-envar}

### WAYLAND_DISPLAY

Specifies the name of the Wayland display to use. Typically, wayland-0
or wayland-1.

### XDG_RUNTIME_DIR

Used to locate the Wayland socket to use.
