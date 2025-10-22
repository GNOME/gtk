Title: Running and debugging GTK Applications
Slug: gtk-running

## Environment variables

GTK inspects a number of environment variables in addition to
standard variables like `LANG`, `PATH`, `HOME` or `DISPLAY`; mostly
to determine paths to look for certain files. The
[X11](https://docs.gtk.org/gtk4/x11.html#x11-specific-environment-variables),
[Wayland](https://docs.gtk.org/gtk4/wayland.html#wayland-specific-environment-variables),
[Windows](https://docs.gtk.org/gtk4/windows.html#windows-specific-environment-variables) and
[Broadway](https://docs.gtk.org/gtk4/broadway.html#broadway-specific-environment-variables)
GDK backends use some additional environment variables.

Note that environment variables are generally used for debugging
purposes. They are not guaranteed to be API stable, and should not
be used for end-user configuration and customization. If you feel the
need to set one of them programmatically, you should probably ask for
an API to do what you want, instead.

### `GTK_DEBUG`

This variable can be set to a list of debug options, which cause GTK to
print out different types of debugging information.

`actions`
: Actions and menu models

`builder`
: Deprecated GtkBuilder features

`builder-trace`
: Trace GtkBuilder operation

`builder-objects`
: Unused GtkBuilder objects

`css`
: Deprecated CSS features

`geometry`
: Size allocation

`icontheme`
: Icon themes

`iconfallback`
: Information about icon fallback

`keybindings`
: Keyboard shortcuts

`modules`
: Modules and extensions

`printing`
: Printing

`size-request`
: Size requests

`text`
: Text widget internals

`tree`
: Tree widget internals

`constraints`
: Constraints and the constraint solver

`layout`
: Layout managers

`accessibility`
: Accessibility state changes

A number of keys are influencing behavior instead of just logging:

`interactive`
: Open the [interactive debugger](#interactive-debugging)

`no-css-cache`
: Bypass caching for CSS style properties

`touch-ui`
: Show touch ui elements for pointer events

`snapshot`
: Include debug render nodes in the generated snapshots

`invert-text-dir`
: Invert the text direction, compared to the locale

The special value `all` can be used to turn on all debug options.
The special value `help` can be used to obtain a list of all
supported debug options.

### `GTK_PATH`

Specifies a list of directories to search when GTK is looking for
dynamically loaded objects such as input method modules and print
backends. If the path to the dynamically loaded object is given as
an absolute path name, then GTK loads it directly. Otherwise, GTK
goes in turn through the directories in `GTK_PATH`, followed
by the system default directory, which is `libdir/gtk-4.0/modules`.
(If `GTK_EXE_PREFIX` is defined, `libdir` is `$GTK_EXE_PREFIX/lib`.
Otherwise it is the libdir specified when GTK was configured, usually
`/usr/lib`, or `/usr/local/lib`.)

For each directory in this list, GTK actually looks in a subdirectory
`directory/version/host/type`. Where `version` is derived from the
version of GTK (use `pkg-config --variable=gtk_binary_version gtk4`
to determine this from a script), `host` is the architecture on
which GTK was built. (use `pkg-config --variable=gtk_host gtk4` to
determine this from a script), and `type` is a directory specific to
the type of modules; currently it can be `modules`, `immodules` or
`printbackends`, corresponding to the types of modules mentioned
above. Either `version`, `host`, or both may be omitted. GTK looks
first in the most specific directory, then in directories with
fewer components.
The components of `GTK_PATH` are separated by the ':' character on
Linux and Unix, and the ';' character on Windows.

Note that this environment variable is read by GTK 2.x and GTK 3.x
too, which makes it unsuitable for setting it system-wide (or
session-wide), since doing so will cause applications using
different GTK versions to see incompatible modules.

### `GTK_IM_MODULE`

Specifies an IM module to use in preference to the one determined
from the locale. If this isn't set and you are running on the system
that enables `XSETTINGS` and has a value in `Gtk/IMModule`, that will
be used for the default IM module. This also can be a colon-separated
list of input-methods, which GTK will try in turn until it finds one
available on the system.

### `GTK_MEDIA`

Specifies what backend to load for [class@Gtk.MediaFile]. The possible values
depend on what options GTK was built with, and can include 'gstreamer'
and 'none'. If set to 'none', media playback will be unavailable.
The special value 'help' can be used to obtain a list of all supported
media backends.

### `GTK_EXE_PREFIX`

If set, GTK uses `$GTK_EXE_PREFIX/lib` instead of the libdir
configured when GTK was compiled.

### `GTK_DATA_PREFIX`

If set, GTK uses `$GTK_DATA_PREFIX` instead of the prefix
configured when GTK was compiled.

### `GTK_THEME`

If set, makes GTK use the named theme instead of the theme
that is specified by the gtk-theme-name setting. This is intended
mainly for easy debugging of theme issues.

It is also possible to specify a theme variant to load, by appending
the variant name with a colon, like this: `GTK_THEME=Adwaita:dark`.

## Other environment variables

The following environment variables are used by GdkPixbuf, GDK or
Pango, not by GTK itself, but we list them here for completeness
nevertheless.

### `GDK_PIXBUF_MODULE_FILE`

Specifies the file listing the GdkPixbuf loader modules to load.
This environment variable overrides the default value
`libdir/gtk-4.0/4.0.0/loaders.cache` (`libdir` is the sysconfdir
specified when GTK was configured, usually `/usr/lib`.)

The `loaders.cache` file is generated by the
`gdk-pixbuf-query-loaders` utility.

### `GDK_DEBUG`

This variable can be set to a list of debug options, which cause GDK to
print out different types of debugging information.

`misc`
: Miscellaneous information

`events`
: Information about events

`dnd`
: Information about drag-and-drop

`input`
: Information about input (mostly Windows)

`eventloop`
: Information about event loop operation (mostly macOS)

`frames`
: Information about the frame clock

`settings`
: Information about xsettings

`opengl`
: Information about OpenGL

`vulkan`
: Information about Vulkan

`selection`
: Information about selections

`clipboard`
: Information about clipboards

`dmabuf`
: Information about dmabuf handling (Linux-only)

`d3d12`
: Information about Direct3D12 (Windows-only)

`offload`
: Information about subsurfaces and graphics offload (Wayland-only)

A number of options affect behavior instead of logging:

`portals`
: Force the use of [portals](https://docs.flatpak.org/en/latest/portals.html)

`no-portals`
: Disable use of [portals](https://docs.flatpak.org/en/latest/portals.html)

`force-offload`
: Force graphics offload for all textures, even when slower. This allows
  to debug offloading in the absence of dmabufs.

`gl-debug`
: Insert debugging information in OpenGL

`gl-prefer-gl`
: Prefer OpenGL over OpenGL ES. This was the default behavior before GTK 4.14.

`default-settings`
: Force default values for xsettings

`high-depth`
: Use high bit depth rendering if possible

`linear`
: Enable linear rendering

`hdr`
: Force HDR rendering

`no-vsync`
: Repaint instantly (uses 100% CPU with animations)

`color-mgmt`
: Enable color management

`session-mgmt`
: Enable session management

The special value `all` can be used to turn on all debug options. The special
value `help` can be used to obtain a list of all supported debug options.

### `GSK_DEBUG`

This variable can be set to a list of debug options, which cause GSK to
print out different types of debugging information.

`renderer`
: General renderer information

`vulkan`
: Check Vulkan errors

`shaders`
: Information about shaders

`fallback`
: Information about fallback usage in renderers

`cache`
: Information about caching

`verbose`
: Print verbose output while rendering

A number of options affect behavior instead of logging:

`geometry`
: Show borders (when using cairo)

`full-redraw`
: Force full redraws

`staging`
: Use a staging image for texture upload (Vulkan only)

`cairo`
: Overlay error pattern over cairo drawing (finds fallbacks)

`occlusion`
: Overlay highlight over areas optimized via occlusion culling

The special value `all` can be used to turn on all debug options. The special
value `help` can be used to obtain a list of all supported debug options.

### `GDK_BACKEND`

If set, selects the GDK backend to use. Selecting a backend
requires that GTK is compiled with support for that backend.
The following backends can be selected, provided they are
included in the GDK libraries you are using:

`macos`
: Selects the native Quartz backend

`win32`
: Selects the native backend for Microsoft Windows

`x11`
: Selects the native backend for connecting to X11 servers

`broadway`
: Selects the Broadway backend for display in web browsers

`wayland`
: Selects the Wayland backend for connecting to Wayland compositors

This environment variable can contain a comma-separated list of
backend names, which are tried in order. The list may also contain
a `*`, which means: try all remaining backends. The special value
`help` can be used to make GDK print out a list of all available
backends. For more information about selecting backends,
see the [func@Gdk.DisplayManager.get] function.

### `GDK_DISABLE`

This variable can be set to a list of values, which cause GDK to
disable certain features.

`gl`
: Disable OpenGL support

`gl-api`
: Don't allow the use of OpenGL GL API. This forces GLES to be used

`gles-api`
: Don't allow the use of OpenGL GLES API. This forces GL to be used

`egl`
: Don't allow the use of an EGL context

`glx`
: Don't allow the use of GLX

`wgl`
: Don't allow the use of WGL

`vulkan`
: Disable Vulkan support

`dmabuf`
: Disable dmabuf support

`offload`
: Disable graphics offload to subsurfaces

`threads`
: Disables the use of threads where possible

`icon-nodes`
: Disables the svg-to-node conversion for symbolic icons

### `GDK_GL_DISABLE`

This variable can be set to a list of values, which cause GDK to
disable extension features of the OpenGL support.
Note that these features may already be disabled if the GL driver
does not support them.

`debug`
: GL_KHR_debug

`base-instance`
: GL_ARB_base_instance

`buffer-storage`
: GL_EXT_buffer_storage

`external-objects`
: GL_EXT_memory_object and GL_EXT_semaphore

`external-objects-win32`
: GL_EXT_memory_object_win32 and GL_EXT_semaphore_win32

### `GDK_VULKAN_DISABLE`

This variable can be set to a list of values, which cause GDK to
disable features of the Vulkan support.
Note that these features may already be disabled if the Vulkan driver
does not support them.

`dmabuf`
: Never import Dmabufs

`ycbr`
: Do not support Ycbcr textures

`semaphore-export`
: Disable sync of exported dmabufs

`semaphore-import`
: Disable sync of imported dmabufs

`incremental-present`
: Do not send damage regions

`swapchain-maintenance`
: Do not use advanced swapchain features

The special value `all` can be used to turn on all values. The special
value `help` can be used to obtain a list of all supported values.

### `GDK_WAYLAND_DISABLE`

This variable can be set to a list of values, which cause the GDK Wayland
backend to not use certain Wayland interfaces, even if the compositor advertises
them.

The special value `help` can be used to obtain help.

To see a list of Wayland interface names, use `GDK_DEBUG=misc` and look for
`global` in the output.

### `GSK_RENDERER`

If set, selects the GSK renderer to use. The following renderers can
be selected, provided they are included in the GTK library you are
using and the GDK backend supports them:

`help`
: Prints information about available options

`broadway`
: Selects the Broadway-backend specific renderer

`cairo`
: Selects the fallback Cairo renderer

`opengl`
: Selects the OpenGL renderer

`gl`:
: Selects the OpenGL renderer.

`vulkan`
: Selects the Vulkan renderer


::: note
    If you are running the Nahimic 3 service on a Windows system with
    nVidia graphics, you need to perform one of the following:

     - stop the "Nahimic service"
     - insert the GTK application into the Nahimic blocklist, as noted in the
       [nVidia forums](https://www.nvidia.com/en-us/geforce/forums/game-ready-drivers/13/297952/nahimic-and-nvidia-drivers-conflict/2334568/)
     - use the cairo renderer (at the cost of being unable to use OpenGL features)
     - use `GDK_DEBUG=gl-gles`, if you know that GLES support is enabled for the build.

    This is a known issue, as the above link indicates, and affects quite
    a number of applications—sadly, since this issue lies within the
    nVidia graphics driver and/or the Nahimic 3 code, we are not able
    to rememdy this on the GTK side; the best bet before trying the above
    workarounds is to try to update your graphics drivers and Nahimic
    installation.


### `GSK_GPU_DISABLE`

This variable can be set to a list of values, which cause GSK to
disable certain optimizations of the "ngl" and "vulkan" renderer.

`clear`
: Use shaders instead of vkCmdClearAttachment()/glClear()

`merge`
: USe one vkCmdDraw()/glDrawArrays() per operation

`blit`
: Use shaders instead of vkCmdBlit()/glBlitFramebuffer()

`gradients`
: Don't supersample gradients

`mipmap`
: Avoid creating mipmaps

`to-image`
: Don't fast-path creation of images for nodes

`occlusion`
: Disable occlusion culling via opacity tracking

`repeat`
: Repeat drawing operations instead of using offscreen and GL_REPEAT

The special value `all` can be used to turn on all values. The special
value `help` can be used to obtain a list of all supported values.

### `GSK_CACHE_TIMEOUT`

Overrides the timeout for cache GC in the "ngl" and "vulkan" renderers.
The value can be -1 to disable GC entirely, 0 to force GC to happen
before every frame, or a positive number to do GC in a timeout every
n seconds. The default timeout is 15 seconds.

### `GTK_CSD`

The default value of this environment variable is `1`. If changed
to `0`, this disables the default use of client-side decorations
on GTK windows, thus making the window manager responsible for
drawing the decorations of windows that do not have a custom
titlebar widget.

CSD is always used for windows with a custom titlebar widget set,
as the WM should not draw another titlebar or other decorations
around the custom one.

### `GTK_A11Y`

If set, selects the accessibility backend to use. The following
backends can be selected, provided they are included in the GTK
library you are using:

`help`
: Prints information about available options

`atspi`
: Selects the AT-SPI accessibility backend

`accesskit`
: Selects the AccessKit accessibility backend

`test`
: Selects the test backend

`none`
: Disables the accessibility backend

The `test` accessibility backend is recommended for test suites and remote
continuous integration pipelines.

### `XDG_DATA_HOME`, `XDG_DATA_DIRS`

GTK uses these environment variables to locate icon themes
and MIME information. For more information, see the
[Icon Theme Specification](https://freedesktop.org/Standards/icon-theme-spec)
the [Shared MIME-Info Database](https://freedesktop.org/Standards/shared-mime-info-spec)
and the [Base Directory Specification](https://freedesktop.org/Standards/basedir-spec).

### `DESKTOP_STARTUP_ID`

GTK uses this environment variable to provide startup notification
according to the [Startup Notification Spec](https://standards.freedesktop.org/startup-notification-spec/startup-notification-latest.txt).
Following the specification, GTK unsets this variable after reading
it (to keep it from leaking to child processes). So, if you need its
value for your own purposes, you have to read it before calling
[func@Gtk.init].

## Interactive debugging

![The inspector](inspector.png)

GTK includes an interactive debugger, called the GTK Inspector, which
lets you explore the widget tree of any GTK application at runtime,
as well as tweak the theme and trigger visual debugging aids. You can
easily try out changes at runtime before putting them into the code.

Note that the GTK inspector can only show GTK internals. It can not
understand the application-specific logic of a GTK application. Also,
the fact that the GTK inspector is running in the application process
limits what it can do. It is meant as a complement to full-blown
debuggers and system tracing facilities such as DTrace, not as a
replacement.

To enable the GTK inspector, you can use the <kbd>Control</kbd>+<kbd>Shift</kbd>+<kbd>I</kbd> or
<kbd>Control</kbd>+<kbd>Shift</kbd>+<kbd>D</kbd> keyboard shortcuts, or
set the `GTK_DEBUG=interactive` environment variable.

After opening the inspector, it listens for a few keyboard shortcuts that
let you use its frame and event recording functionality without moving the
focus away from the application window: <kbd>Super</kbd>+<kbd>R</kbd> turns
the recording on and off, <kbd>Super</kbd>+<kbd>C</kbd> records a single
frame and <kbd>Super</kbd>+<kbd>F</kbd> records a single frame and saves
it to a `.node` file.

There are a few more environment variables that can be set to influence
how the inspector renders its UI. `GTK_INSPECTOR_DISPLAY` and
`GTK_INSPECTOR_RENDERER` determine the GDK display and the GSK
renderer that the inspector is using.

In some situations, it may be inappropriate to give users access to
the GTK inspector. The keyboard shortcuts can be disabled with the
`enable-inspector-keybinding` key in the `org.gtk.Settings.Debug`
GSettings schema.


## Profiling

GTK supports profiling with sysprof. It exports timing information
about frameclock phases and various characteristics of GskRenderers
in a format that can be displayed by sysprof or GNOME Builder.

A simple way to capture data is to run your application under the
`sysprof-cli` wrapper, which will write profiling data to a file
called `capture.syscap`.

When launching the application from sysprof, it will set the
`SYSPROF_TRACE_FD` environment variable to point GTK at a file
descriptor to write profiling data to.
