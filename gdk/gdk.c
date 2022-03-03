/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include "config.h"

#include "gdkversionmacros.h"
#include "gdkmain.h"

#include "gdkprofilerprivate.h"
#include "gdkinternals.h"
#include "gdkintl.h"

#include "gdkresources.h"

#include "gdk-private.h"

#ifndef HAVE_XCONVERTCASE
#include "gdkkeysyms.h"
#endif

#include "gdkconstructor.h"

#include <string.h>
#include <stdlib.h>

#include <fribidi.h>


/**
 * SECTION:general
 * @Short_description: Library initialization and miscellaneous functions
 * @Title: General
 *
 * This section describes the GDK initialization functions and miscellaneous
 * utility functions, as well as deprecation facilities.
 *
 * The GDK and GTK+ headers annotate deprecated APIs in a way that produces
 * compiler warnings if these deprecated APIs are used. The warnings
 * can be turned off by defining the macro %GDK_DISABLE_DEPRECATION_WARNINGS
 * before including the glib.h header.
 *
 * GDK and GTK+ also provide support for building applications against
 * defined subsets of deprecated or new APIs. Define the macro
 * %GDK_VERSION_MIN_REQUIRED to specify up to what version
 * you want to receive warnings about deprecated APIs. Define the
 * macro %GDK_VERSION_MAX_ALLOWED to specify the newest version
 * whose API you want to use.
 */

/**
 * GDK_WINDOWING_X11:
 *
 * The #GDK_WINDOWING_X11 macro is defined if the X11 backend
 * is supported.
 *
 * Use this macro to guard code that is specific to the X11 backend.
 */

/**
 * GDK_WINDOWING_WIN32:
 *
 * The #GDK_WINDOWING_WIN32 macro is defined if the Win32 backend
 * is supported.
 *
 * Use this macro to guard code that is specific to the Win32 backend.
 */

/**
 * GDK_WINDOWING_QUARTZ:
 *
 * The #GDK_WINDOWING_QUARTZ macro is defined if the Quartz backend
 * is supported.
 *
 * Use this macro to guard code that is specific to the Quartz backend.
 */

/**
 * GDK_WINDOWING_WAYLAND:
 *
 * The #GDK_WINDOWING_WAYLAND macro is defined if the Wayland backend
 * is supported.
 *
 * Use this macro to guard code that is specific to the Wayland backend.
 */

typedef struct _GdkPredicate  GdkPredicate;

struct _GdkPredicate
{
  GdkEventFunc func;
  gpointer data;
};

typedef struct _GdkThreadsDispatch GdkThreadsDispatch;

struct _GdkThreadsDispatch
{
  GSourceFunc func;
  gpointer data;
  GDestroyNotify destroy;
};


/* Private variable declarations
 */
static int gdk_initialized = 0;                     /* 1 if the library is initialized,
                                                     * 0 otherwise.
                                                     */

static gchar  *gdk_progclass = NULL;
static gboolean gdk_progclass_overridden;

static GMutex gdk_threads_mutex;

static GCallback gdk_threads_lock = NULL;
static GCallback gdk_threads_unlock = NULL;

static const GDebugKey gdk_gl_keys[] = {
  { "disable",               GDK_GL_DISABLE },
  { "always",                GDK_GL_ALWAYS },
  { "software-draw",         GDK_GL_SOFTWARE_DRAW_GL | GDK_GL_SOFTWARE_DRAW_SURFACE} ,
  { "software-draw-gl",      GDK_GL_SOFTWARE_DRAW_GL },
  { "software-draw-surface", GDK_GL_SOFTWARE_DRAW_SURFACE },
  { "texture-rectangle",     GDK_GL_TEXTURE_RECTANGLE },
  { "legacy",                GDK_GL_LEGACY },
  { "gles",                  GDK_GL_GLES },
};

#ifdef G_ENABLE_DEBUG
static const GDebugKey gdk_debug_keys[] = {
  { "events",        GDK_DEBUG_EVENTS },
  { "misc",          GDK_DEBUG_MISC },
  { "dnd",           GDK_DEBUG_DND },
  { "xim",           GDK_DEBUG_XIM },
  { "nograbs",       GDK_DEBUG_NOGRABS },
  { "input",         GDK_DEBUG_INPUT },
  { "cursor",        GDK_DEBUG_CURSOR },
  { "multihead",     GDK_DEBUG_MULTIHEAD },
  { "xinerama",      GDK_DEBUG_XINERAMA },
  { "draw",          GDK_DEBUG_DRAW },
  { "eventloop",     GDK_DEBUG_EVENTLOOP },
  { "frames",        GDK_DEBUG_FRAMES },
  { "settings",      GDK_DEBUG_SETTINGS },
  { "opengl",        GDK_DEBUG_OPENGL }
};

