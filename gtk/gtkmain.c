/* GTK - The GIMP Toolkit
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

#include "gdkconfig.h"

#include <locale.h>

#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
#include <libintl.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gmodule.h>
#ifdef G_OS_UNIX
#include <unistd.h>
#endif

#include <pango/pango-utils.h>	/* For pango_split_file_list */

#include "gtkdnd.h"
#include "gtkversion.h"
#include "gtkmain.h"
#include "gtkrc.h"
#include "gtkselection.h"
#include "gtksettings.h"
#include "gtksignal.h"
#include "gtkwidget.h"
#include "gtkwindow.h"
#include "gtkprivate.h"
#include "gdk/gdki18n.h"
#include "config.h"
#include "gtkdebug.h"
#include "gtkintl.h"

/* Private type definitions
 */
typedef struct _GtkInitFunction		 GtkInitFunction;
typedef struct _GtkQuitFunction		 GtkQuitFunction;
typedef struct _GtkClosure	         GtkClosure;
typedef struct _GtkKeySnooperData	 GtkKeySnooperData;

struct _GtkInitFunction
{
  GtkFunction function;
  gpointer data;
};

struct _GtkQuitFunction
{
  guint id;
  guint main_level;
  GtkCallbackMarshal marshal;
  GtkFunction function;
  gpointer data;
  GtkDestroyNotify destroy;
};

struct _GtkClosure
{
  GtkCallbackMarshal marshal;
  gpointer data;
  GtkDestroyNotify destroy;
};

struct _GtkKeySnooperData
{
  GtkKeySnoopFunc func;
  gpointer func_data;
  guint id;
};

static void  gtk_exit_func		 (void);
static gint  gtk_quit_invoke_function	 (GtkQuitFunction    *quitf);
static void  gtk_quit_destroy		 (GtkQuitFunction    *quitf);
static gint  gtk_invoke_key_snoopers	 (GtkWidget	     *grab_widget,
					  GdkEvent	     *event);

static void     gtk_destroy_closure      (gpointer            data);
static gboolean gtk_invoke_idle_timeout  (gpointer            data);
static void     gtk_invoke_input         (gpointer            data,
					  gint                source,
					  GdkInputCondition   condition);

#if 0
static void  gtk_error			 (gchar		     *str);
static void  gtk_warning		 (gchar		     *str);
static void  gtk_message		 (gchar		     *str);
static void  gtk_print			 (gchar		     *str);
#endif

static GtkWindowGroup *gtk_main_get_window_group (GtkWidget   *widget);

const guint gtk_major_version = GTK_MAJOR_VERSION;
const guint gtk_minor_version = GTK_MINOR_VERSION;
const guint gtk_micro_version = GTK_MICRO_VERSION;
const guint gtk_binary_age = GTK_BINARY_AGE;
const guint gtk_interface_age = GTK_INTERFACE_AGE;

static guint gtk_main_loop_level = 0;
static gint gtk_initialized = FALSE;
static GList *current_events = NULL;

static GSList *main_loops = NULL;      /* stack of currently executing main loops */

static GList *init_functions = NULL;	   /* A list of init functions.
					    */
static GList *quit_functions = NULL;	   /* A list of quit functions.
					    */
static GMemChunk *quit_mem_chunk = NULL;

static GSList *key_snoopers = NULL;

static GdkVisual *gtk_visual;		   /* The visual to be used in creating new
					    *  widgets.
					    */
static GdkColormap *gtk_colormap;	   /* The colormap to be used in creating new
					    *  widgets.
					    */

guint gtk_debug_flags = 0;		   /* Global GTK debug flag */

#ifdef G_ENABLE_DEBUG
static const GDebugKey gtk_debug_keys[] = {
  {"misc", GTK_DEBUG_MISC},
  {"plugsocket", GTK_DEBUG_PLUGSOCKET},
  {"text", GTK_DEBUG_TEXT},
  {"tree", GTK_DEBUG_TREE},
  {"updates", GTK_DEBUG_UPDATES}
};

static const guint gtk_ndebug_keys = sizeof (gtk_debug_keys) / sizeof (GDebugKey);

#endif /* G_ENABLE_DEBUG */

gchar*
gtk_check_version (guint required_major,
		   guint required_minor,
		   guint required_micro)
{
  if (required_major > GTK_MAJOR_VERSION)
    return "Gtk+ version too old (major mismatch)";
  if (required_major < GTK_MAJOR_VERSION)
    return "Gtk+ version too new (major mismatch)";
  if (required_minor > GTK_MINOR_VERSION)
    return "Gtk+ version too old (minor mismatch)";
  if (required_minor < GTK_MINOR_VERSION)
    return "Gtk+ version too new (minor mismatch)";
  if (required_micro < GTK_MICRO_VERSION - GTK_BINARY_AGE)
    return "Gtk+ version too new (micro mismatch)";
  if (required_micro > GTK_MICRO_VERSION)
    return "Gtk+ version too old (micro mismatch)";
  return NULL;
}

#undef gtk_init_check

/* This checks to see if the process is running suid or sgid
 * at the current time. If so, we don't allow GTK+ to be initialized.
 * This is meant to be a mild check - we only error out if we
 * can prove the programmer is doing something wrong, not if
 * they could be doing something wrong. For this reason, we
 * don't use issetugid() on BSD or prctl (PR_GET_DUMPABLE).
 */
static gboolean
check_setugid (void)
{
/* this isn't at all relevant on MS Windows and doesn't compile ... --hb */
#ifndef G_OS_WIN32
  uid_t ruid, euid, suid; /* Real, effective and saved user ID's */
  gid_t rgid, egid, sgid; /* Real, effective and saved group ID's */
  
#ifdef HAVE_GETRESUID
  /* These aren't in the header files, so we prototype them here.
   */
  int getresuid(uid_t *ruid, uid_t *euid, uid_t *suid);
  int getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid);

  if (getresuid (&ruid, &euid, &suid) != 0 ||
      getresgid (&rgid, &egid, &sgid) != 0)
#endif /* HAVE_GETRESUID */
    {
      suid = ruid = getuid ();
      sgid = rgid = getgid ();
      euid = geteuid ();
      egid = getegid ();
    }

  if (ruid != euid || ruid != suid ||
      rgid != egid || rgid != sgid)
    {
      g_warning ("This process is currently running setuid or setgid.\n"
		 "This is not a supported use of GTK+. You must create a helper\n"
		 "program instead. For further details, see:\n\n"
		 "    http://www.gtk.org/setuid.html\n\n"
		 "Refusing to initialize GTK+.");
      exit (1);
    }
#endif
  return TRUE;
}

