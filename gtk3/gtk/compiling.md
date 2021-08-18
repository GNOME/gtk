Title: Compiling GTK Applications

## Compiling GTK Applications on UNIX

To compile a GTK application, you need to tell the compiler where to find
the GTK header files and libraries. This is done with the `pkg-config`
utility.

The following interactive shell session demonstrates how pkg-config is used
(the actual output on your system may be different):

```
$ pkg-config --cflags gtk+-3.0
 -pthread -I/usr/include/gtk-3.0 -I/usr/lib64/gtk-3.0/include -I/usr/include/atk-1.0 -I/usr/include/cairo -I/usr/include/pango-1.0 -I/usr/include/glib-2.0 -I/usr/lib64/glib-2.0/include -I/usr/include/pixman-1 -I/usr/include/freetype2 -I/usr/include/libpng12
$ pkg-config --libs gtk+-3.0
 -pthread -lgtk-3 -lgdk-3 -latk-1.0 -lgio-2.0 -lpangoft2-1.0 -lgdk_pixbuf-2.0 -lpangocairo-1.0 -lcairo -lpango-1.0 -lfreetype -lfontconfig -lgobject-2.0 -lgmodule-2.0 -lgthread-2.0 -lrt -lglib-2.0
```

The simplest way to compile a program is to use the "backticks" feature of
the shell. If you enclose a command in backticks (not single quotes), then
its output will be substituted into the command line before execution. So to
compile a GTK Hello, World, you would type the following:

    $ cc `pkg-config --cflags gtk+-3.0` hello.c -o hello `pkg-config --libs gtk+-3.0`

Deprecated GTK functions are annotated to make the compiler emit warnings
when they are used (e.g. with gcc, you need to use the
`-Wdeprecated-declarations` option). If these warnings are problematic, they
can be turned off by defining the preprocessor symbol
`GDK_DISABLE_DEPRECATION_WARNINGS` by using the commandline option
`-DGDK_DISABLE_DEPRECATION_WARNINGS`

GTK deprecation annotations are versioned; by defining the macros
`GDK_VERSION_MIN_REQUIRED` and `GDK_VERSION_MAX_ALLOWED`, you can specify the
range of GTK versions whose API you want to use. APIs that were deprecated
before or introduced after this range will trigger compiler warnings.

Here is how you would compile `hello.c` if you want to allow it to use
symbols that were not deprecated in 3.2:

```
$ cc `pkg-config --cflags gtk+-3.0` -DGDK_VERSION_MIN_REQIRED=GDK_VERSION_3_2 hello.c -o hello `pkg-config --libs gtk+-3.0`
```

And here is how you would compile `hello.c` if you don't want it to use any
symbols that were introduced after 3.4:

```
$ cc `pkg-config --cflags gtk+-3.0` -DGDK_VERSION_MAX_ALLOWED=GDK_VERSION_3_4 hello.c -o hello `pkg-config --libs gtk+-3.0`
```

The older deprecation mechanism of hiding deprecated interfaces entirely
from the compiler by using the preprocessor symbol `GTK_DISABLE_DEPRECATED` is
still used for deprecated macros, enumeration values, etc. To detect uses of
these in your code, use the commandline option `-DGTK_DISABLE_DEPRECATED`.
There are similar symbols `GDK_DISABLE_DEPRECATED`,
`GDK_PIXBUF_DISABLE_DEPRECATED` and `G_DISABLE_DEPRECATED` for GDK, GdkPixbuf
and GLib, respectively.

Similarly, if you want to make sure that your program doesn't use any
functions which may be problematic in a multidevice setting, you can define
the preprocessor symbol `GDK_MULTIDEVICE_SAFE` by using the command line
option `-DGTK_MULTIDEVICE_SAFE=1`.

### Useful autotools macros

GTK provides various macros for easily checking version and backends
supported. The macros are

`AM_PATH_GTK_3_0([minimum-version], [if-found], [if-not-found], [modules])`
: This macro should be used to check that GTK is installed and available for
  compilation. The four arguments are optional, and they are: `minimum-version`,
  the minimum version of GTK required for compilation; `if-found`, the action
  to perform if a valid version of GTK has been found; `if-not-found`, the
  action to perform if a valid version of GTK has not been found; `modules`,
  a list of modules to be checked along with GTK.

`GTK_CHECK_BACKEND([backend-name], [minimum-version], [if-found], [if-not-found])`
: This macro should be used to check if a specific backend is supported by GTK.
  The `minimum-version`, `if-found` and `if-not-found` arguments are optional.
