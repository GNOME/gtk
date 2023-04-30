Title: Running GLib Applications

# Running GLib Applications

## Environment variables

The runtime behaviour of GLib applications can be influenced by a number of
environment variables.

Standard variables
:  GLib reads standard environment variables like `LANG`, `PATH`, `HOME`,
   `TMPDIR`, `TZ` and `LOGNAME`.

XDG directories
:  GLib consults the environment variables `XDG_DATA_HOME`,
   `XDG_DATA_DIRS`, `XDG_CONFIG_HOME`, `XDG_CONFIG_DIRS`, `XDG_CACHE_HOME` and
   `XDG_RUNTIME_DIR` for the various XDG directories. For more information, see
   the [XDG basedir specification](https://specifications.freedesktop.org/basedir-spec/latest/).

`G_FILENAME_ENCODING`
:  This environment variable can be set to a comma-separated list of character
   set names. GLib assumes that filenames are encoded in the first character
   set from that list rather than in UTF-8. The special token "@locale" can be
   used to specify the character set for the current locale.

`G_BROKEN_FILENAMES`
:  If this environment variable is set, GLib assumes that filenames are in the
   locale encoding rather than in UTF-8. `G_FILENAME_ENCODING` takes priority
   over `G_BROKEN_FILENAMES`.

`G_MESSAGES_PREFIXED`
:  A list of log levels for which messages should be prefixed by the program
   name and PID of the application. The default is to prefix everything except
   `G_LOG_LEVEL_MESSAGE` and `G_LOG_LEVEL_INFO`. The possible values are error,
   warning, critical, message, info and debug. You can also use the special
   values all and help. This environment variable only affects the default log
   handler, `g_log_default_handler()`.

`G_MESSAGES_DEBUG`
:  A space-separated list of log domains for which informational and debug
   messages should be printed. By default, these messages are not printed. You
   can also use the special value all. This environment variable only affects
   the default log handler, `g_log_default_handler()`.

`G_DEBUG`
:  This environment variable can be set to a list of debug options, which cause
   GLib to print out different types of debugging information.

   - `fatal-warnings`: Causes GLib to abort the program at the first call to
     `g_warning()` or `g_critical()`. Use of this flag is not recommended
     except when debugging.
   - `fatal-criticals`: Causes GLib to abort the program at the first call
     to `g_critical()`. This flag can be useful during debugging and
     testing.
   - `gc-friendly`: Newly allocated memory that isn't directly initialized,
     as well as memory being freed will be reset to 0. The point here is to
     allow memory checkers and similar programs that use Boehm GC alike
     algorithms to produce more accurate results.
   - `resident-modules`: All modules loaded by GModule will be made
     resident. This can be useful for tracking memory leaks in modules which
     are later unloaded; but it can also hide bugs where code is accessed
     after the module would have normally been unloaded.
   - `bind-now-modules`: All modules loaded by GModule will bind their
     symbols at load time, even when the code uses `G_MODULE_BIND_LAZY`.

   The special value `all` can be used to turn on all debug options. The special
   value `help` can be used to print all available options.

`G_SLICE`
:  This environment variable allows reconfiguration of the GSlice memory
   allocator.

   - `always-malloc`: This will cause all slices allocated through
     `g_slice_alloc()` and released by `g_slice_free1()` to be actually
     allocated via direct calls to `g_malloc()` and `g_free()`. This is most
     useful for memory checkers and similar programs that use Boehm GC alike
     algorithms to produce more accurate results. It can also be in
     conjunction with debugging features of the system's `malloc()`
     implementation such as glibc's `MALLOC_CHECK_=2` to debug erroneous
     slice allocation code, although debug-blocks is usually a better suited
     debugging tool.
   - `debug-blocks`: Using this option (present since GLib 2.13) engages
     extra code which performs sanity checks on the released memory slices.
     Invalid slice addresses or slice sizes will be reported and lead to a
     program halt. This option is for debugging scenarios. In particular,
     client packages sporting their own test suite should always enable this
     option when running tests. Global slice validation is ensured by
     storing size and address information for each allocated chunk, and
     maintaining a global hash table of that data. That way, multi-thread
     scalability is given up, and memory consumption is increased. However,
     the resulting code usually performs acceptably well, possibly better
     than with comparable memory checking carried out using external tools.

   An example of a memory corruption scenario that cannot be reproduced with
   `G_SLICE=always-malloc`, but will be caught by G_SLICE=debug-blocks is as
   follows:

     void *slist = g_slist_alloc (); /* void* gives up type-safety */
     g_list_free (slist);            /* corruption: sizeof (GSList) != sizeof (GList) */


   The special value `all` can be used to turn on all options. The special
   value `help` can be used to print all available options.

`G_RANDOM_VERSION`
:  If this environment variable is set to '2.0', the outdated pseudo-random
   number seeding and generation algorithms from GLib 2.0 are used instead of
   the newer, better ones. You should only set this variable if you have
   sequences of numbers that were generated with Glib 2.0 that you need to
   reproduce exactly.

`LIBCHARSET_ALIAS_DIR`
:  Allows to specify a nonstandard location for the `charset.aliases` file
   that is used by the character set conversion routines. The default
   location is the `libdir` specified at compilation time.

`TZDIR`
:  Allows to specify a nonstandard location for the timezone data files that
   are used by the `GDateTime` API. The default location is under
   `/usr/share/zoneinfo`. For more information, also look at the `tzset` manual
   page.

`G_ENABLE_DIAGNOSTIC`
:  If set to a non-zero value, this environment variable enables diagnostic
   messages, like deprecation messages for GObject properties and signals.

`G_DEBUGGER`
:  When running on Windows, if set to a non-empty string, GLib will try to
   interpret the contents of this environment variable as a command line to a
   debugger, and run it if the process crashes. The debugger command line
   should contain `%p` and `%e` substitution tokens, which GLib will replace
   with the process ID of the crashing process and a handle to an event that
   the debugger should signal to let GLib know that the debugger successfully
   attached to the process. If `%e` is absent, or if the debugger is not able
   to signal events, GLib will resume execution after 60 seconds. If `%p` is
   absent, the debugger won't know which process to attach to, and GLib will
   also resume execution after 60 seconds. Additionally, even if `G_DEBUGGER`
   is not set, GLib would still try to print basic exception information (code
   and address) into `stderr`. By default the debugger gets a new console
   allocated for it. Set the `G_DEBUGGER_OLD_CONSOLE` environment variable to
   any non-empty string to make the debugger inherit the console of the
   crashing process. Normally this is only used by the GLib testsuite. The
   exception handler is written with the aim of making it as simple as
   possible, to minimize the risk of it invoking buggy functions or running
   buggy code, which would result in exceptions being raised recursively.
   Because of that it lacks most of the amenities that one would expect of
   GLib. Namely, it does not support Unicode, so it is highly advisable to
   only use ASCII characters in `G_DEBUGGER`. See also `G_VEH_CATCH`.

`G_VEH_CATCH`
:  Catching some exceptions can break the program, since Windows will
   sometimes use exceptions for execution flow control and other purposes
   other than signalling a crash. The `G_VEH_CATCH` environment variable
   augments Vectored Exception Handling on Windows (see `G_DEBUGGER`),
   allowing GLib to catch more exceptions. Set this variable to a
   comma-separated list of hexadecimal exception codes that should
   additionally be caught. By default GLib will only catch Access Violation,
   Stack Overflow and Illegal Instruction exceptions.

## Locale

A number of interfaces in GLib depend on the current locale in which an
application is running. Therefore, most GLib-using applications should call
`setlocale (LC_ALL, "")` to set up the current locale.

On Windows, in a C program there are several locale concepts that not
necessarily are synchronized. On one hand, there is the system default ANSI
code-page, which determines what encoding is used for file names handled by
the C library's functions and the Win32 API. (We are talking about the
"narrow" functions here that take character pointers, not the "wide" ones.)