static gchar **
get_module_path (void)
{
  gchar *module_path = g_getenv ("GTK_MODULE_PATH");
  gchar *exe_prefix = g_getenv("GTK_EXE_PREFIX");
  gchar **result;
  gchar *default_dir;

  if (exe_prefix)
    default_dir = g_build_filename (exe_prefix, "lib", "gtk-2.0", "modules", NULL);
  else
    {
#ifndef G_OS_WIN32
      default_dir = g_build_filename (GTK_LIBDIR, "gtk-2.0", "modules", NULL);
#else
      default_dir = g_build_filename (get_gtk_win32_directory (""), "modules", NULL);
#endif
    }
  module_path = g_strconcat (module_path ? module_path : "",
			     module_path ? G_SEARCHPATH_SEPARATOR_S : "",
			     default_dir, NULL);

  result = pango_split_file_list (module_path);

  g_free (default_dir);
  g_free (module_path);

  return result;
}

static GModule *
find_module (gchar      **module_path,
	     const gchar *name)
{
  GModule *module;
  gchar *module_name;
  gint i;

  if (g_path_is_absolute (name))
    return g_module_open (name, G_MODULE_BIND_LAZY);

  for (i = 0; module_path[i]; i++)
    {
      gchar *version_directory;

#ifndef G_OS_WIN32 /* ignoring GTK_BINARY_VERSION elsewhere too */
      version_directory = g_build_filename (module_path[i], GTK_BINARY_VERSION, NULL);
      module_name = g_module_build_path (version_directory, name);
      g_free (version_directory);
      
      if (g_file_test (module_name, G_FILE_TEST_EXISTS))
	{
	  g_free (module_name);
	  return g_module_open (module_name, G_MODULE_BIND_LAZY);
	}
      
      g_free (module_name);
#endif

      module_name = g_module_build_path (module_path[i], name);
      
      if (g_file_test (module_name, G_FILE_TEST_EXISTS))
	{
	  module = g_module_open (module_name, G_MODULE_BIND_LAZY);
	  g_free (module_name);
	  return module;
	}

      g_free (module_name);
    }

  /* As last resort, try loading without an absolute path (using system
   * library path)
   */
  module_name = g_module_build_path (NULL, name);
  module = g_module_open (module_name, G_MODULE_BIND_LAZY);
  g_free(module_name);

  return module;
}

static GSList *
load_module (GSList      *gtk_modules,
	     gchar      **module_path,
	     const gchar *name)
{
  GtkModuleInitFunc modinit_func = NULL;
  GModule *module = NULL;
  
  if (g_module_supported ())
    {
      module = find_module (module_path, name);
      if (module &&
	  g_module_symbol (module, "gtk_module_init", (gpointer*) &modinit_func) &&
	  modinit_func)
	{
	  if (!g_slist_find (gtk_modules, modinit_func))
	    {
	      g_module_make_resident (module);
	      gtk_modules = g_slist_prepend (gtk_modules, modinit_func);
	    }
	  else
	    {
	      g_module_close (module);
	      module = NULL;
	    }
	}
    }
  if (!modinit_func)
    {
      g_message ("Failed to load module \"%s\": %s",
		 module ? g_module_name (module) : name,
		 g_module_error ());
      if (module)
	g_module_close (module);
    }
  
  return gtk_modules;
}

static GSList *
load_modules (const char *module_str)
{
  gchar **module_path = get_module_path ();
  gchar **module_names = pango_split_file_list (module_str);
  GSList *gtk_modules = NULL;
  gint i;
  
  for (i = 0; module_names[i]; i++)
    gtk_modules = load_module (gtk_modules, module_path, module_names[i]);
  
  gtk_modules = g_slist_reverse (gtk_modules);
  
  g_strfreev (module_names);
  g_strfreev (module_path);

  return gtk_modules;
}

static gboolean do_setlocale = TRUE;

/**
 * gtk_disable_setlocale:
 * 
 * Prevents gtk_init() and gtk_init_check() from automatically
 * calling setlocale (LC_ALL, ""). You would want to use this
 * function if you wanted to set the locale for your program
 * to something other than the user's locale, or if you wanted
 * to set different values for different locale categories.
 *
 * Most programs should not need to call this function.
 **/
static void
gtk_disable_setlocale (void)
{
  if (gtk_initialized)
    g_warning ("gtk_disable_setlocale() must be called before gtk_init()");
    
  do_setlocale = FALSE;
}