static gboolean
gdk_arg_debug_cb (const char *key, const char *value, gpointer user_data, GError **error)
{
  guint debug_value = g_parse_debug_string (value,
                                            (GDebugKey *) gdk_debug_keys,
                                            G_N_ELEMENTS (gdk_debug_keys));

  if (debug_value == 0 && value != NULL && strcmp (value, "") != 0)
    {
      g_set_error (error,
                   G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                   _("Error parsing option --gdk-debug"));
      return FALSE;
    }

  _gdk_debug_flags |= debug_value;

  return TRUE;
}

static gboolean
gdk_arg_no_debug_cb (const char *key, const char *value, gpointer user_data, GError **error)
{
  guint debug_value = g_parse_debug_string (value,
                                            (GDebugKey *) gdk_debug_keys,
                                            G_N_ELEMENTS (gdk_debug_keys));

  if (debug_value == 0 && value != NULL && strcmp (value, "") != 0)
    {
      g_set_error (error,
                   G_OPTION_ERROR, G_OPTION_ERROR_FAILED,
                   _("Error parsing option --gdk-no-debug"));
      return FALSE;
    }

  _gdk_debug_flags &= ~debug_value;

  return TRUE;
}
#endif /* G_ENABLE_DEBUG */

static gboolean
gdk_arg_class_cb (const char *key, const char *value, gpointer user_data, GError **error)
{
  gdk_set_program_class (value);
  gdk_progclass_overridden = TRUE;

  return TRUE;
}

static gboolean
gdk_arg_name_cb (const char *key, const char *value, gpointer user_data, GError **error)
{
  g_set_prgname (value);

  return TRUE;
}

static const GOptionEntry gdk_args[] = {
  { "class",        0, 0,                     G_OPTION_ARG_CALLBACK, gdk_arg_class_cb,
    /* Description of --class=CLASS in --help output */        N_("Program class as used by the window manager"),
    /* Placeholder in --class=CLASS in --help output */        N_("CLASS") },
  { "name",         0, 0,                     G_OPTION_ARG_CALLBACK, gdk_arg_name_cb,
    /* Description of --name=NAME in --help output */          N_("Program name as used by the window manager"),
    /* Placeholder in --name=NAME in --help output */          N_("NAME") },
#ifndef G_OS_WIN32
  { "display",      0, G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,   &_gdk_display_name,
    /* Description of --display=DISPLAY in --help output */    N_("X display to use"),
    /* Placeholder in --display=DISPLAY in --help output */    N_("DISPLAY") },
#endif
#ifdef G_ENABLE_DEBUG
  { "gdk-debug",    0, 0, G_OPTION_ARG_CALLBACK, gdk_arg_debug_cb,  
    /* Description of --gdk-debug=FLAGS in --help output */    N_("GDK debugging flags to set"),
    /* Placeholder in --gdk-debug=FLAGS in --help output */    N_("FLAGS") },
  { "gdk-no-debug", 0, 0, G_OPTION_ARG_CALLBACK, gdk_arg_no_debug_cb, 
    /* Description of --gdk-no-debug=FLAGS in --help output */ N_("GDK debugging flags to unset"),
    /* Placeholder in --gdk-no-debug=FLAGS in --help output */ N_("FLAGS") },
#endif 
  { NULL }
};

void
gdk_add_option_entries (GOptionGroup *group)
{
  g_option_group_add_entries (group, gdk_args);
}

/**
 * gdk_add_option_entries_libgtk_only:
 * @group: An option group.
 *
 * Appends gdk option entries to the passed in option group. This is
 * not public API and must not be used by applications.
 *
 * Deprecated: 3.16: This symbol was never meant to be used outside
 *   of GTK+
 */
void
gdk_add_option_entries_libgtk_only (GOptionGroup *group)
{
  gdk_add_option_entries (group);
}

static gpointer
register_resources (gpointer dummy G_GNUC_UNUSED)
{
  _gdk_register_resource ();

  return NULL;
}

static void
gdk_ensure_resources (void)
{
  static GOnce register_resources_once = G_ONCE_INIT;

  g_once (&register_resources_once, register_resources, NULL);
}

