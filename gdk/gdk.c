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

#include "gdkalias.h"
#include "gdk.h"
#include "gdkinternals.h"

#ifndef HAVE_XCONVERTCASE
#include "gdkkeysyms.h"
#endif

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
static gint    gdk_argc = 0;
static gchar **gdk_argv = NULL;

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
  {"xinerama",	    GDK_DEBUG_XINERAMA}
};

static const int gdk_ndebug_keys = G_N_ELEMENTS (gdk_debug_keys);

#endif /* G_ENABLE_DEBUG */

static GdkArgContext *
gdk_arg_context_new (gpointer cb_data)
{
  GdkArgContext *result = g_new (GdkArgContext, 1);
  result->tables = g_ptr_array_new ();
  result->cb_data = cb_data;

  return result;
}

static void
gdk_arg_context_destroy (GdkArgContext *context)
{
  g_ptr_array_free (context->tables, TRUE);
  g_free (context);
}

static void
gdk_arg_context_add_table (GdkArgContext *context, GdkArgDesc *table)
{
  g_ptr_array_add (context->tables, table);
}

static void
gdk_arg_context_parse (GdkArgContext *context, gint *argc, gchar ***argv)
{
  int i, j, k;

  if (argc && argv)
    {
      for (i = 1; i < *argc; i++)
	{
	  char *arg;
	  
	  if (!((*argv)[i][0] == '-' && (*argv)[i][1] == '-'))
	    continue;
	  
	  arg = (*argv)[i] + 2;

	  /* '--' terminates list of arguments */
	  if (*arg == 0)
	    {
	      (*argv)[i] = NULL;
	      break;
	    }
	      
	  for (j = 0; j < context->tables->len; j++)
	    {
	      GdkArgDesc *table = context->tables->pdata[j];
	      for (k = 0; table[k].name; k++)
		{
		  switch (table[k].type)
		    {
		    case GDK_ARG_STRING:
		    case GDK_ARG_CALLBACK:
		    case GDK_ARG_INT:
		      {
			int len = strlen (table[k].name);
			
			if (strncmp (arg, table[k].name, len) == 0 &&
			    (arg[len] == '=' || arg[len] == 0))
			  {
			    char *value = NULL;

			    (*argv)[i] = NULL;

			    if (arg[len] == '=')
			      value = arg + len + 1;
			    else if (i < *argc - 1)
			      {
				value = (*argv)[i + 1];
				(*argv)[i+1] = NULL;
				i++;
			      }
			    else
			      value = "";

			    switch (table[k].type)
			      {
			      case GDK_ARG_STRING:
				*(gchar **)table[k].location = g_strdup (value);
				break;
			      case GDK_ARG_INT:
				*(gint *)table[k].location = atoi (value);
				break;
			      case GDK_ARG_CALLBACK:
				(*table[k].callback)(table[k].name, value, context->cb_data);
				break;
			      default:
				;
			      }

			    goto next_arg;
			  }
			break;
		      }
		    case GDK_ARG_BOOL:
		    case GDK_ARG_NOBOOL:
		      if (strcmp (arg, table[k].name) == 0)
			{
			  (*argv)[i] = NULL;
			  
			  *(gboolean *)table[k].location = (table[k].type == GDK_ARG_BOOL) ? TRUE : FALSE;
			  goto next_arg;
			}
		    }
		}
	    }
	next_arg:
	  ;
	}
	  
      for (i = 1; i < *argc; i++)
	{
	  for (k = i; k < *argc; k++)
	    if ((*argv)[k] != NULL)
	      break;
	  
	  if (k > i)
	    {
	      k -= i;
	      for (j = i + k; j < *argc; j++)
		(*argv)[j-k] = (*argv)[j];
	      *argc -= k;
	    }
	}
    }
}

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

static void
gdk_arg_class_cb (const char *key, const char *value, gpointer user_data)
{
  gdk_set_program_class (value);
}

static void
gdk_arg_name_cb (const char *key, const char *value, gpointer user_data)
{
  g_set_prgname (value);
}

static GdkArgDesc gdk_args[] = {
  { "class" ,       GDK_ARG_CALLBACK, NULL, gdk_arg_class_cb                   },
  { "name",         GDK_ARG_CALLBACK, NULL, gdk_arg_name_cb                    },
  { "display",      GDK_ARG_STRING,   &_gdk_display_name,     (GdkArgFunc)NULL },
  { "screen",       GDK_ARG_INT,      &_gdk_screen_number,    (GdkArgFunc)NULL },

#ifdef G_ENABLE_DEBUG
  { "gdk-debug",    GDK_ARG_CALLBACK, NULL, gdk_arg_debug_cb    },
  { "gdk-no-debug", GDK_ARG_CALLBACK, NULL, gdk_arg_no_debug_cb },
#endif /* G_ENABLE_DEBUG */
  { NULL }
};


/**
 * _gdk_get_command_line_args:
 * @argv: location to store argv pointer
 * @argc: location 
 * 
 * Retrieve the command line arguments passed to gdk_init().
 * The returned argv pointer points to static storage ; no
 * copy is made.
 **/
void
_gdk_get_command_line_args (int    *argc,
			    char ***argv)
{
  *argc = gdk_argc;
  *argv = gdk_argv;
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
  GdkArgContext *arg_context;
  gint i;

  if (gdk_initialized)
    return;

  gdk_initialized = TRUE;

  /* Save a copy of the original argc and argv */
  if (argc && argv)
    {
      gdk_argc = *argc;
      
      gdk_argv = g_malloc ((gdk_argc + 1) * sizeof (char*));
      for (i = 0; i < gdk_argc; i++)
	gdk_argv[i] = g_strdup ((*argv)[i]);
      gdk_argv[gdk_argc] = NULL;
    }

  if (argc && argv && *argc > 0)
    {
      gchar *d;
      
      d = strrchr((*argv)[0], G_DIR_SEPARATOR);
      if (d != NULL)
	g_set_prgname (d + 1);
      else
	g_set_prgname ((*argv)[0]);
    }
  else
    {
      g_set_prgname ("<unknown>");
    }

  /* We set the fallback program class here, rather than lazily in
   * gdk_get_program_class, since we don't want -name to override it.
   */
  gdk_progclass = g_strdup (g_get_prgname ());
  if (gdk_progclass[0])
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
  
  arg_context = gdk_arg_context_new (NULL);
  gdk_arg_context_add_table (arg_context, gdk_args);
  gdk_arg_context_add_table (arg_context, _gdk_windowing_args);
  gdk_arg_context_parse (arg_context, argc, argv);
  gdk_arg_context_destroy (arg_context);
  
  GDK_NOTE (MISC, g_message ("progname: \"%s\"", g_get_prgname ()));

  g_type_init ();

  /* Do any setup particular to the windowing system
   */
  _gdk_windowing_init (argc, argv);
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
      g_warning ("cannot open display: %s", gdk_get_display_arg_name ());
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
gdk_threads_enter ()
{
  GDK_THREADS_ENTER ();
}

void
gdk_threads_leave ()
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
gdk_threads_init ()
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