gboolean
gtk_init_check (int	 *argc,
		char   ***argv)
{
  GString *gtk_modules_string = NULL;
  GSList *gtk_modules = NULL;
  GSList *slist;
  gchar *env_string;

  if (gtk_initialized)
    return TRUE;

  if (!check_setugid ())
    return FALSE;
  
#if	0
  g_set_error_handler (gtk_error);
  g_set_warning_handler (gtk_warning);
  g_set_message_handler (gtk_message);
  g_set_print_handler (gtk_print);
#endif

  if (do_setlocale)
    setlocale (LC_ALL, "");
  
  /* Initialize "gdk". We pass along the 'argc' and 'argv'
   *  parameters as they contain information that GDK uses
   */
  if (!gdk_init_check (argc, argv))
    return FALSE;

  gdk_event_handler_set ((GdkEventFunc)gtk_main_do_event, NULL, NULL);
  
#ifdef G_ENABLE_DEBUG
  env_string = getenv ("GTK_DEBUG");
  if (env_string != NULL)
    {
      gtk_debug_flags = g_parse_debug_string (env_string,
					      gtk_debug_keys,
					      gtk_ndebug_keys);
      env_string = NULL;
    }
#endif	/* G_ENABLE_DEBUG */

  env_string = getenv ("GTK_MODULES");
  if (env_string)
    gtk_modules_string = g_string_new (env_string);

  if (argc && argv)
    {
      gint i, j, k;
      
      for (i = 1; i < *argc;)
	{
	  if (strcmp ("--gtk-module", (*argv)[i]) == 0 ||
	      strncmp ("--gtk-module=", (*argv)[i], 13) == 0)
	    {
	      gchar *module_name = (*argv)[i] + 12;
	      
	      if (*module_name == '=')
		module_name++;
	      else if (i + 1 < *argc)
		{
		  (*argv)[i] = NULL;
		  i += 1;
		  module_name = (*argv)[i];
		}
	      (*argv)[i] = NULL;

	      if (module_name && *module_name)
		{
		  if (gtk_modules_string)
		    g_string_append_c (gtk_modules_string, G_SEARCHPATH_SEPARATOR);
		  else
		    gtk_modules_string = g_string_new (NULL);

		  g_string_append (gtk_modules_string, module_name);
		}
	    }
	  else if (strcmp ("--g-fatal-warnings", (*argv)[i]) == 0)
	    {
	      GLogLevelFlags fatal_mask;
	      
	      fatal_mask = g_log_set_always_fatal (G_LOG_FATAL_MASK);
	      fatal_mask |= G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL;
              g_log_set_always_fatal (fatal_mask);
	      (*argv)[i] = NULL;
	    }
#ifdef G_ENABLE_DEBUG
	  else if ((strcmp ("--gtk-debug", (*argv)[i]) == 0) ||
		   (strncmp ("--gtk-debug=", (*argv)[i], 12) == 0))
	    {
	      gchar *equal_pos = strchr ((*argv)[i], '=');
	      
	      if (equal_pos != NULL)
		{
		  gtk_debug_flags |= g_parse_debug_string (equal_pos+1,
							   gtk_debug_keys,
							   gtk_ndebug_keys);
		}
	      else if ((i + 1) < *argc && (*argv)[i + 1])
		{
		  gtk_debug_flags |= g_parse_debug_string ((*argv)[i+1],
							   gtk_debug_keys,
							   gtk_ndebug_keys);
		  (*argv)[i] = NULL;
		  i += 1;
		}
	      (*argv)[i] = NULL;
	    }
	  else if ((strcmp ("--gtk-no-debug", (*argv)[i]) == 0) ||
		   (strncmp ("--gtk-no-debug=", (*argv)[i], 15) == 0))
	    {
	      gchar *equal_pos = strchr ((*argv)[i], '=');
	      
	      if (equal_pos != NULL)
		{
		  gtk_debug_flags &= ~g_parse_debug_string (equal_pos+1,
							    gtk_debug_keys,
							    gtk_ndebug_keys);
		}
	      else if ((i + 1) < *argc && (*argv)[i + 1])
		{
		  gtk_debug_flags &= ~g_parse_debug_string ((*argv)[i+1],
							    gtk_debug_keys,
							    gtk_ndebug_keys);
		  (*argv)[i] = NULL;
		  i += 1;
		}
	      (*argv)[i] = NULL;
	    }
#endif /* G_ENABLE_DEBUG */
	  i += 1;
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

  if (gtk_debug_flags & GTK_DEBUG_UPDATES)
    gdk_window_set_debug_updates (TRUE);

  /* load gtk modules */
  if (gtk_modules_string)
    {
      gtk_modules = load_modules (gtk_modules_string->str);
      g_string_free (gtk_modules_string, TRUE);
    }

#ifdef ENABLE_NLS
#  ifndef G_OS_WIN32
  bindtextdomain (GETTEXT_PACKAGE, GTK_LOCALEDIR);
#    ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#    endif
#  else /* !G_OS_WIN32 */
  {
    bindtextdomain (GETTEXT_PACKAGE,
		    g_win32_get_package_installation_subdirectory (GETTEXT_PACKAGE,
								   g_strdup_printf ("gtk-win32-%d.%d.dll", GTK_MAJOR_VERSION, GTK_MINOR_VERSION),
								   "locale"));
  }
#endif
#endif  

  {
  /* Translate to default:RTL if you want your widgets
   * to be RTL, otherwise translate to default:LTR.
   * Do *not* translate it to "predefinito:LTR", if it
   * it isn't default:LTR or default:RTL it will not work 
   */
    char *e = _("default:LTR");
    if (strcmp (e, "default:RTL")==0) {
      gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);
    } else if (strcmp (e, "default:LTR")) {
      g_warning ("Whoever translated default:LTR did so wrongly.\n");
    }
  }

  /* Initialize the default visual and colormap to be
   *  used in creating widgets. (We want to use the system
   *  defaults so as to be nice to the colormap).
   */
  gtk_visual = gdk_visual_get_system ();
  gtk_colormap = gdk_colormap_get_system ();

  gtk_type_init (0);
  _gtk_rc_init ();
  
  
  /* Register an exit function to make sure we are able to cleanup.
   */
  g_atexit (gtk_exit_func);
  
  /* Set the 'initialized' flag.
   */
  gtk_initialized = TRUE;

  /* initialize gtk modules
   */
  for (slist = gtk_modules; slist; slist = slist->next)
    {
      if (slist->data)
	{
	  GtkModuleInitFunc modinit;
	  
	  modinit = slist->data;
	  modinit (argc, argv);
	}
    }
  g_slist_free (gtk_modules);
  
#ifndef G_OS_WIN32
  /* No use warning on Win32, there aren't any non-devel versions anyhow... */
  g_message (""              "YOU ARE USING THE DEVEL BRANCH 1.3.x OF GTK+ WHICH IS CURRENTLY\n"
	     "                UNDER HEAVY DEVELOPMENT AND FREQUENTLY INTRODUCES INSTABILITIES.\n"
	     "                if you don't know why you are getting this, you probably want to\n"
	     "                use the stable branch which can be retrieved from\n"
	     "                ftp://ftp.gtk.org/pub/gtk/v1.2/ or via CVS with\n"
	     "                cvs checkout -r glib-1-2 glib; cvs checkout -r gtk-1-2 gtk+");
#endif

  return TRUE;
}

#undef gtk_init

void
gtk_init (int *argc, char ***argv)
{
  if (!gtk_init_check (argc, argv))
    {
      g_warning ("cannot open display: %s", gdk_get_display ());
      exit (1);
    }
}

#ifdef G_OS_WIN32

static void
check_sizeof_GtkWindow (size_t sizeof_GtkWindow)
{
  if (sizeof_GtkWindow != sizeof (GtkWindow))
    g_error ("Incompatible build!\n"
	     "The code using GTK+ thinks GtkWindow is of different\n"
             "size than it actually is in this build of GTK+.\n"
	     "On Windows, this probably means that you have compiled\n"
	     "your code with gcc without the -fnative-struct switch.");
}

/* These two functions might get more checks added later, thus pass
 * in the number of extra args.
 */
void
gtk_init_abi_check (int *argc, char ***argv, int num_checks, size_t sizeof_GtkWindow)
{
  check_sizeof_GtkWindow (sizeof_GtkWindow);
  gtk_init (argc, argv);
}

gboolean
gtk_init_check_abi_check (int *argc, char ***argv, int num_checks, size_t sizeof_GtkWindow)
{
  check_sizeof_GtkWindow (sizeof_GtkWindow);
  return gtk_init_check (argc, argv);
}

#endif

void
gtk_exit (gint errorcode)
{
  /* Only if "gtk" has been initialized should we de-initialize.
   */
  /* de-initialisation is done by the gtk_exit_funct(),
   * no need to do this here (Alex J.)
   */
  gdk_exit (errorcode);
}


/**
 * gtk_set_locale:
 *
 * Initializes internationalization support for GTK+. gtk_init()
 * automatically does this, so there is typically no point
 * in calling this function.
 *
 * If you are calling this function because you changed the locale
 * after GTK+ is was initialized, then calling this function
 * may help a bit. (Note, however, that changing the locale
 * after GTK+ is initialized may produce inconsistent results and
 * is not really supported.)
 * 
 * In detail - sets the current locale according to the
 * program environment. This is the same as calling the libc function
 * setlocale (LC_ALL, "") but also takes care of the locale specific
 * setup of the windowing system used by GDK.
 * 
 * Return value: a string corresponding to the locale set, as with the
 * C library function setlocale()
 **/
gchar*
gtk_set_locale (void)
{
  return gdk_set_locale ();
}

/**
 * gtk_get_default_language:
 *
 * Returns the ISO language code for the default language currently in
 * effect. (Note that this can change over the life of an
 * application.)  The default language is derived from the current
 * locale. It determines, for example, whether GTK+ uses the
 * right-to-left or left-to-right text direction.
 * 
 * Return value: the default language as an allocated string, must be freed
 **/
PangoLanguage *
gtk_get_default_language (void)
{
  gchar *lang;
  PangoLanguage *result;
  gchar *p;
  
  lang = g_strdup (setlocale (LC_CTYPE, NULL));
  p = strchr (lang, '.');
  if (p)
    *p = '\0';
  p = strchr (lang, '@');
  if (p)
    *p = '\0';

  result = pango_language_from_string (lang);
  g_free (lang);
  
  return result;
}

void
gtk_main (void)
{
  GList *tmp_list;
  GList *functions;
  GtkInitFunction *init;
  GMainLoop *loop;

  gtk_main_loop_level++;
  
  loop = g_main_new (TRUE);
  main_loops = g_slist_prepend (main_loops, loop);

  tmp_list = functions = init_functions;
  init_functions = NULL;
  
  while (tmp_list)
    {
      init = tmp_list->data;
      tmp_list = tmp_list->next;
      
      (* init->function) (init->data);
      g_free (init);
    }
  g_list_free (functions);

  if (g_main_is_running (main_loops->data))
    {
      GDK_THREADS_LEAVE ();
      g_main_run (loop);
      GDK_THREADS_ENTER ();
      gdk_flush ();
    }

  if (quit_functions)
    {
      GList *reinvoke_list = NULL;
      GtkQuitFunction *quitf;

      while (quit_functions)
	{
	  quitf = quit_functions->data;

	  tmp_list = quit_functions;
	  quit_functions = g_list_remove_link (quit_functions, quit_functions);
	  g_list_free_1 (tmp_list);

	  if ((quitf->main_level && quitf->main_level != gtk_main_loop_level) ||
	      gtk_quit_invoke_function (quitf))
	    {
	      reinvoke_list = g_list_prepend (reinvoke_list, quitf);
	    }
	  else
	    {
	      gtk_quit_destroy (quitf);
	    }
	}
      if (reinvoke_list)
	{
	  GList *work;
	  
	  work = g_list_last (reinvoke_list);
	  if (quit_functions)
	    quit_functions->prev = work;
	  work->next = quit_functions;
	  quit_functions = work;
	}

      gdk_flush ();
    }
	      
  main_loops = g_slist_remove (main_loops, loop);

  g_main_destroy (loop);

  gtk_main_loop_level--;
}

guint
gtk_main_level (void)
{
  return gtk_main_loop_level;
}

void
gtk_main_quit (void)
{
  g_return_if_fail (main_loops != NULL);

  g_main_quit (main_loops->data);
}

gint
gtk_events_pending (void)
{
  gboolean result;
  
  GDK_THREADS_LEAVE ();  
  result = g_main_pending ();
  GDK_THREADS_ENTER ();

  return result;
}

gboolean
gtk_main_iteration (void)
{
  GDK_THREADS_LEAVE ();
  g_main_iteration (TRUE);
  GDK_THREADS_ENTER ();

  if (main_loops)
    return !g_main_is_running (main_loops->data);
  else
    return TRUE;
}

gboolean
gtk_main_iteration_do (gboolean blocking)
{
  GDK_THREADS_LEAVE ();
  g_main_iteration (blocking);
  GDK_THREADS_ENTER ();

  if (main_loops)
    return !g_main_is_running (main_loops->data);
  else
    return TRUE;
}

void 
gtk_main_do_event (GdkEvent *event)
{
  GtkWidget *event_widget;
  GtkWidget *grab_widget;
  GtkWindowGroup *window_group;
  GdkEvent *next_event;
  GList *tmp_list;

  /* If there are any events pending then get the next one.
   */
  next_event = gdk_event_peek ();
  
  /* Try to compress enter/leave notify events. These event
   *  pairs occur when the mouse is dragged quickly across
   *  a window with many buttons (or through a menu). Instead
   *  of highlighting and de-highlighting each widget that
   *  is crossed it is better to simply de-highlight the widget
   *  which contained the mouse initially and highlight the
   *  widget which ends up containing the mouse.
   */
  if (next_event)
    if (((event->type == GDK_ENTER_NOTIFY) ||
	 (event->type == GDK_LEAVE_NOTIFY)) &&
	((next_event->type == GDK_ENTER_NOTIFY) ||
	 (next_event->type == GDK_LEAVE_NOTIFY)) &&
	(next_event->type != event->type) &&
	(next_event->any.window == event->any.window))
      {
	/* Throw both the peeked copy and the queued copy away 
	 */
	gdk_event_free (next_event);
	next_event = gdk_event_get ();
	gdk_event_free (next_event);
	
	return;
      }

  if (next_event)
    gdk_event_free (next_event);

  /* Find the widget which got the event. We store the widget
   *  in the user_data field of GdkWindow's.
   *  Ignore the event if we don't have a widget for it, except
   *  for GDK_PROPERTY_NOTIFY events which are handled specialy.
   *  Though this happens rarely, bogus events can occour
   *  for e.g. destroyed GdkWindows. 
   */
  event_widget = gtk_get_event_widget (event);
  if (!event_widget)
    {
      /* To handle selection INCR transactions, we select
       * PropertyNotify events on the requestor window and create
       * a corresponding (fake) GdkWindow so that events get
       * here. There won't be a widget though, so we have to handle
	   * them specially
	   */
      if (event->type == GDK_PROPERTY_NOTIFY)
	gtk_selection_incr_event (event->any.window,
				  &event->property);
      else if (event->type == GDK_SETTING)
	_gtk_settings_handle_event (&event->setting);

      return;
    }
  
  /* Push the event onto a stack of current events for
   * gtk_current_event_get().
   */
  current_events = g_list_prepend (current_events, event);

  window_group = gtk_main_get_window_group (event_widget);
  
  /* If there is a grab in effect...
   */
  if (window_group->grabs)
    {
      grab_widget = window_group->grabs->data;
      
      /* If the grab widget is an ancestor of the event widget
       *  then we send the event to the original event widget.
       *  This is the key to implementing modality.
       */
      if (GTK_WIDGET_IS_SENSITIVE (event_widget) &&
	  gtk_widget_is_ancestor (event_widget, grab_widget))
	grab_widget = event_widget;
    }
  else
    {
      grab_widget = event_widget;
    }

  /* Not all events get sent to the grabbing widget.
   * The delete, destroy, expose, focus change and resize
   *  events still get sent to the event widget because
   *  1) these events have no meaning for the grabbing widget
   *  and 2) redirecting these events to the grabbing widget
   *  could cause the display to be messed up.
   * 
   * Drag events are also not redirected, since it isn't
   *  clear what the semantics of that would be.
   */
  switch (event->type)
    {
    case GDK_NOTHING:
      break;
      
    case GDK_DELETE:
      gtk_widget_ref (event_widget);
      if ((!window_group->grabs || gtk_widget_get_toplevel (window_group->grabs->data) == event_widget) &&
	  !gtk_widget_event (event_widget, event))
	gtk_widget_destroy (event_widget);
      gtk_widget_unref (event_widget);
      break;
      
    case GDK_DESTROY:
      /* Unexpected GDK_DESTROY from the outside, ignore for
       * child windows, handle like a GDK_DELETE for toplevels
       */
      if (!event_widget->parent)
	{
	  gtk_widget_ref (event_widget);
	  if (!gtk_widget_event (event_widget, event) &&
	      GTK_WIDGET_REALIZED (event_widget))
	    gtk_widget_destroy (event_widget);
	  gtk_widget_unref (event_widget);
	}
      break;
      
    case GDK_EXPOSE:
      if (event->any.window && GTK_WIDGET_DOUBLE_BUFFERED (event_widget))
	{
	  gdk_window_begin_paint_region (event->any.window, event->expose.region);
	  gtk_widget_send_expose (event_widget, event);
	  gdk_window_end_paint (event->any.window);
	}
      else
	gtk_widget_send_expose (event_widget, event);
      break;

    case GDK_PROPERTY_NOTIFY:
    case GDK_NO_EXPOSE:
    case GDK_FOCUS_CHANGE:
    case GDK_CONFIGURE:
    case GDK_MAP:
    case GDK_UNMAP:
    case GDK_SELECTION_CLEAR:
    case GDK_SELECTION_REQUEST:
    case GDK_SELECTION_NOTIFY:
    case GDK_CLIENT_EVENT:
    case GDK_VISIBILITY_NOTIFY:
    case GDK_WINDOW_STATE:
      gtk_widget_event (event_widget, event);
      break;

    case GDK_SCROLL:
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
      gtk_propagate_event (grab_widget, event);
      break;

    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      if (key_snoopers)
	{
	  if (gtk_invoke_key_snoopers (grab_widget, event))
	    break;
	}
      /* else fall through */
    case GDK_MOTION_NOTIFY:
    case GDK_BUTTON_RELEASE:
    case GDK_PROXIMITY_IN:
    case GDK_PROXIMITY_OUT:
      gtk_propagate_event (grab_widget, event);
      break;
      
    case GDK_ENTER_NOTIFY:
      if (GTK_WIDGET_IS_SENSITIVE (grab_widget))
	{
	  gtk_widget_event (grab_widget, event);
	  if (event_widget == grab_widget)
	    GTK_PRIVATE_SET_FLAG (event_widget, GTK_LEAVE_PENDING);
	}
      break;
      
    case GDK_LEAVE_NOTIFY:
      if (GTK_WIDGET_LEAVE_PENDING (event_widget))
	{
	  GTK_PRIVATE_UNSET_FLAG (event_widget, GTK_LEAVE_PENDING);
	  gtk_widget_event (event_widget, event);
	}
      else if (GTK_WIDGET_IS_SENSITIVE (grab_widget))
	gtk_widget_event (grab_widget, event);
      break;
      
    case GDK_DRAG_STATUS:
    case GDK_DROP_FINISHED:
      _gtk_drag_source_handle_event (event_widget, event);
      break;
    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DROP_START:
      _gtk_drag_dest_handle_event (event_widget, event);
      break;
    default:
      g_assert_not_reached ();
      break;
    }
  
  tmp_list = current_events;
  current_events = g_list_remove_link (current_events, tmp_list);
  g_list_free_1 (tmp_list);
}

gboolean
gtk_true (void)
{
  return TRUE;
}

gboolean
gtk_false (void)
{
  return FALSE;
}

static GtkWindowGroup *
gtk_main_get_window_group (GtkWidget   *widget)
{
  GtkWidget *toplevel = NULL;

  if (widget)
    toplevel = gtk_widget_get_toplevel (widget);

  if (toplevel && GTK_IS_WINDOW (toplevel))
    return _gtk_window_get_group (GTK_WINDOW (toplevel));
  else
    return _gtk_window_get_group (NULL);
}

typedef struct
{
  gboolean was_grabbed;
  GtkWidget *grab_widget;
} GrabNotifyInfo;

static void
gtk_grab_notify_foreach (GtkWidget *child,
			 gpointer   data)
                        
{
  GrabNotifyInfo *info = data;

  if (child != info->grab_widget)
    {
      g_object_ref (G_OBJECT (child));

      gtk_signal_emit_by_name (GTK_OBJECT (child), "grab_notify", info->was_grabbed);

      if (GTK_IS_CONTAINER (child))
       gtk_container_foreach (GTK_CONTAINER (child), gtk_grab_notify_foreach, info);
      
      g_object_unref (G_OBJECT (child));
    }
}

static void
gtk_grab_notify (GtkWindowGroup *group,
		 GtkWidget      *grab_widget,
		 gboolean        was_grabbed)
{
  GList *toplevels;
  GrabNotifyInfo info;

  info.grab_widget = grab_widget;
  info.was_grabbed = was_grabbed;

  g_object_ref (group);
  g_object_ref (grab_widget);

  toplevels = gtk_window_list_toplevels ();
  g_list_foreach (toplevels, (GFunc)g_object_ref, NULL);
			    
  while (toplevels)
    {
      GtkWindow *toplevel = toplevels->data;
      toplevels = g_list_delete_link (toplevels, toplevels);

      if (group == toplevel->group)
	gtk_container_foreach (GTK_CONTAINER (toplevel), gtk_grab_notify_foreach, &info);
      g_object_unref (toplevel);
    }

  g_object_unref (group);
  g_object_unref (grab_widget);
}

void
gtk_grab_add (GtkWidget *widget)
{
  GtkWindowGroup *group;
  gboolean was_grabbed;
  
  g_return_if_fail (widget != NULL);
  
  if (!GTK_WIDGET_HAS_GRAB (widget) && GTK_WIDGET_IS_SENSITIVE (widget))
    {
      GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_GRAB);
      
      group = gtk_main_get_window_group (widget);

      was_grabbed = (group->grabs != NULL);
      
      gtk_widget_ref (widget);
      group->grabs = g_slist_prepend (group->grabs, widget);

      if (!was_grabbed)
	gtk_grab_notify (group, widget, FALSE);
    }
}