void
gdk_pre_parse (void)
{
  const char *rendering_mode;
  const gchar *gl_string;

  gdk_initialized = TRUE;

  gdk_ensure_resources ();

  /* We set the fallback program class here, rather than lazily in
   * gdk_get_program_class, since we don't want -name to override it.
   */
  gdk_progclass = g_strdup (g_get_prgname ());
  if (gdk_progclass && gdk_progclass[0])
    gdk_progclass[0] = g_ascii_toupper (gdk_progclass[0]);
  
#ifdef G_ENABLE_DEBUG
  {
    gchar *debug_string = getenv("GDK_DEBUG");
    if (debug_string != NULL)
      _gdk_debug_flags = g_parse_debug_string (debug_string,
                                              (GDebugKey *) gdk_debug_keys,
                                              G_N_ELEMENTS (gdk_debug_keys));
    if (g_getenv ("GTK_TRACE_FD"))
      gdk_profiler_start (atoi (g_getenv ("GTK_TRACE_FD")));
    else if (g_getenv ("GTK_TRACE"))
      gdk_profiler_start (-1);
  }
#endif  /* G_ENABLE_DEBUG */

  gl_string = getenv("GDK_GL");
  if (gl_string != NULL)
    _gdk_gl_flags = g_parse_debug_string (gl_string,
                                          (GDebugKey *) gdk_gl_keys,
                                          G_N_ELEMENTS (gdk_gl_keys));

  if (getenv ("GDK_NATIVE_WINDOWS"))
    {
      g_warning ("The GDK_NATIVE_WINDOWS environment variable is not supported in GTK3.\n"
                 "See the documentation for gdk_window_ensure_native() on how to get native windows.");
      g_unsetenv ("GDK_NATIVE_WINDOWS");
    }

  rendering_mode = g_getenv ("GDK_RENDERING");
  if (rendering_mode)
    {
      if (g_str_equal (rendering_mode, "similar"))
        _gdk_rendering_mode = GDK_RENDERING_MODE_SIMILAR;
      else if (g_str_equal (rendering_mode, "image"))
        _gdk_rendering_mode = GDK_RENDERING_MODE_IMAGE;
      else if (g_str_equal (rendering_mode, "recording"))
        _gdk_rendering_mode = GDK_RENDERING_MODE_RECORDING;
    }
}

/**
 * gdk_pre_parse_libgtk_only:
 *
 * Prepare for parsing command line arguments for GDK. This is not
 * public API and should not be used in application code.
 *
 * Deprecated: 3.16: This symbol was never meant to be used outside
 *   of GTK+
 */
void
gdk_pre_parse_libgtk_only (void)
{
  gdk_pre_parse ();
}
  
/**
 * gdk_parse_args:
 * @argc: the number of command line arguments.
 * @argv: (inout) (array length=argc): the array of command line arguments.
 * 
 * Parse command line arguments, and store for future
 * use by calls to gdk_display_open().
 *
 * Any arguments used by GDK are removed from the array and @argc and @argv are
 * updated accordingly.
 *
 * You shouldn’t call this function explicitly if you are using
 * gtk_init(), gtk_init_check(), gdk_init(), or gdk_init_check().
 *
 * Since: 2.2
 **/
void
gdk_parse_args (int    *argc,
                char ***argv)
{
  GOptionContext *option_context;
  GOptionGroup *option_group;
  GError *error = NULL;

  if (gdk_initialized)
    return;

  gdk_pre_parse ();

  option_context = g_option_context_new (NULL);
  g_option_context_set_ignore_unknown_options (option_context, TRUE);
  g_option_context_set_help_enabled (option_context, FALSE);
  option_group = g_option_group_new (NULL, NULL, NULL, NULL, NULL);
  g_option_context_set_main_group (option_context, option_group);

  g_option_group_add_entries (option_group, gdk_args);

  if (!g_option_context_parse (option_context, argc, argv, &error))
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }
  g_option_context_free (option_context);

  GDK_NOTE (MISC, g_message ("progname: \"%s\"", g_get_prgname ()));
}

/**
 * gdk_get_display:
 *
 * Gets the name of the display, which usually comes from the
 * `DISPLAY` environment variable or the
 * `--display` command line option.
 *
 * Returns: the name of the display.
 *
 * Deprecated: 3.8: Call gdk_display_get_name (gdk_display_get_default ()))
 *    instead.
 */
gchar *
gdk_get_display (void)
{
  return g_strdup (gdk_display_get_name (gdk_display_get_default ()));
}

/**
 * gdk_get_display_arg_name:
 *
 * Gets the display name specified in the command line arguments passed
 * to gdk_init() or gdk_parse_args(), if any.
 *
 * Returns: (nullable): the display name, if specified explicitly,
 *   otherwise %NULL this string is owned by GTK+ and must not be
 *   modified or freed.
 *
 * Since: 2.2
 */
const gchar *
gdk_get_display_arg_name (void)
{
  if (!_gdk_display_arg_name)
    _gdk_display_arg_name = g_strdup (_gdk_display_name);

   return _gdk_display_arg_name;
}

