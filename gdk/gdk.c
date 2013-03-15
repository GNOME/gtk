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

#define GDK_DISABLE_DEPRECATION_WARNINGS 1

#include "gdkversionmacros.h"
#include "gdkmain.h"

#include "gdkinternals.h"
#include "gdkintl.h"

#ifndef HAVE_XCONVERTCASE
#include "gdkkeysyms.h"
#endif

#include <string.h>
#include <stdlib.h>


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
 * GDK_DISABLE_DEPRECATION_WARNINGS:
 *
 * A macro that should be defined before including the gdk.h header.
 * If it is defined, no compiler warnings will be produced for uses
 * of deprecated GDK APIs.
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

static GMutex gdk_threads_mutex;

static GCallback gdk_threads_lock = NULL;
static GCallback gdk_threads_unlock = NULL;

#ifdef G_ENABLE_DEBUG
static const GDebugKey gdk_debug_keys[] = {
  {"events",        GDK_DEBUG_EVENTS},
  {"misc",          GDK_DEBUG_MISC},
  {"dnd",           GDK_DEBUG_DND},
  {"xim",           GDK_DEBUG_XIM},
  {"nograbs",       GDK_DEBUG_NOGRABS},
  {"input",         GDK_DEBUG_INPUT},
  {"cursor",        GDK_DEBUG_CURSOR},
  {"multihead",     GDK_DEBUG_MULTIHEAD},
  {"xinerama",      GDK_DEBUG_XINERAMA},
  {"draw",          GDK_DEBUG_DRAW},
  {"eventloop",     GDK_DEBUG_EVENTLOOP},
  {"frames",        GDK_DEBUG_FRAMES},
  {"settings",      GDK_DEBUG_SETTINGS}
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
  { "display",      0, G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING,   &_gdk_display_name,
    /* Description of --display=DISPLAY in --help output */    N_("X display to use"),
    /* Placeholder in --display=DISPLAY in --help output */    N_("DISPLAY") },
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

/**
 * gdk_add_option_entries_libgtk_only:
 * @group: An option group.
 *
 * Appends gdk option entries to the passed in option group. This is
 * not public API and must not be used by applications.
 */
void
gdk_add_option_entries_libgtk_only (GOptionGroup *group)
{
  g_option_group_add_entries (group, gdk_args);
}

void
gdk_pre_parse_libgtk_only (void)
{
  const char *rendering_mode;

  gdk_initialized = TRUE;

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
  }
#endif  /* G_ENABLE_DEBUG */

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
 * You shouldn't call this function explicitely if you are using
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

  gdk_pre_parse_libgtk_only ();

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
 * <envar>DISPLAY</envar> environment variable or the
 * <option>--display</option> command line option.
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
 * Returns: the display name, if specified explicitely, otherwise %NULL
 *   this string is owned by GTK+ and must not be modified or freed.
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

/**
 * gdk_display_open_default_libgtk_only:
 *
 * Opens the default display specified by command line arguments or
 * environment variables, sets it as the default display, and returns
 * it. gdk_parse_args() must have been called first. If the default
 * display has previously been set, simply returns that. An internal
 * function that should not be used by applications.
 *
 * Return value: (transfer none): the default display, if it could be
 *   opened, otherwise %NULL.
 **/
GdkDisplay *
gdk_display_open_default_libgtk_only (void)
{
  GdkDisplay *display;

  g_return_val_if_fail (gdk_initialized, NULL);

  display = gdk_display_get_default ();
  if (display)
    return display;

  display = gdk_display_open (gdk_get_display_arg_name ());

  return display;
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

  return gdk_display_open_default_libgtk_only () != NULL;
}


/**
 * gdk_init:
 * @argc: (inout): the number of command line arguments.
 * @argv: (array length=argc) (inout): the array of command line arguments.
 *
 * Initializes the GDK library and connects to the windowing system.
 * If initialization fails, a warning message is output and the application
 * terminates with a call to <literal>exit(1)</literal>.
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
 * GTK+ is "thread aware" but not thread safe &mdash; it provides a
 * global lock controlled by gdk_threads_enter()/gdk_threads_leave()
 * which protects all use of GTK+. That is, only one thread can use GTK+
 * at any given time.
 *
 * You must call gdk_threads_init() before executing any other GTK+ or
 * GDK functions in a threaded GTK+ program.
 *
 * Idles, timeouts, and input functions from GLib, such as g_idle_add(),
 * are executed outside of the main GTK+ lock. So, if you need to call
 * GTK+ inside of such a callback, you must surround the callback with
 * a gdk_threads_enter()/gdk_threads_leave() pair or use
 * gdk_threads_add_idle_full() which does this for you.
 * However, event dispatching from the mainloop is still executed within
 * the main GTK+ lock, so callback functions connected to event signals
 * like #GtkWidget::button-press-event, do not need thread protection.
 *
 * In particular, this means, if you are writing widgets that might
 * be used in threaded programs, you <emphasis>must</emphasis> surround
 * timeouts and idle functions in this matter.
 *
 * As always, you must also surround any calls to GTK+ not made within
 * a signal handler with a gdk_threads_enter()/gdk_threads_leave() pair.
 *
 * Before calling gdk_threads_leave() from a thread other
 * than your main thread, you probably want to call gdk_flush()
 * to send all pending commands to the windowing system.
 * (The reason you don't need to do this from the main thread
 * is that GDK always automatically flushes pending commands
 * when it runs out of incoming events to process and has
 * to sleep while waiting for more events.)
 *
 * A minimal main program for a threaded GTK+ application
 * looks like:
 * <informalexample>
 * <programlisting role="C">
 * int
 * main (int argc, char *argv[])
 * {
 *   GtkWidget *window;
 *
 *   gdk_threads_init (<!-- -->);
 *   gdk_threads_enter (<!-- -->);
 *
 *   gtk_init (&argc, &argv);
 *
 *   window = create_window (<!-- -->);
 *   gtk_widget_show (window);
 *
 *   gtk_main (<!-- -->);
 *   gdk_threads_leave (<!-- -->);
 *
 *   return 0;
 * }
 * </programlisting>
 * </informalexample>
 *
 * Callbacks require a bit of attention. Callbacks from GTK+ signals
 * are made within the GTK+ lock. However callbacks from GLib (timeouts,
 * IO callbacks, and idle functions) are made outside of the GTK+
 * lock. So, within a signal handler you do not need to call
 * gdk_threads_enter(), but within the other types of callbacks, you
 * do.
 *
 * Erik Mouw contributed the following code example to
 * illustrate how to use threads within GTK+ programs.
 * <informalexample>
 * <programlisting role="C">
 * /<!---->*-------------------------------------------------------------------------
 *  * Filename:      gtk-thread.c
 *  * Version:       0.99.1
 *  * Copyright:     Copyright (C) 1999, Erik Mouw
 *  * Author:        Erik Mouw &lt;J.A.K.Mouw@its.tudelft.nl&gt;
 *  * Description:   GTK threads example.
 *  * Created at:    Sun Oct 17 21:27:09 1999
 *  * Modified by:   Erik Mouw &lt;J.A.K.Mouw@its.tudelft.nl&gt;
 *  * Modified at:   Sun Oct 24 17:21:41 1999
 *  *-----------------------------------------------------------------------*<!---->/
 * /<!---->*
 *  * Compile with:
 *  *
 *  * cc -o gtk-thread gtk-thread.c `gtk-config --cflags --libs gthread`
 *  *
 *  * Thanks to Sebastian Wilhelmi and Owen Taylor for pointing out some
 *  * bugs.
 *  *
 *  *<!---->/
 *
 * #include <stdio.h>
 * #include <stdlib.h>
 * #include <unistd.h>
 * #include <time.h>
 * #include <gtk/gtk.h>
 * #include <glib.h>
 * #include <pthread.h>
 *
 * #define YES_IT_IS    (1)
 * #define NO_IT_IS_NOT (0)
 *
 * typedef struct
 * {
 *   GtkWidget *label;
 *   int what;
 * } yes_or_no_args;
 *
 * G_LOCK_DEFINE_STATIC (yes_or_no);
 * static volatile int yes_or_no = YES_IT_IS;
 *
 * void destroy (GtkWidget *widget, gpointer data)
 * {
 *   gtk_main_quit (<!-- -->);
 * }
 *
 * void *argument_thread (void *args)
 * {
 *   yes_or_no_args *data = (yes_or_no_args *)args;
 *   gboolean say_something;
 *
 *   for (;;)
 *     {
 *       /<!---->* sleep a while *<!---->/
 *       sleep(rand(<!-- -->) / (RAND_MAX / 3) + 1);
 *
 *       /<!---->* lock the yes_or_no_variable *<!---->/
 *       G_LOCK(yes_or_no);
 *
 *       /<!---->* do we have to say something? *<!---->/
 *       say_something = (yes_or_no != data->what);
 *
 *       if(say_something)
 *      {
 *        /<!---->* set the variable *<!---->/
 *        yes_or_no = data->what;
 *      }
 *
 *       /<!---->* Unlock the yes_or_no variable *<!---->/
 *       G_UNLOCK (yes_or_no);
 *
 *       if (say_something)
 *      {
 *        /<!---->* get GTK thread lock *<!---->/
 *        gdk_threads_enter (<!-- -->);
 *
 *        /<!---->* set label text *<!---->/
 *        if(data->what == YES_IT_IS)
 *          gtk_label_set_text (GTK_LABEL (data->label), "O yes, it is!");
 *        else
 *          gtk_label_set_text (GTK_LABEL (data->label), "O no, it isn't!");
 *
 *        /<!---->* release GTK thread lock *<!---->/
 *        gdk_threads_leave (<!-- -->);
 *      }
 *     }
 *
 *   return NULL;
 * }
 *
 * int main (int argc, char *argv[])
 * {
 *   GtkWidget *window;
 *   GtkWidget *label;
 *   yes_or_no_args yes_args, no_args;
 *   pthread_t no_tid, yes_tid;
 *
 *   /<!---->* init threads *<!---->/
 *   gdk_threads_init (<!-- -->);
 *   gdk_threads_enter (<!-- -->);
 *
 *   /<!---->* init gtk *<!---->/
 *   gtk_init(&argc, &argv);
 *
 *   /<!---->* init random number generator *<!---->/
 *   srand ((unsigned int) time (NULL));
 *
 *   /<!---->* create a window *<!---->/
 *   window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
 *
 *   g_signal_connect (window, "destroy", G_CALLBACK (destroy), NULL);
 *
 *   gtk_container_set_border_width (GTK_CONTAINER (window), 10);
 *
 *   /<!---->* create a label *<!---->/
 *   label = gtk_label_new ("And now for something completely different ...");
 *   gtk_container_add (GTK_CONTAINER (window), label);
 *
 *   /<!---->* show everything *<!---->/
 *   gtk_widget_show (label);
 *   gtk_widget_show (window);
 *
 *   /<!---->* create the threads *<!---->/
 *   yes_args.label = label;
 *   yes_args.what = YES_IT_IS;
 *   pthread_create (&yes_tid, NULL, argument_thread, &yes_args);
 *
 *   no_args.label = label;
 *   no_args.what = NO_IT_IS_NOT;
 *   pthread_create (&no_tid, NULL, argument_thread, &no_args);
 *
 *   /<!---->* enter the GTK main loop *<!---->/
 *   gtk_main (<!-- -->);
 *   gdk_threads_leave (<!-- -->);
 *
 *   return 0;
 * }
 * </programlisting>
 * </informalexample>
 *
 * Unfortunately, all of the above documentation holds with the X11
 * backend only. With the Win32 or Quartz backends, GDK and GTK+ calls
 * must occur only in the main thread (see below). When using Python,
 * even on X11 combining the GDK lock with other locks such as the
 * Python global interpreter lock can be complicated.
 *
 * For these reasons, the threading support has been deprecated in
 * GTK+ 3.6. Instead of calling GTK+ directly from multiple threads,
 * it is recommended to use g_idle_add(), g_main_context_invoke()
 * and similar functions to make these calls from the main thread
 * instead. The main thread is the thread which has called gtk_init()
 * and is running the GTK+ mainloop. GTK+ itself will continue to
 * use the GDK lock internally as long as the deprecated functionality
 * is still available, and other libraries should probably do the same.
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
 * Most threaded GTK+ apps won't need to use this method.
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
 * gdk_threads_add_idle_full:
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
 * |[
 * static gboolean
 * idle_callback (gpointer data)
 * {
 *    /&ast; gdk_threads_enter(); would be needed for g_idle_add() &ast;/
 *
 *    SomeWidget *self = data;
 *    /&ast; do stuff with self &ast;/
 *
 *    self->idle_id = 0;
 *
 *    /&ast; gdk_threads_leave(); would be needed for g_idle_add() &ast;/
 *    return FALSE;
 * }
 *
 * static void
 * some_widget_do_stuff_later (SomeWidget *self)
 * {
 *    self->idle_id = gdk_threads_add_idle (idle_callback, self)
 *    /&ast; using g_idle_add() here would require thread protection in the callback &ast;/
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
 * Return value: the ID (greater than 0) of the event source.
 *
 * Since: 2.12
 * Rename to: gdk_threads_add_idle
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
 * Return value: the ID (greater than 0) of the event source.
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
 * gdk_threads_add_timeout_full:
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
 * (it does not try to 'catch up' time lost in delays).
 *
 * This variant of g_timeout_add_full() can be thought of a MT-safe version 
 * for GTK+ widgets for the following use case:
 *
 * |[
 * static gboolean timeout_callback (gpointer data)
 * {
 *    SomeWidget *self = data;
 *    
 *    /&ast; do stuff with self &ast;/
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
 * Return value: the ID (greater than 0) of the event source.
 * 
 * Since: 2.12
 * Rename to: gdk_threads_add_timeout
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
 * Return value: the ID (greater than 0) of the event source.
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
 * gdk_threads_add_timeout_seconds_full:
 * @priority: the priority of the timeout source. Typically this will be in the
 *            range between #G_PRIORITY_DEFAULT_IDLE and #G_PRIORITY_HIGH_IDLE.
 * @interval: the time between calls to the function, in seconds
 * @function: function to call
 * @data:     data to pass to @function
 * @notify: (allow-none): function to call when the timeout is removed, or %NULL
 *
 * A variant of gdk_threads_add_timeout_full() with second-granularity.
 * See g_timeout_add_seconds_full() for a discussion of why it is
 * a good idea to use this function if you don't need finer granularity.
 *
 *  Return value: the ID (greater than 0) of the event source.
 * 
 * Since: 2.14
 * Rename to: gdk_threads_add_timeout_seconds
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
 * Return value: the ID (greater than 0) of the event source.
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
 * been set with gdk_set_program_class() or with the <option>--class</option>
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
 * the class name part of the <literal>WM_CLASS</literal> property on
 * toplevel windows; see the ICCCM.
 */
void
gdk_set_program_class (const char *program_class)
{
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
 * Most common GTK+ applications won't ever need to call this. Only
 * applications that do mixed GDK/Xlib calls could want to disable
 * multidevice support if such Xlib code deals with input devices in
 * any way and doesn't observe the presence of XInput 2.
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