GtkWidget*
gtk_grab_get_current (void)
{
  GtkWindowGroup *group;

  group = gtk_main_get_window_group (NULL);

  if (group->grabs)
    return GTK_WIDGET (group->grabs->data);
  return NULL;
}

void
gtk_grab_remove (GtkWidget *widget)
{
  GtkWindowGroup *group;
  
  g_return_if_fail (widget != NULL);
  
  if (GTK_WIDGET_HAS_GRAB (widget))
    {
      GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_GRAB);

      group = gtk_main_get_window_group (widget);
      group->grabs = g_slist_remove (group->grabs, widget);
      
      gtk_widget_unref (widget);

      if (!group->grabs)
	gtk_grab_notify (group, widget, TRUE);
    }
}

void
gtk_init_add (GtkFunction function,
	      gpointer	  data)
{
  GtkInitFunction *init;
  
  init = g_new (GtkInitFunction, 1);
  init->function = function;
  init->data = data;
  
  init_functions = g_list_prepend (init_functions, init);
}

guint
gtk_key_snooper_install (GtkKeySnoopFunc snooper,
			 gpointer	 func_data)
{
  GtkKeySnooperData *data;
  static guint snooper_id = 1;

  g_return_val_if_fail (snooper != NULL, 0);

  data = g_new (GtkKeySnooperData, 1);
  data->func = snooper;
  data->func_data = func_data;
  data->id = snooper_id++;
  key_snoopers = g_slist_prepend (key_snoopers, data);

  return data->id;
}