/*< private >
 * gdk_display_open_default:
 *
 * Opens the default display specified by command line arguments or
 * environment variables, sets it as the default display, and returns
 * it. gdk_parse_args() must have been called first. If the default
 * display has previously been set, simply returns that. An internal
 * function that should not be used by applications.
 *
 * Returns: (nullable) (transfer none): the default display, if it
 *   could be opened, otherwise %NULL.
 */
GdkDisplay *
gdk_display_open_default (void)
{
  GdkDisplay *display;

  g_return_val_if_fail (gdk_initialized, NULL);

  display = gdk_display_get_default ();
  if (display)
    return display;

  display = gdk_display_open (gdk_get_display_arg_name ());

  return display;
}

gboolean
gdk_running_in_sandbox (void)
{
  return g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS);
}

gboolean
gdk_should_use_portal (void)
{
  static const char *use_portal = NULL;

  if (G_UNLIKELY (use_portal == NULL))
    {
      if (gdk_running_in_sandbox ())
        use_portal = "1";
      else
        {
          use_portal = g_getenv ("GTK_USE_PORTAL");
          if (!use_portal)
            use_portal = "";
        }
    }

  return use_portal[0] == '1';
}

/**
 * gdk_display_open_default_libgtk_only:
 *
 * Opens the default display specified by command line arguments or
 * environment variables, sets it as the default display, and returns
 * it. gdk_parse_args() must have been called first. If the default
 * display has previously been set, simply returns that. An internal
 * function that should not be used by applications.
 *
 * Returns: (nullable) (transfer none): the default display, if it
 *   could be opened, otherwise %NULL.
 *
 * Deprecated: 3.16: This symbol was never meant to be used outside
 *   of GTK+
 */
GdkDisplay *
gdk_display_open_default_libgtk_only (void)
{
  return gdk_display_open_default ();
}

/**
 * gdk_init_check:
 * @argc: (inout): the number of command line arguments.
 * @argv: (array length=argc) (inout): the array of command line arguments.
 *
 * Initializes the GDK library and connects to the windowing system,
 * returning %TRUE on success.
 *
 * Any arguments used by GDK are removed from the array and @argc and @argv
 * are updated accordingly.
 *
 * GTK+ initializes GDK in gtk_init() and so this function is not usually
 * needed by GTK+ applications.
 *
 * Returns: %TRUE if initialization succeeded.
 */
gboolean
gdk_init_check (int    *argc,
                char ***argv)
{
  gdk_parse_args (argc, argv);

  return gdk_display_open_default () != NULL;
}


/**
 * gdk_init:
 * @argc: (inout): the number of command line arguments.
 * @argv: (array length=argc) (inout): the array of command line arguments.
 *
 * Initializes the GDK library and connects to the windowing system.
 * If initialization fails, a warning message is output and the application
 * terminates with a call to `exit(1)`.
 *
 * Any arguments used by GDK are removed from the array and @argc and @argv
 * are updated accordingly.
 *
 * GTK+ initializes GDK in gtk_init() and so this function is not usually
 * needed by GTK+ applications.
 */
void
gdk_init (int *argc, char ***argv)
{
  if (!gdk_init_check (argc, argv))
    {
      const char *display_name = gdk_get_display_arg_name ();
      g_warning ("cannot open display: %s", display_name ? display_name : "");
      exit(1);
    }
}



/**
 * SECTION:threads
 * @Short_description: Functions for using GDK in multi-threaded programs
 * @Title: Threads
 *
 * For thread safety, GDK relies on the thread primitives in GLib,
 * and on the thread-safe GLib main loop.
 *
 * GLib is completely thread safe (all global data is automatically
 * locked), but individual data structure instances are not automatically
 * locked for performance reasons. So e.g. you must coordinate
 * accesses to the same #GHashTable from multiple threads.
 *
 * GTK+, however, is not thread safe. You should only use GTK+ and GDK
 * from the thread gtk_init() and gtk_main() were called on.
 * This is usually referred to as the “main thread”.
 *
 * Signals on GTK+ and GDK types, as well as non-signal callbacks, are
 * emitted in the main thread.
 *
 * You can schedule work in the main thread safely from other threads
 * by using gdk_threads_add_idle() and gdk_threads_add_timeout():
 *
 * |[<!-- language="C" -->
 * static void
 * worker_thread (void)
 * {
 *   ExpensiveData *expensive_data = do_expensive_computation ();
 *
 *   gdk_threads_add_idle (got_value, expensive_data);
 * }
 *
 * static gboolean
 * got_value (gpointer user_data)
 * {
 *   ExpensiveData *expensive_data = user_data;
 *
 *   my_app->expensive_data = expensive_data;
 *   gtk_button_set_sensitive (my_app->button, TRUE);
 *   gtk_button_set_label (my_app->button, expensive_data->result_label);
 *
 *   return G_SOURCE_REMOVE;
 * }
 * ]|
 *
 * You should use gdk_threads_add_idle() and gdk_threads_add_timeout()
 * instead of g_idle_add() and g_timeout_add() since libraries not under
 * your control might be using the deprecated GDK locking mechanism.
 * If you are sure that none of the code in your application and libraries
 * use the deprecated gdk_threads_enter() or gdk_threads_leave() methods,
 * then you can safely use g_idle_add() and g_timeout_add().
 *
 * For more information on this "worker thread" pattern, you should
 * also look at #GTask, which gives you high-level tools to perform
 * expensive tasks from worker threads, and will handle thread
 * management for you.
 */

