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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <config.h>

#include <string.h>
#include <stdlib.h>

#include "gdk.h"
#include "gdkinternals.h"
#include "gdkintl.h"

#ifndef HAVE_XCONVERTCASE
#include "gdkkeysyms.h"
#endif
#include "gdkalias.h"

typedef struct _GdkPredicate  GdkPredicate;

struct _GdkPredicate
{
  GdkEventFunc func;
  gpointer data;
};

/* Private variable declarations
 */
static int gdk_initialized = 0;			    /* 1 if the library is initialized,
						     * 0 otherwise.
						     */

static gchar  *gdk_progclass = NULL;

#ifdef G_ENABLE_DEBUG
static const GDebugKey gdk_debug_keys[] = {
  {"events",	    GDK_DEBUG_EVENTS},
  {"misc",	    GDK_DEBUG_MISC},
  {"dnd",	    GDK_DEBUG_DND},
  {"xim",	    GDK_DEBUG_XIM},
  {"nograbs",       GDK_DEBUG_NOGRABS},
  {"colormap",	    GDK_DEBUG_COLORMAP},
  {"gdkrgb",	    GDK_DEBUG_GDKRGB},
  {"gc",	    GDK_DEBUG_GC},
  {"pixmap",	    GDK_DEBUG_PIXMAP},
  {"image",	    GDK_DEBUG_IMAGE},
  {"input",	    GDK_DEBUG_INPUT},
  {"cursor",	    GDK_DEBUG_CURSOR},
  {"multihead",	    GDK_DEBUG_MULTIHEAD},
  {"xinerama",	    GDK_DEBUG_XINERAMA},
  {"draw",	    GDK_DEBUG_DRAW}
};

static const int gdk_ndebug_keys = G_N_ELEMENTS (gdk_debug_keys);

#endif /* G_ENABLE_DEBUG */

#ifdef G_ENABLE_DEBUG
static void
gdk_arg_debug_cb (const char *key, const char *value, gpointer user_data)
{
  _gdk_debug_flags |= g_parse_debug_string (value,
					    (GDebugKey *) gdk_debug_keys,
					    gdk_ndebug_keys);
}

static void
gdk_arg_no_debug_cb (const char *key, const char *value, gpointer user_data)
{
  _gdk_debug_flags &= ~g_parse_debug_string (value,
					     (GDebugKey *) gdk_debug_keys,
					     gdk_ndebug_keys);
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
  { "screen",       0, 0, G_OPTION_ARG_INT,      &_gdk_screen_number,
    /* Description of --screen=SCREEN in --help output */      N_("X screen to use"),
    /* Placeholder in --screen=SCREEN in --help output */      N_("SCREEN") },
#ifdef G_ENABLE_DEBUG
  { "gdk-debug",    0, 0, G_OPTION_ARG_CALLBACK, gdk_arg_debug_cb,  
    /* Description of --gdk-debug=FLAGS in --help output */    N_("Gdk debugging flags to set"),
    /* Placeholder in --gdk-debug=FLAGS in --help output */    N_("FLAGS") },
  { "gdk-no-debug", 0, 0, G_OPTION_ARG_CALLBACK, gdk_arg_no_debug_cb, 
    /* Description of --gdk-no-debug=FLAGS in --help output */ N_("Gdk debugging flags to unset"), 
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
 **/
void
gdk_add_option_entries_libgtk_only (GOptionGroup *group)
{
  g_option_group_add_entries (group, gdk_args);
  g_option_group_add_entries (group, _gdk_windowing_args);
}

void
gdk_pre_parse_libgtk_only (void)
{
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
					      gdk_ndebug_keys);
  }
#endif	/* G_ENABLE_DEBUG */

  g_type_init ();

  /* Do any setup particular to the windowing system
   */
  _gdk_windowing_init ();  
}

  
/**
 * gdk_parse_args:
 * @argc: the number of command line arguments.
 * @argv: the array of command line arguments.
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
  g_option_group_add_entries (option_group, _gdk_windowing_args);

  if (!g_option_context_parse (option_context, argc, argv, &error))
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }
  g_option_context_free (option_context);
  
  GDK_NOTE (MISC, g_message ("progname: \"%s\"", g_get_prgname ()));
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
G_CONST_RETURN gchar *
gdk_get_display_arg_name (void)
{
  if (!_gdk_display_arg_name)
    {
      if (_gdk_screen_number >= 0)
	_gdk_display_arg_name = _gdk_windowing_substitute_screen_number (_gdk_display_name, _gdk_screen_number);
      else
	_gdk_display_arg_name = g_strdup (_gdk_display_name);
   }

   return _gdk_display_arg_name;
}

/**
 * gdk_display_open_default_libgtk_only:
 * 
 * Opens the default display specified by command line arguments or
 * environment variables, sets it as the default display, and returns
 * it.  gdk_parse_args must have been called first. If the default
 * display has previously been set, simply returns that. An internal
 * function that should not be used by applications.
 * 
 * Return value: the default display, if it could be opened,
 *   otherwise %NULL.
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

  if (!display && _gdk_screen_number >= 0)
    {
      g_free (_gdk_display_arg_name);
      _gdk_display_arg_name = g_strdup (_gdk_display_name);
      
      display = gdk_display_open (_gdk_display_name);
    }
  
  if (display)
    gdk_display_manager_set_default_display (gdk_display_manager_get (),
					     display);
  
  return display;
}

/*
 *--------------------------------------------------------------
 * gdk_init_check
 *
 *   Initialize the library for use.
 *
 * Arguments:
 *   "argc" is the number of arguments.
 *   "argv" is an array of strings.
 *
 * Results:
 *   "argc" and "argv" are modified to reflect any arguments
 *   which were not handled. (Such arguments should either
 *   be handled by the application or dismissed). If initialization
 *   fails, returns FALSE, otherwise TRUE.
 *
 * Side effects:
 *   The library is initialized.
 *
 *--------------------------------------------------------------
 */