void
gtk_key_snooper_remove (guint		 snooper_id)
{
  GtkKeySnooperData *data = NULL;
  GSList *slist;

  slist = key_snoopers;
  while (slist)
    {
      data = slist->data;
      if (data->id == snooper_id)
	break;

      slist = slist->next;
      data = NULL;
    }
  if (data)
    key_snoopers = g_slist_remove (key_snoopers, data);
}

static gint
gtk_invoke_key_snoopers (GtkWidget *grab_widget,
			 GdkEvent  *event)
{
  GSList *slist;
  gint return_val = FALSE;

  slist = key_snoopers;
  while (slist && !return_val)
    {
      GtkKeySnooperData *data;

      data = slist->data;
      slist = slist->next;
      return_val = (*data->func) (grab_widget, (GdkEventKey*) event, data->func_data);
    }

  return return_val;
}

guint
gtk_quit_add_full (guint		main_level,
		   GtkFunction		function,
		   GtkCallbackMarshal	marshal,
		   gpointer		data,
		   GtkDestroyNotify	destroy)
{
  static guint quit_id = 1;
  GtkQuitFunction *quitf;
  
  g_return_val_if_fail ((function != NULL) || (marshal != NULL), 0);

  if (!quit_mem_chunk)
    quit_mem_chunk = g_mem_chunk_new ("quit mem chunk", sizeof (GtkQuitFunction),
				      512, G_ALLOC_AND_FREE);
  
  quitf = g_chunk_new (GtkQuitFunction, quit_mem_chunk);
  
  quitf->id = quit_id++;
  quitf->main_level = main_level;
  quitf->function = function;
  quitf->marshal = marshal;
  quitf->data = data;
  quitf->destroy = destroy;

  quit_functions = g_list_prepend (quit_functions, quitf);
  
  return quitf->id;
}

