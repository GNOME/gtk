Title: Using GTK on Windows
Slug: gtk-windows

The Windows port of GTK is an implementation of GDK (and therefore GTK)
on top of the Win32 API. When compiling GTK on Windows, this backend is
the default.

More information about GTK on Windows, including detailed build
instructions, binary downloads, etc, can be found
[online](https://www.gtk.org/docs/installations/windows/).

## Windows-specific environment variables

The Win32 GDK backend can be influenced with some additional environment
variables.

### GDK_WIN32_TABLET_INPUT_API

If this variable is set, it determines the API that GTK uses for tablet support.
The possible values are:

`none`
: Disables tablet support

`wintab`
: Use the Wintab API

`winpointer`
: Use the Windows Pointer Input Stack API. This is the default.

### GDK_WIN32_PER_MONITOR_HIDPI

By default GTK makes the process Per Monitor v2 DPI aware, so windows are
rescaled when they are moved between monitors with different scaling. This
needs Windows 10 version 1703; on older versions GTK uses system DPI awareness.
Set this variable to `0` to use system DPI awareness everywhere, which keeps
the scale of the primary monitor.

An application manifest or a compatibility setting that already fixes the
DPI awareness of the process takes precedence over both.

### GDK_WIN32_DISABLE_HIDPI

If this variable is set, GTK does not change the DPI awareness of the process
and uses what the application manifest or the user's compatibility settings
specify.

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

