# Using GTK with Wayland {#gtk-wayland}

The GDK Wayland backend provides support for running GTK applications
under a Wayland compositor. To run your application in this way, select
the Wayland backend by setting `GDK_BACKEND=wayland`.

On UNIX, the Wayland backend is enabled by default, so you don't need to
do anything special when compiling it, and everything should "just work."

Currently, the Wayland backend does not use any additional environment variables.