static void
gtk_quit_destroy (GtkQuitFunction *quitf)
{
  if (quitf->destroy)
    quitf->destroy (quitf->data);
  g_mem_chunk_free (quit_mem_chunk, quitf);
}

static gint
gtk_quit_destructor (GtkObject **object_p)
{
  if (*object_p)
    gtk_object_destroy (*object_p);
  g_free (object_p);

  return FALSE;
}

void
gtk_quit_add_destroy (guint              main_level,
		      GtkObject         *object)
{
  GtkObject **object_p;

  g_return_if_fail (main_level > 0);
  g_return_if_fail (GTK_IS_OBJECT (object));

  object_p = g_new (GtkObject*, 1);
  *object_p = object;
  gtk_signal_connect (object,
		      "destroy",
		      GTK_SIGNAL_FUNC (gtk_widget_destroyed),
		      object_p);
  gtk_quit_add (main_level, (GtkFunction) gtk_quit_destructor, object_p);
}

guint
gtk_quit_add (guint	  main_level,
	      GtkFunction function,
	      gpointer	  data)
{
  return gtk_quit_add_full (main_level, function, NULL, data, NULL);
}

void
gtk_quit_remove (guint id)
{
  GtkQuitFunction *quitf;
  GList *tmp_list;
  
  tmp_list = quit_functions;
  while (tmp_list)
    {
      quitf = tmp_list->data;
      
      if (quitf->id == id)
	{
	  quit_functions = g_list_remove_link (quit_functions, tmp_list);
	  g_list_free (tmp_list);
	  gtk_quit_destroy (quitf);
	  
	  return;
	}
      
      tmp_list = tmp_list->next;
    }
}

void
gtk_quit_remove_by_data (gpointer data)
{
  GtkQuitFunction *quitf;
  GList *tmp_list;
  
  tmp_list = quit_functions;
  while (tmp_list)
    {
      quitf = tmp_list->data;
      
      if (quitf->data == data)
	{
	  quit_functions = g_list_remove_link (quit_functions, tmp_list);
	  g_list_free (tmp_list);
	  gtk_quit_destroy (quitf);

	  return;
	}
      
      tmp_list = tmp_list->next;
    }
}