/**
 * gdk_threads_enter:
 *
 * This function marks the beginning of a critical section in which
 * GDK and GTK+ functions can be called safely and without causing race
 * conditions. Only one thread at a time can be in such a critial
 * section.
 *
 * Deprecated:3.6: All GDK and GTK+ calls should be made from the main
 *     thread
 */
void
gdk_threads_enter (void)
{
  if (gdk_threads_lock)
    (*gdk_threads_lock) ();
}

/**
 * gdk_threads_leave:
 *
 * Leaves a critical region begun with gdk_threads_enter().
 *
 * Deprecated:3.6: All GDK and GTK+ calls should be made from the main
 *     thread
 */
void
gdk_threads_leave (void)
{
  if (gdk_threads_unlock)
    (*gdk_threads_unlock) ();
}

static void
gdk_threads_impl_lock (void)
{
  g_mutex_lock (&gdk_threads_mutex);
}

static void
gdk_threads_impl_unlock (void)
{
  /* we need a trylock() here because trying to unlock a mutex
   * that hasn't been locked yet is:
   *
   *  a) not portable
   *  b) fail on GLib ≥ 2.41
   *
   * trylock() will either succeed because nothing is holding the
   * GDK mutex, and will be unlocked right afterwards; or it's
   * going to fail because the mutex is locked already, in which
   * case we unlock it as expected.
   *
   * this is needed in the case somebody called gdk_threads_init()
   * without calling gdk_threads_enter() before calling gtk_main().
   * in theory, we could just say that this is undefined behaviour,
   * but our documentation has always been *less* than explicit as
   * to what the behaviour should actually be.
   *
   * see bug: https://bugzilla.gnome.org/show_bug.cgi?id=735428
   */
  g_mutex_trylock (&gdk_threads_mutex);
  g_mutex_unlock (&gdk_threads_mutex);
}

/**
 * gdk_threads_init:
 *
 * Initializes GDK so that it can be used from multiple threads
 * in conjunction with gdk_threads_enter() and gdk_threads_leave().
 *
 * This call must be made before any use of the main loop from
 * GTK+; to be safe, call it before gtk_init().
 *
 * Deprecated:3.6: All GDK and GTK+ calls should be made from the main
 *     thread
 */
void
gdk_threads_init (void)
{
  if (!gdk_threads_lock)
    gdk_threads_lock = gdk_threads_impl_lock;
  if (!gdk_threads_unlock)
    gdk_threads_unlock = gdk_threads_impl_unlock;
}

/**
 * gdk_threads_set_lock_functions: (skip)
 * @enter_fn:   function called to guard GDK
 * @leave_fn: function called to release the guard
 *
 * Allows the application to replace the standard method that
 * GDK uses to protect its data structures. Normally, GDK
 * creates a single #GMutex that is locked by gdk_threads_enter(),
 * and released by gdk_threads_leave(); using this function an
 * application provides, instead, a function @enter_fn that is
 * called by gdk_threads_enter() and a function @leave_fn that is
 * called by gdk_threads_leave().
 *
 * The functions must provide at least same locking functionality
 * as the default implementation, but can also do extra application
 * specific processing.
 *
 * As an example, consider an application that has its own recursive
 * lock that when held, holds the GTK+ lock as well. When GTK+ unlocks
 * the GTK+ lock when entering a recursive main loop, the application
 * must temporarily release its lock as well.
 *
 * Most threaded GTK+ apps won’t need to use this method.
 *
 * This method must be called before gdk_threads_init(), and cannot
 * be called multiple times.
 *
 * Deprecated:3.6: All GDK and GTK+ calls should be made from the main
 *     thread
 *
 * Since: 2.4
 **/
void
gdk_threads_set_lock_functions (GCallback enter_fn,
                                GCallback leave_fn)
{
  g_return_if_fail (gdk_threads_lock == NULL &&
                    gdk_threads_unlock == NULL);

  gdk_threads_lock = enter_fn;
  gdk_threads_unlock = leave_fn;
}