On the other hand, there is the C library's current locale. The character
set (code-page) used by that is not necessarily the same as the system
default ANSI code-page. Strings in this character set are returned by
functions like `strftime()`.

## Debugging with GDB

GLib ships with a set of Python macros for the GDB debugger. These includes
pretty printers for lists, hashtables and GObject types. It also has a
backtrace filter that makes backtraces with signal emissions easier to read.

To use this you need a version of GDB that supports Python scripting;
anything from 7.0 should be fine. You then need to install GLib in the same
prefix as GDB so that the Python GDB autoloaded files get installed in the
right place for GDB to pick up.

General pretty printing should just happen without having to do anything
special. To get the signal emission filtered backtrace you must use the
"new-backtrace" command instead of the standard one.

There is also a new command called gforeach that can be used to apply a
command on each item in a list. E.g. you can do

`gforeach i in some_list_variable: print *(GtkWidget *)l`

Which would print the contents of each widget in a list of widgets.

## SystemTap

SystemTap is a dynamic whole-system analysis toolkit. GLib ships with a file
`libglib-2.0.so.*.stp` which defines a set of probe points, which you can hook
into with custom SystemTap scripts. See the files `libglib-2.0.so.*.stp`,
`libgobject-2.0.so.*.stp` and `libgio-2.0.so.*.stp` which are in your shared
SystemTap scripts directory.

## Memory statistics

`g_mem_profile()` will output a summary `g_malloc()` memory usage, if memory
profiling has been enabled by calling:

```
g_mem_set_vtable (glib_mem_profiler_table);
```

upon startup.

If GLib has been configured with full debugging support, then
`g_slice_debug_tree_statistics()` can be called in a debugger to output details
about the memory usage of the slice allocator.