guint
gtk_timeout_add_full (guint32		 interval,
		      GtkFunction	 function,
		      GtkCallbackMarshal marshal,
		      gpointer		 data,
		      GtkDestroyNotify	 destroy)
{
  if (marshal)
    {
      GtkClosure *closure;

      closure = g_new (GtkClosure, 1);
      closure->marshal = marshal;
      closure->data = data;
      closure->destroy = destroy;

      return g_timeout_add_full (0, interval, 
				 gtk_invoke_idle_timeout,
				 closure,
				 gtk_destroy_closure);
    }
  else
    return g_timeout_add_full (0, interval, function, data, destroy);
}

guint
gtk_timeout_add (guint32     interval,
		 GtkFunction function,
		 gpointer    data)
{
  return g_timeout_add_full (0, interval, function, data, NULL);
}

void
gtk_timeout_remove (guint tag)
{
  g_source_remove (tag);
}

guint
gtk_idle_add_full (gint			priority,
		   GtkFunction		function,
		   GtkCallbackMarshal	marshal,
		   gpointer		data,
		   GtkDestroyNotify	destroy)
{
  if (marshal)
    {
      GtkClosure *closure;

      closure = g_new (GtkClosure, 1);
      closure->marshal = marshal;
      closure->data = data;
      closure->destroy = destroy;

      return g_idle_add_full (priority,
			      gtk_invoke_idle_timeout,
			      closure,
			      gtk_destroy_closure);
    }
  else
    return g_idle_add_full (priority, function, data, destroy);
}

guint
gtk_idle_add (GtkFunction function,
	      gpointer	  data)
{
  return g_idle_add_full (GTK_PRIORITY_DEFAULT, function, data, NULL);
}

guint	    
gtk_idle_add_priority (gint        priority,
		       GtkFunction function,
		       gpointer	   data)
{
  return g_idle_add_full (priority, function, data, NULL);
}

void
gtk_idle_remove (guint tag)
{
  g_source_remove (tag);
}

void
gtk_idle_remove_by_data (gpointer data)
{
  if (!g_idle_remove_by_data (data))
    g_warning ("gtk_idle_remove_by_data(%p): no such idle", data);
}

guint
gtk_input_add_full (gint		source,
		    GdkInputCondition	condition,
		    GdkInputFunction	function,
		    GtkCallbackMarshal	marshal,
		    gpointer		data,
		    GtkDestroyNotify	destroy)
{
  if (marshal)
    {
      GtkClosure *closure;

      closure = g_new (GtkClosure, 1);
      closure->marshal = marshal;
      closure->data = data;
      closure->destroy = destroy;

      return gdk_input_add_full (source,
				 condition,
				 (GdkInputFunction) gtk_invoke_input,
				 closure,
				 (GdkDestroyNotify) gtk_destroy_closure);
    }
  else
    return gdk_input_add_full (source, condition, function, data, destroy);
}

void
gtk_input_remove (guint tag)
{
  g_source_remove (tag);
}

static void
gtk_destroy_closure (gpointer data)
{
  GtkClosure *closure = data;

  if (closure->destroy)
    (closure->destroy) (closure->data);
  g_free (closure);
}

static gboolean
gtk_invoke_idle_timeout (gpointer data)
{
  GtkClosure *closure = data;

  GtkArg args[1];
  gint ret_val = FALSE;
  args[0].name = NULL;
  args[0].type = GTK_TYPE_BOOL;
  args[0].d.pointer_data = &ret_val;
  closure->marshal (NULL, closure->data,  0, args);
  return ret_val;
}

static void
gtk_invoke_input (gpointer	    data,
		  gint		    source,
		  GdkInputCondition condition)
{
  GtkClosure *closure = data;

  GtkArg args[3];
  args[0].type = GTK_TYPE_INT;
  args[0].name = NULL;
  GTK_VALUE_INT (args[0]) = source;
  args[1].type = GDK_TYPE_INPUT_CONDITION;
  args[1].name = NULL;
  GTK_VALUE_FLAGS (args[1]) = condition;
  args[2].type = GTK_TYPE_NONE;
  args[2].name = NULL;

  closure->marshal (NULL, closure->data, 2, args);
}

/**
 * gtk_get_current_event:
 * 
 * Obtains a copy of the event currently being processed by GTK+.  For
 * example, if you get a "clicked" signal from #GtkButton, the current
 * event will be the #GdkEventButton that triggered the "clicked"
 * signal. The returned event must be freed with gdk_event_free().
 * If there is no current event, the function returns %NULL.
 * 
 * Return value: a copy of the current event, or %NULL if no current event.
 **/
GdkEvent*
gtk_get_current_event (void)
{
  if (current_events)
    return gdk_event_copy (current_events->data);
  else
    return NULL;
}

/**
 * gtk_get_current_event_time:
 * 
 * If there is a current event and it has a timestamp, return that
 * timestamp, otherwise return %GDK_CURRENT_TIME.
 * 
 * Return value: the timestamp from the current event, or %GDK_CURRENT_TIME.
 **/
guint32
gtk_get_current_event_time (void)
{
  if (current_events)
    return gdk_event_get_time (current_events->data);
  else
    return GDK_CURRENT_TIME;
}

/**
 * gtk_get_current_event_state:
 * @state: a location to store the state of the current event
 * 
 * If there is a current event and it has a state field, place
 * that state field in @state and return %TRUE, otherwise return
 * %FALSE.
 * 
 * Return value: %TRUE if there was a current event and it had a state field
 **/
gboolean
gtk_get_current_event_state (GdkModifierType *state)
{
  g_return_val_if_fail (state != NULL, FALSE);
  
  if (current_events)
    return gdk_event_get_state (current_events->data, state);
  else
    {
      *state = 0;
      return FALSE;
    }
}

/**
 * gtk_get_event_widget:
 * @event: a #GdkEvent
 *
 * If @event is %NULL or the event was not associated with any widget,
 * returns %NULL, otherwise returns the widget that received the event
 * originally.
 * 
 * Return value: the widget that originally received @event, or %NULL
 **/
GtkWidget*
gtk_get_event_widget (GdkEvent *event)
{
  GtkWidget *widget;

  widget = NULL;
  if (event && event->any.window)
    gdk_window_get_user_data (event->any.window, (void**) &widget);
  
  return widget;
}

static void
gtk_exit_func (void)
{
  if (gtk_initialized)
    {
      gtk_initialized = FALSE;
    }
}


static gint
gtk_quit_invoke_function (GtkQuitFunction *quitf)
{
  if (!quitf->marshal)
    return quitf->function (quitf->data);
  else
    {
      GtkArg args[1];
      gint ret_val = FALSE;

      args[0].name = NULL;
      args[0].type = GTK_TYPE_BOOL;
      args[0].d.pointer_data = &ret_val;
      ((GtkCallbackMarshal) quitf->marshal) (NULL,
					     quitf->data,
					     0, args);
      return ret_val;
    }
}

