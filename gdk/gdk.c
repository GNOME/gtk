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

#include "config.h"

#include <string.h>
#include <stdlib.h>

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

  /* Save a copy of the original argc and argv */
  if (argc && argv)
    {
      for (i = 1; i < *argc; i++)
	{
	  char *arg;
	  
	  if (!(*argv)[i][0] == '-' && (*argv)[i][1] == '-')
	    continue;
	  
	  arg = (*argv)[i] + 2;

	  /* '--' terminates list of arguments */
	  if (arg == 0)
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
 * use by calls to gdk_open_display().
 *
 * Any arguments used by GDK are removed from the array and @argc and @argv are
 * updated accordingly.
 *
 * You shouldn't call this function explicitely if you are using
 * gtk_init(), gtk_init_check(), gdk_init(), or gdk_init_check().
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

  if (argc && argv)
    {
      gdk_argc = *argc;
      
      gdk_argv = g_malloc ((gdk_argc + 1) * sizeof (char*));
      for (i = 0; i < gdk_argc; i++)
	gdk_argv[i] = g_strdup ((*argv)[i]);
      gdk_argv[gdk_argc] = NULL;

      if (*argc > 0)
	{
	  gchar *d;
	  
	  d = strrchr((*argv)[0], G_DIR_SEPARATOR);
	  if (d != NULL)
	    g_set_prgname (d + 1);
	  else
	    g_set_prgname ((*argv)[0]);
	}
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
  _gdk_windowing_init ();
}

/** 
 * gdk_get_display_arg_name:
 *
 * Gets the display name specified in the command line arguments passed
 * to gdk_init() or gdk_parse_args(), if any.
 *
 * Returns: the display name, if specified explicitely, otherwise %NULL
 */
gchar *
gdk_get_display_arg_name (void)
{
  return _gdk_display_name;
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
  GdkDisplay *display;
  
  gdk_parse_args (argc, argv);

  if (gdk_get_default_display ())
    return TRUE;
    
  display = gdk_open_display (_gdk_display_name);

  if (display)
    {
      gdk_display_manager_set_default_display (gdk_display_manager_get (),
					       display);
      return TRUE;
    }
  else
    return FALSE;
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
  /* de-initialisation is done by the gdk_exit_funct(),
     no need to do this here (Alex J.) */
  exit (errorcode);
}

#if 0

/* This is disabled, but the code isn't removed, because we might
 * want to have some sort of explicit way to shut down GDK cleanly
 * at some point in the future.
 */

/*
 *--------------------------------------------------------------
 * gdk_exit_func
 *
 *   This is the "atexit" function that makes sure the
 *   library gets a chance to cleanup.
 *
 * Arguments:
 *
 * Results:
 *
 * Side effects:
 *   The library is un-initialized and the program exits.
 *
 *--------------------------------------------------------------
 */

static void
gdk_exit_func (void)
{
  static gboolean in_gdk_exit_func = FALSE;
  
  /* This is to avoid an infinite loop if a program segfaults in
     an atexit() handler (and yes, it does happen, especially if a program
     has trounced over memory too badly for even g_message to work) */
  if (in_gdk_exit_func == TRUE)
    return;
  in_gdk_exit_func = TRUE;
  
  if (gdk_initialized)
    {
      _gdk_image_exit ();
      _gdk_input_exit ();

      _gdk_windowing_exit ();
      
      gdk_initialized = 0;
    }
}

#endif

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