gboolean
gdk_init_check (int    *argc,
		char ***argv)
{
  gdk_parse_args (argc, argv);

  return gdk_display_open_default_libgtk_only () != NULL;
}

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

/*
 *--------------------------------------------------------------
 * gdk_exit
 *
 *   Restores the library to an un-itialized state and exits
 *   the program using the "exit" system call.
 *
 * Arguments:
 *   "errorcode" is the error value to pass to "exit".
 *
 * Results:
 *   Allocated structures are freed and the program exits
 *   cleanly.
 *
 * Side effects:
 *
 *--------------------------------------------------------------
 */

void
gdk_exit (gint errorcode)
{
  exit (errorcode);
}

void
gdk_threads_enter (void)
{
  GDK_THREADS_ENTER ();
}

void
gdk_threads_leave (void)
{
  GDK_THREADS_LEAVE ();
}

static void
gdk_threads_impl_lock (void)
{
  if (gdk_threads_mutex)
    g_mutex_lock (gdk_threads_mutex);
}

static void
gdk_threads_impl_unlock (void)
{
  if (gdk_threads_mutex)
    g_mutex_unlock (gdk_threads_mutex);
}

/**
 * gdk_threads_init:
 * 
 * Initializes GDK so that it can be used from multiple threads
 * in conjunction with gdk_threads_enter() and gdk_threads_leave().
 * g_thread_init() must be called previous to this function.
 *
 * This call must be made before any use of the main loop from
 * GTK+; to be safe, call it before gtk_init().
 **/
void
gdk_threads_init (void)
{
  if (!g_thread_supported ())
    g_error ("g_thread_init() must be called before gdk_threads_init()");

  gdk_threads_mutex = g_mutex_new ();
  if (!gdk_threads_lock)
    gdk_threads_lock = gdk_threads_impl_lock;
  if (!gdk_threads_unlock)
    gdk_threads_unlock = gdk_threads_impl_unlock;
}

/**
 * gdk_threads_set_lock_functions:
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

G_CONST_RETURN char *
gdk_get_program_class (void)
{
  return gdk_progclass;
}

void
gdk_set_program_class (const char *program_class)
{
  if (gdk_progclass)
    g_free (gdk_progclass);

  gdk_progclass = g_strdup (program_class);
}

#define __GDK_C__
#include "gdkaliasdef.c"