/**
 * gtk_propagate_event:
 * @widget: a #GtkWidget
 * @event: an event
 *
 * Sends an event to a widget, propagating the event to parent widgets
 * if the event remains unhandled. Events received by GTK+ from GDK
 * normally begin in gtk_main_do_event(). Depending on the type of
 * event, existence of modal dialogs, grabs, etc., the event may be
 * propagated; if so, this function is used. gtk_propagate_event()
 * calls gtk_widget_event() on each widget it decides to send the
 * event to.  So gtk_widget_event() is the lowest-level function; it
 * simply emits the "event" and possibly an event-specific signal on a
 * widget.  gtk_propagate_event() is a bit higher-level, and
 * gtk_main_do_event() is the highest level.
 *
 * All that said, you most likely don't want to use any of these
 * functions; synthesizing events is rarely needed. Consider asking on
 * the mailing list for better ways to achieve your goals. For
 * example, use gdk_window_invalidate_rect() or
 * gtk_widget_queue_draw() instead of making up expose events.
 * 
 **/
void
gtk_propagate_event (GtkWidget *widget,
		     GdkEvent  *event)
{
  gint handled_event;
  
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (event != NULL);
  
  handled_event = FALSE;

  gtk_widget_ref (widget);
      
  if ((event->type == GDK_KEY_PRESS) ||
      (event->type == GDK_KEY_RELEASE))
    {
      /* Only send key events within Window widgets to the Window
       *  The Window widget will in turn pass the
       *  key event on to the currently focused widget
       *  for that window.
       */
      GtkWidget *window;

      window = gtk_widget_get_toplevel (widget);
      if (window && GTK_IS_WINDOW (window))
	{
	  /* If there is a grab within the window, give the grab widget
	   * a first crack at the key event
	   */
	  if (widget != window && GTK_WIDGET_HAS_GRAB (widget))
	    handled_event = gtk_widget_event (widget, event);
	  
	  if (!handled_event)
	    {
	      window = gtk_widget_get_toplevel (widget);
	      if (window && GTK_IS_WINDOW (window))
		{
		  if (GTK_WIDGET_IS_SENSITIVE (window))
		    gtk_widget_event (window, event);
		}
	    }
		  
	  handled_event = TRUE; /* don't send to widget */
	}
    }
  
  /* Other events get propagated up the widget tree
   *  so that parents can see the button and motion
   *  events of the children.
   */
  if (!handled_event)
    {
      while (TRUE)
	{
	  GtkWidget *tmp;
	  
	  handled_event = !GTK_WIDGET_IS_SENSITIVE (widget) || gtk_widget_event (widget, event);
	  tmp = widget->parent;
	  gtk_widget_unref (widget);

	  widget = tmp;
	  
	  if (!handled_event && widget)
	    gtk_widget_ref (widget);
	  else
	    break;
	}
    }
  else
    gtk_widget_unref (widget);
}

#if 0
static void
gtk_error (gchar *str)
{
  gtk_print (str);
}

static void
gtk_warning (gchar *str)
{
  gtk_print (str);
}

static void
gtk_message (gchar *str)
{
  gtk_print (str);
}

static void
gtk_print (gchar *str)
{
  static GtkWidget *window = NULL;
  static GtkWidget *text;
  static int level = 0;
  GtkWidget *box1;
  GtkWidget *box2;
  GtkWidget *table;
  GtkWidget *hscrollbar;
  GtkWidget *vscrollbar;
  GtkWidget *separator;
  GtkWidget *button;
  
  if (level > 0)
    {
      fputs (str, stdout);
      fflush (stdout);
      return;
    }
  
  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      
      gtk_signal_connect (GTK_OBJECT (window), "destroy",
			  (GtkSignalFunc) gtk_widget_destroyed,
			  &window);
      
      gtk_window_set_title (GTK_WINDOW (window), "Messages");
      
      box1 = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), box1);
      gtk_widget_show (box1);
      
      
      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, TRUE, TRUE, 0);
      gtk_widget_show (box2);
      
      
      table = gtk_table_new (2, 2, FALSE);
      gtk_table_set_row_spacing (GTK_TABLE (table), 0, 2);
      gtk_table_set_col_spacing (GTK_TABLE (table), 0, 2);
      gtk_box_pack_start (GTK_BOX (box2), table, TRUE, TRUE, 0);
      gtk_widget_show (table);
      
      text = gtk_text_new (NULL, NULL);
      gtk_text_set_editable (GTK_TEXT (text), FALSE);
      gtk_table_attach_defaults (GTK_TABLE (table), text, 0, 1, 0, 1);
      gtk_widget_show (text);
      gtk_widget_realize (text);
      
      hscrollbar = gtk_hscrollbar_new (GTK_TEXT (text)->hadj);
      gtk_table_attach (GTK_TABLE (table), hscrollbar, 0, 1, 1, 2,
			GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
      gtk_widget_show (hscrollbar);
      
      vscrollbar = gtk_vscrollbar_new (GTK_TEXT (text)->vadj);
      gtk_table_attach (GTK_TABLE (table), vscrollbar, 1, 2, 0, 1,
			GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
      gtk_widget_show (vscrollbar);
      
      separator = gtk_hseparator_new ();
      gtk_box_pack_start (GTK_BOX (box1), separator, FALSE, TRUE, 0);
      gtk_widget_show (separator);
      
      
      box2 = gtk_vbox_new (FALSE, 10);
      gtk_container_set_border_width (GTK_CONTAINER (box2), 10);
      gtk_box_pack_start (GTK_BOX (box1), box2, FALSE, TRUE, 0);
      gtk_widget_show (box2);
      
      
      button = gtk_button_new_with_label ("close");
      gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				 (GtkSignalFunc) gtk_widget_hide,
				 GTK_OBJECT (window));
      gtk_box_pack_start (GTK_BOX (box2), button, TRUE, TRUE, 0);
      GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
      gtk_widget_grab_default (button);
      gtk_widget_show (button);
    }
  
  level += 1;
  gtk_text_insert (GTK_TEXT (text), NULL, NULL, NULL, str, -1);
  level -= 1;
  
  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show (window);
}
#endif

gboolean
_gtk_boolean_handled_accumulator (GSignalInvocationHint *ihint,
				  GValue                *return_accu,
				  const GValue          *handler_return,
				  gpointer               dummy)
{
  gboolean continue_emission;
  gboolean signal_handled;
  
  signal_handled = g_value_get_boolean (handler_return);
  g_value_set_boolean (return_accu, signal_handled);
  continue_emission = !signal_handled;
  
  return continue_emission;
}