static gboolean
gdk_threads_dispatch (gpointer data)
{
  GdkThreadsDispatch *dispatch = data;
  gboolean ret = FALSE;

  gdk_threads_enter ();

  if (!g_source_is_destroyed (g_main_current_source ()))
    ret = dispatch->func (dispatch->data);

  gdk_threads_leave ();

  return ret;
}

static void
gdk_threads_dispatch_free (gpointer data)
{
  GdkThreadsDispatch *dispatch = data;

  if (dispatch->destroy && dispatch->data)
    dispatch->destroy (dispatch->data);

  g_slice_free (GdkThreadsDispatch, data);
}


/**
 * gdk_threads_add_idle_full: (rename-to gdk_threads_add_idle)
 * @priority: the priority of the idle source. Typically this will be in the
 *            range between #G_PRIORITY_DEFAULT_IDLE and #G_PRIORITY_HIGH_IDLE
 * @function: function to call
 * @data:     data to pass to @function
 * @notify: (allow-none):   function to call when the idle is removed, or %NULL
 *
 * Adds a function to be called whenever there are no higher priority
 * events pending.  If the function returns %FALSE it is automatically
 * removed from the list of event sources and will not be called again.
 *
 * This variant of g_idle_add_full() calls @function with the GDK lock
 * held. It can be thought of a MT-safe version for GTK+ widgets for the
 * following use case, where you have to worry about idle_callback()
 * running in thread A and accessing @self after it has been finalized
 * in thread B:
 *
 * |[<!-- language="C" -->
 * static gboolean
 * idle_callback (gpointer data)
 * {
 *    // gdk_threads_enter(); would be needed for g_idle_add()
 *
 *    SomeWidget *self = data;
 *    // do stuff with self
 *
 *    self->idle_id = 0;
 *
 *    // gdk_threads_leave(); would be needed for g_idle_add()
 *    return FALSE;
 * }
 *
 * static void
 * some_widget_do_stuff_later (SomeWidget *self)
 * {
 *    self->idle_id = gdk_threads_add_idle (idle_callback, self)
 *    // using g_idle_add() here would require thread protection in the callback
 * }
 *
 * static void
 * some_widget_finalize (GObject *object)
 * {
 *    SomeWidget *self = SOME_WIDGET (object);
 *    if (self->idle_id)
 *      g_source_remove (self->idle_id);
 *    G_OBJECT_CLASS (parent_class)->finalize (object);
 * }
 * ]|
 *
 * Returns: the ID (greater than 0) of the event source.
 *
 * Since: 2.12
 */
guint
gdk_threads_add_idle_full (gint           priority,
                           GSourceFunc    function,
                           gpointer       data,
                           GDestroyNotify notify)
{
  GdkThreadsDispatch *dispatch;

  g_return_val_if_fail (function != NULL, 0);

  dispatch = g_slice_new (GdkThreadsDispatch);
  dispatch->func = function;
  dispatch->data = data;
  dispatch->destroy = notify;

  return g_idle_add_full (priority,
                          gdk_threads_dispatch,
                          dispatch,
                          gdk_threads_dispatch_free);
}

/**
 * gdk_threads_add_idle: (skip)
 * @function: function to call
 * @data:     data to pass to @function
 *
 * A wrapper for the common usage of gdk_threads_add_idle_full() 
 * assigning the default priority, #G_PRIORITY_DEFAULT_IDLE.
 *
 * See gdk_threads_add_idle_full().
 *
 * Returns: the ID (greater than 0) of the event source.
 * 
 * Since: 2.12
 */
guint
gdk_threads_add_idle (GSourceFunc    function,
                      gpointer       data)
{
  return gdk_threads_add_idle_full (G_PRIORITY_DEFAULT_IDLE,
                                    function, data, NULL);
}


