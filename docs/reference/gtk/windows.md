Title: Using GTK on Windows
Slug: gtk-windows

The Windows port of GTK is an implementation of GDK (and therefore GTK)
on top of the Win32 API. When compiling GTK on Windows, this backend is
the default.

More information about GTK on Windows, including detailed build
instructions, binary downloads, etc, can be found
[online](https://wiki.gnome.org/Projects/GTK/Win32).

## Windows-specific environment variables

The Win32 GDK backend can be influenced with some additional environment
variables.

### GDK_IGNORE_WINTAB

If this variable is set, GTK doesn't use the Wintab API for tablet support.

### GDK_USE_WINTAB

If this variable is set, GTK uses the Wintab API for tablet support.
This is the default.

## Windows-specific handling of cursors

By default the "system" cursor theme is used. This makes GTK prefer cursors
that Windows currently uses, falling back to Adwaita cursors and (as the last
resort) built-in X cursors.

When any other cursor theme is used, GTK will prefer cursors from that theme,
falling back to Windows cursors and built-in X cursors.

Theme can be changed by setting `gtk-cursor-theme-name` GTK setting. Users
can override GTK settings in the `settings.ini` file or at runtime in the
GTK Inspector.

Themes are loaded from normal Windows variants of the XDG locations:
`%HOME%/icons/THEME/cursors`,
`%APPDATA%/icons/THEME/cursors`,
`RUNTIME_PREFIX/share/icons/THEME/cursors`

The `gtk-cursor-theme-size` setting is ignored, GTK will use
the cursor size that Windows tells it to use.