/**
 * gdk_threads_add_timeout_full: (rename-to gdk_threads_add_timeout)
 * @priority: the priority of the timeout source. Typically this will be in the
 *            range between #G_PRIORITY_DEFAULT_IDLE and #G_PRIORITY_HIGH_IDLE.
 * @interval: the time between calls to the function, in milliseconds
 *             (1/1000ths of a second)
 * @function: function to call
 * @data:     data to pass to @function
 * @notify: (allow-none):   function to call when the timeout is removed, or %NULL
 *
 * Sets a function to be called at regular intervals holding the GDK lock,
 * with the given priority.  The function is called repeatedly until it 
 * returns %FALSE, at which point the timeout is automatically destroyed 
 * and the function will not be called again.  The @notify function is
 * called when the timeout is destroyed.  The first call to the
 * function will be at the end of the first @interval.
 *
 * Note that timeout functions may be delayed, due to the processing of other
 * event sources. Thus they should not be relied on for precise timing.
 * After each call to the timeout function, the time of the next
 * timeout is recalculated based on the current time and the given interval
 * (it does not try to “catch up” time lost in delays).
 *
 * This variant of g_timeout_add_full() can be thought of a MT-safe version 
 * for GTK+ widgets for the following use case:
 *
 * |[<!-- language="C" -->
 * static gboolean timeout_callback (gpointer data)
 * {
 *    SomeWidget *self = data;
 *    
 *    // do stuff with self
 *    
 *    self->timeout_id = 0;
 *    
 *    return G_SOURCE_REMOVE;
 * }
 *  
 * static void some_widget_do_stuff_later (SomeWidget *self)
 * {
 *    self->timeout_id = g_timeout_add (timeout_callback, self)
 * }
 *  
 * static void some_widget_finalize (GObject *object)
 * {
 *    SomeWidget *self = SOME_WIDGET (object);
 *    
 *    if (self->timeout_id)
 *      g_source_remove (self->timeout_id);
 *    
 *    G_OBJECT_CLASS (parent_class)->finalize (object);
 * }
 * ]|
 *
 * Returns: the ID (greater than 0) of the event source.
 * 
 * Since: 2.12
 */
guint
gdk_threads_add_timeout_full (gint           priority,
                              guint          interval,
                              GSourceFunc    function,
                              gpointer       data,
                              GDestroyNotify notify)
{
  GdkThreadsDispatch *dispatch;

  g_return_val_if_fail (function != NULL, 0);

  dispatch = g_slice_new (GdkThreadsDispatch);
  dispatch->func = function;
  dispatch->data = data;
  dispatch->destroy = notify;

  return g_timeout_add_full (priority, 
                             interval,
                             gdk_threads_dispatch, 
                             dispatch, 
                             gdk_threads_dispatch_free);
}

/**
 * gdk_threads_add_timeout: (skip)
 * @interval: the time between calls to the function, in milliseconds
 *             (1/1000ths of a second)
 * @function: function to call
 * @data:     data to pass to @function
 *
 * A wrapper for the common usage of gdk_threads_add_timeout_full() 
 * assigning the default priority, #G_PRIORITY_DEFAULT.
 *
 * See gdk_threads_add_timeout_full().
 * 
 * Returns: the ID (greater than 0) of the event source.
 *
 * Since: 2.12
 */
guint
gdk_threads_add_timeout (guint       interval,
                         GSourceFunc function,
                         gpointer    data)
{
  return gdk_threads_add_timeout_full (G_PRIORITY_DEFAULT,
                                       interval, function, data, NULL);
}


/**
 * gdk_threads_add_timeout_seconds_full: (rename-to gdk_threads_add_timeout_seconds)
 * @priority: the priority of the timeout source. Typically this will be in the
 *            range between #G_PRIORITY_DEFAULT_IDLE and #G_PRIORITY_HIGH_IDLE.
 * @interval: the time between calls to the function, in seconds
 * @function: function to call
 * @data:     data to pass to @function
 * @notify: (allow-none): function to call when the timeout is removed, or %NULL
 *
 * A variant of gdk_threads_add_timeout_full() with second-granularity.
 * See g_timeout_add_seconds_full() for a discussion of why it is
 * a good idea to use this function if you don’t need finer granularity.
 *
 * Returns: the ID (greater than 0) of the event source.
 * 
 * Since: 2.14
 */
guint
gdk_threads_add_timeout_seconds_full (gint           priority,
                                      guint          interval,
                                      GSourceFunc    function,
                                      gpointer       data,
                                      GDestroyNotify notify)
{
  GdkThreadsDispatch *dispatch;

  g_return_val_if_fail (function != NULL, 0);

  dispatch = g_slice_new (GdkThreadsDispatch);
  dispatch->func = function;
  dispatch->data = data;
  dispatch->destroy = notify;

  return g_timeout_add_seconds_full (priority, 
                                     interval,
                                     gdk_threads_dispatch, 
                                     dispatch, 
                                     gdk_threads_dispatch_free);
}

/**
 * gdk_threads_add_timeout_seconds: (skip)
 * @interval: the time between calls to the function, in seconds
 * @function: function to call
 * @data:     data to pass to @function
 *
 * A wrapper for the common usage of gdk_threads_add_timeout_seconds_full() 
 * assigning the default priority, #G_PRIORITY_DEFAULT.
 *
 * For details, see gdk_threads_add_timeout_full().
 * 
 * Returns: the ID (greater than 0) of the event source.
 *
 * Since: 2.14
 */
guint
gdk_threads_add_timeout_seconds (guint       interval,
                                 GSourceFunc function,
                                 gpointer    data)
{
  return gdk_threads_add_timeout_seconds_full (G_PRIORITY_DEFAULT,
                                               interval, function, data, NULL);
}

/**
 * gdk_get_program_class:
 *
 * Gets the program class. Unless the program class has explicitly
 * been set with gdk_set_program_class() or with the `--class`
 * commandline option, the default value is the program name (determined
 * with g_get_prgname()) with the first character converted to uppercase.
 *
 * Returns: the program class.
 */
const char *
gdk_get_program_class (void)
{
  return gdk_progclass;
}

/**
 * gdk_set_program_class:
 * @program_class: a string.
 *
 * Sets the program class. The X11 backend uses the program class to set
 * the class name part of the `WM_CLASS` property on
 * toplevel windows; see the ICCCM.
 *
 * The program class can still be overridden with the --class command
 * line option.
 */
void
gdk_set_program_class (const char *program_class)
{
  if (gdk_progclass_overridden)
    return;

  g_free (gdk_progclass);

  gdk_progclass = g_strdup (program_class);
}

/**
 * gdk_disable_multidevice:
 *
 * Disables multidevice support in GDK. This call must happen prior
 * to gdk_display_open(), gtk_init(), gtk_init_with_args() or
 * gtk_init_check() in order to take effect.
 *
 * Most common GTK+ applications won’t ever need to call this. Only
 * applications that do mixed GDK/Xlib calls could want to disable
 * multidevice support if such Xlib code deals with input devices in
 * any way and doesn’t observe the presence of XInput 2.
 *
 * Since: 3.0
 */
void
gdk_disable_multidevice (void)
{
  if (gdk_initialized)
    return;

  _gdk_disable_multidevice = TRUE;
}

PangoDirection
gdk_unichar_direction (gunichar ch)
{
  FriBidiCharType fribidi_ch_type;

  G_STATIC_ASSERT (sizeof (FriBidiChar) == sizeof (gunichar));

  fribidi_ch_type = fribidi_get_bidi_type (ch);

  if (!FRIBIDI_IS_STRONG (fribidi_ch_type))
    return PANGO_DIRECTION_NEUTRAL;
  else if (FRIBIDI_IS_RTL (fribidi_ch_type))
    return PANGO_DIRECTION_RTL;
  else
    return PANGO_DIRECTION_LTR;
}

#if defined (G_HAS_CONSTRUCTORS) && !defined (G_OS_WIN32)
#define GDK_USE_CONSTRUCTORS
#ifdef G_DEFINE_CONSTRUCTOR_NEEDS_PRAGMA
#pragma G_DEFINE_CONSTRUCTOR_PRAGMA_ARGS(stash_startup_id)
#pragma G_DEFINE_CONSTRUCTOR_PRAGMA_ARGS(stash_autostart_id)
#endif
G_DEFINE_CONSTRUCTOR(stash_startup_id)
G_DEFINE_CONSTRUCTOR(stash_autostart_id)
#endif

static char *desktop_startup_id = NULL;
static char *desktop_autostart_id = NULL;

static void
stash_startup_id (void)
{
  const char *startup_id = g_getenv ("DESKTOP_STARTUP_ID");

  if (startup_id == NULL || startup_id[0] == '\0')
    return;

  if (!g_utf8_validate (startup_id, -1, NULL))
    {
      g_warning ("DESKTOP_STARTUP_ID contains invalid UTF-8");
      return;
    }

  desktop_startup_id = g_strdup (startup_id);
}

static void
stash_autostart_id (void)
{
  const char *autostart_id = g_getenv ("DESKTOP_AUTOSTART_ID");
  desktop_autostart_id = g_strdup (autostart_id ? autostart_id : "");
}

const gchar *
gdk_get_desktop_startup_id (void)
{
  static gsize init = 0;

  if (g_once_init_enter (&init))
    {
#ifndef GDK_USE_CONSTRUCTORS
      stash_startup_id ();
#endif
      /* Clear the environment variable so it won't be inherited by
       * child processes and confuse things.
       */
      g_unsetenv ("DESKTOP_STARTUP_ID");

      g_once_init_leave (&init, 1);
    }

  return desktop_startup_id;
}

const gchar *
gdk_get_desktop_autostart_id (void)
{
  static gsize init = 0;

  if (g_once_init_enter (&init))
    {
#ifndef GDK_USE_CONSTRUCTORS
      stash_autostart_id ();
#endif
      /* Clear the environment variable so it won't be inherited by
       * child processes and confuse things.
       */
      g_unsetenv ("DESKTOP_AUTOSTART_ID");

      g_once_init_leave (&init, 1);
    }

  return desktop_autostart_id;
}
