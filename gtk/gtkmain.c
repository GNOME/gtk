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

#include <config.h>

#include <glib.h>
#include "gdkconfig.h"

#include <locale.h>

#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
#include <libintl.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>		/* For uid_t, gid_t */

#ifdef G_OS_WIN32
#define STRICT
#include <windows.h>
#undef STRICT
#endif

#include <pango/pango-types.h>	/* For pango_language_from_string */

#include "gtkintl.h"

#include "gtkaccelmap.h"
#include "gtkbox.h"
#include "gtkclipboard.h"
#include "gtkdnd.h"
#include "gtkversion.h"
#include "gtkmain.h"
#include "gtkmodules.h"
#include "gtkrc.h"
#include "gtkselection.h"
#include "gtksettings.h"
#include "gtkwidget.h"
#include "gtkwindow.h"
#include "gtkprivate.h"
#include "gtkdebug.h"
#include "gtkalias.h"

#include "gdk/gdkprivate.h" /* for GDK_WINDOW_DESTROYED */

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
static gint pre_initialized = FALSE;
static gint gtk_initialized = FALSE;
static GList *current_events = NULL;

static GSList *main_loops = NULL;      /* stack of currently executing main loops */

static GList *init_functions = NULL;	   /* A list of init functions.
					    */
static GList *quit_functions = NULL;	   /* A list of quit functions.
					    */
static GSList *key_snoopers = NULL;

guint gtk_debug_flags = 0;		   /* Global GTK debug flag */

#ifdef G_ENABLE_DEBUG
static const GDebugKey gtk_debug_keys[] = {
  {"misc", GTK_DEBUG_MISC},
  {"plugsocket", GTK_DEBUG_PLUGSOCKET},
  {"text", GTK_DEBUG_TEXT},
  {"tree", GTK_DEBUG_TREE},
  {"updates", GTK_DEBUG_UPDATES},
  {"keybindings", GTK_DEBUG_KEYBINDINGS},
  {"multihead", GTK_DEBUG_MULTIHEAD},
  {"modules", GTK_DEBUG_MODULES},
  {"geometry", GTK_DEBUG_GEOMETRY},
  {"icontheme", GTK_DEBUG_ICONTHEME},
  {"printing", GTK_DEBUG_PRINTING}
};
#endif /* G_ENABLE_DEBUG */

/**
 * gtk_check_version:
 * @required_major: the required major version.
 * @required_minor: the required major version.
 * @required_micro: the required major version.
 * 
 * Checks that the GTK+ library in use is compatible with the
 * given version. Generally you would pass in the constants
 * #GTK_MAJOR_VERSION, #GTK_MINOR_VERSION, #GTK_MICRO_VERSION
 * as the three arguments to this function; that produces
 * a check that the library in use is compatible with
 * the version of GTK+ the application or module was compiled
 * against.
 *
 * Compatibility is defined by two things: first the version
 * of the running library is newer than the version
 * @required_major.required_minor.@required_micro. Second
 * the running library must be binary compatible with the
 * version @required_major.required_minor.@required_micro
 * (same major version.)
 *
 * This function is primarily for GTK+ modules; the module
 * can call this function to check that it wasn't loaded
 * into an incompatible version of GTK+. However, such a
 * a check isn't completely reliable, since the module may be
 * linked against an old version of GTK+ and calling the
 * old version of gtk_check_version(), but still get loaded
 * into an application using a newer version of GTK+.
 *
 * Return value: %NULL if the GTK+ library is compatible with the
 *   given version, or a string describing the version mismatch.
 *   The returned string is owned by GTK+ and should not be modified
 *   or freed.
 **/
gchar*
gtk_check_version (guint required_major,
		   guint required_minor,
		   guint required_micro)
{
  gint gtk_effective_micro = 100 * GTK_MINOR_VERSION + GTK_MICRO_VERSION;
  gint required_effective_micro = 100 * required_minor + required_micro;

  if (required_major > GTK_MAJOR_VERSION)
    return "Gtk+ version too old (major mismatch)";
  if (required_major < GTK_MAJOR_VERSION)
    return "Gtk+ version too new (major mismatch)";
  if (required_effective_micro < gtk_effective_micro - GTK_BINARY_AGE)
    return "Gtk+ version too new (micro mismatch)";
  if (required_effective_micro > gtk_effective_micro)
    return "Gtk+ version too old (micro mismatch)";
  return NULL;
}

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

#ifdef G_OS_WIN32

G_WIN32_DLLMAIN_FOR_DLL_NAME(static, dll_name)

const gchar *
_gtk_get_datadir (void)
{
  static char *gtk_datadir = NULL;
  if (gtk_datadir == NULL)
    gtk_datadir = g_win32_get_package_installation_subdirectory
      (GETTEXT_PACKAGE, dll_name, "share");

  return gtk_datadir;
}

const gchar *
_gtk_get_libdir (void)
{
  static char *gtk_libdir = NULL;
  if (gtk_libdir == NULL)
    gtk_libdir = g_win32_get_package_installation_subdirectory
      (GETTEXT_PACKAGE, dll_name, "lib");

  return gtk_libdir;
}

const gchar *
_gtk_get_localedir (void)
{
  static char *gtk_localedir = NULL;
  if (gtk_localedir == NULL)
    {
      gchar *temp;
      
      temp = g_win32_get_package_installation_subdirectory
        (GETTEXT_PACKAGE, dll_name, "lib\\locale");

      /* gtk_localedir is passed to bindtextdomain() which isn't
       * UTF-8-aware.
       */
      gtk_localedir = g_win32_locale_filename_from_utf8 (temp);
      g_free (temp);
    }
  return gtk_localedir;
}

const gchar *
_gtk_get_sysconfdir (void)
{
  static char *gtk_sysconfdir = NULL;
  if (gtk_sysconfdir == NULL)
    gtk_sysconfdir = g_win32_get_package_installation_subdirectory
      (GETTEXT_PACKAGE, dll_name, "etc");

  return gtk_sysconfdir;
}

const gchar *
_gtk_get_data_prefix (void)
{
  static char *gtk_data_prefix = NULL;
  if (gtk_data_prefix == NULL)
    gtk_data_prefix = g_win32_get_package_installation_directory
      (GETTEXT_PACKAGE, dll_name);

  return gtk_data_prefix;
}

#endif /* G_OS_WIN32 */

static gboolean do_setlocale = TRUE;

/**
 * gtk_disable_setlocale:
 * 
 * Prevents gtk_init(), gtk_init_check(), gtk_init_with_args() and
 * gtk_parse_args() from automatically
 * calling <literal>setlocale (LC_ALL, "")</literal>. You would 
 * want to use this function if you wanted to set the locale for 
 * your program to something other than the user's locale, or if 
 * you wanted to set different values for different locale categories.
 *
 * Most programs should not need to call this function.
 **/
void
gtk_disable_setlocale (void)
{
  if (pre_initialized)
    g_warning ("gtk_disable_setlocale() must be called before gtk_init()");
    
  do_setlocale = FALSE;
}

#ifdef G_PLATFORM_WIN32
#undef gtk_init_check
#endif

static GString *gtk_modules_string = NULL;
static gboolean g_fatal_warnings = FALSE;

#ifdef G_ENABLE_DEBUG
static gboolean
gtk_arg_debug_cb (const char *key, const char *value, gpointer user_data)
{
  gtk_debug_flags |= g_parse_debug_string (value,
					   gtk_debug_keys,
					   G_N_ELEMENTS (gtk_debug_keys));

  return TRUE;
}

static gboolean
gtk_arg_no_debug_cb (const char *key, const char *value, gpointer user_data)
{
  gtk_debug_flags &= ~g_parse_debug_string (value,
					    gtk_debug_keys,
					    G_N_ELEMENTS (gtk_debug_keys));

  return TRUE;
}
#endif /* G_ENABLE_DEBUG */

static gboolean
gtk_arg_module_cb (const char *key, const char *value, gpointer user_data)
{
  if (value && *value)
    {
      if (gtk_modules_string)
	g_string_append_c (gtk_modules_string, G_SEARCHPATH_SEPARATOR);
      else
	gtk_modules_string = g_string_new (NULL);
      
      g_string_append (gtk_modules_string, value);
    }

  return TRUE;
}

static const GOptionEntry gtk_args[] = {
  { "gtk-module",       0, 0, G_OPTION_ARG_CALLBACK, gtk_arg_module_cb,   
    /* Description of --gtk-module=MODULES in --help output */ N_("Load additional GTK+ modules"), 
    /* Placeholder in --gtk-module=MODULES in --help output */ N_("MODULES") },
  { "g-fatal-warnings", 0, 0, G_OPTION_ARG_NONE, &g_fatal_warnings, 
    /* Description of --g-fatal-warnings in --help output */   N_("Make all warnings fatal"), NULL },
#ifdef G_ENABLE_DEBUG
  { "gtk-debug",        0, 0, G_OPTION_ARG_CALLBACK, gtk_arg_debug_cb,    
    /* Description of --gtk-debug=FLAGS in --help output */    N_("GTK+ debugging flags to set"), 
    /* Placeholder in --gtk-debug=FLAGS in --help output */    N_("FLAGS") },
  { "gtk-no-debug",     0, 0, G_OPTION_ARG_CALLBACK, gtk_arg_no_debug_cb, 
    /* Description of --gtk-no-debug=FLAGS in --help output */ N_("GTK+ debugging flags to unset"), 
    /* Placeholder in --gtk-no-debug=FLAGS in --help output */ N_("FLAGS") },
#endif 
  { NULL }
};

#ifdef G_OS_WIN32

static char *iso639_to_check = NULL;
static char *iso3166_to_check = NULL;
static char *script_to_check = NULL;
static gboolean setlocale_called = FALSE;

static BOOL CALLBACK
enum_locale_proc (LPTSTR locale)
{
  LCID lcid;
  char iso639[10];
  char iso3166[10];
  char *endptr;


  lcid = strtoul (locale, &endptr, 16);
  if (*endptr == '\0' &&
      GetLocaleInfo (lcid, LOCALE_SISO639LANGNAME, iso639, sizeof (iso639)) &&
      GetLocaleInfo (lcid, LOCALE_SISO3166CTRYNAME, iso3166, sizeof (iso3166)))
    {
      if (strcmp (iso639, iso639_to_check) == 0 &&
	  ((iso3166_to_check != NULL &&
	    strcmp (iso3166, iso3166_to_check) == 0) ||
	   (iso3166_to_check == NULL &&
	    SUBLANGID (LANGIDFROMLCID (lcid)) == SUBLANG_DEFAULT)))
	{
	  char language[100], country[100];
	  char locale[300];

	  if (script_to_check != NULL)
	    {
	      /* If lcid is the "other" script for this language,
	       * return TRUE, i.e. continue looking.
	       */
	      if (strcmp (script_to_check, "Latn") == 0)
		{
		  switch (LANGIDFROMLCID (lcid))
		    {
		    case MAKELANGID (LANG_AZERI, SUBLANG_AZERI_CYRILLIC):
		      return TRUE;
		    case MAKELANGID (LANG_UZBEK, SUBLANG_UZBEK_CYRILLIC):
		      return TRUE;
		    case MAKELANGID (LANG_SERBIAN, SUBLANG_SERBIAN_CYRILLIC):
		      return TRUE;
		    case MAKELANGID (LANG_SERBIAN, 0x07):
		      /* Serbian in Bosnia and Herzegovina, Cyrillic */
		      return TRUE;
		    }
		}
	      else if (strcmp (script_to_check, "Cyrl") == 0)
		{
		  switch (LANGIDFROMLCID (lcid))
		    {
		    case MAKELANGID (LANG_AZERI, SUBLANG_AZERI_LATIN):
		      return TRUE;
		    case MAKELANGID (LANG_UZBEK, SUBLANG_UZBEK_LATIN):
		      return TRUE;
		    case MAKELANGID (LANG_SERBIAN, SUBLANG_SERBIAN_LATIN):
		      return TRUE;
		    case MAKELANGID (LANG_SERBIAN, 0x06):
		      /* Serbian in Bosnia and Herzegovina, Latin */
		      return TRUE;
		    }
		}
	    }

	  SetThreadLocale (lcid);

	  if (GetLocaleInfo (lcid, LOCALE_SENGLANGUAGE, language, sizeof (language)) &&
	      GetLocaleInfo (lcid, LOCALE_SENGCOUNTRY, country, sizeof (country)))
	    {
	      strcpy (locale, language);
	      strcat (locale, "_");
	      strcat (locale, country);

	      if (setlocale (LC_ALL, locale) != NULL)
		setlocale_called = TRUE;
	    }

	  return FALSE;
	}
    }

  return TRUE;
}
  
#endif

static void
do_pre_parse_initialization (int    *argc,
			     char ***argv)
{
  const gchar *env_string;
  
#if	0
  g_set_error_handler (gtk_error);
  g_set_warning_handler (gtk_warning);
  g_set_message_handler (gtk_message);
  g_set_print_handler (gtk_print);
#endif

  if (pre_initialized)
    return;

  pre_initialized = TRUE;

  if (do_setlocale)
    {
#ifdef G_OS_WIN32
      /* If some of the POSIXish environment variables are set, set
       * the Win32 thread locale correspondingly.
       */ 
      char *p = getenv ("LC_ALL");
      if (p == NULL)
	p = getenv ("LANG");

      if (p != NULL)
	{
	  p = g_strdup (p);
	  if (strcmp (p, "C") == 0)
	    SetThreadLocale (LOCALE_SYSTEM_DEFAULT);
	  else
	    {
	      /* Check if one of the supported locales match the
	       * environment variable. If so, use that locale.
	       */
	      iso639_to_check = p;
	      iso3166_to_check = strchr (iso639_to_check, '_');
	      if (iso3166_to_check != NULL)
		{
		  *iso3166_to_check++ = '\0';

		  script_to_check = strchr (iso3166_to_check, '@');
		  if (script_to_check != NULL)
		    *script_to_check++ = '\0';

		  /* Handle special cases. */
		  
		  /* The standard code for Serbia and Montenegro was
		   * "CS", but MSFT uses for some reason "SP". By now
		   * (October 2006), SP has split into two, "RS" and
		   * "ME", but don't bother trying to handle those
		   * yet. Do handle the even older "YU", though.
		   */
		  if (strcmp (iso3166_to_check, "CS") == 0 ||
		      strcmp (iso3166_to_check, "YU") == 0)
		    iso3166_to_check = "SP";
		}
	      else
		{
		  script_to_check = strchr (iso639_to_check, '@');
		  if (script_to_check != NULL)
		    *script_to_check++ = '\0';
		  /* LANG_SERBIAN == LANG_CROATIAN, recognize just "sr" */
		  if (strcmp (iso639_to_check, "sr") == 0)
		    iso3166_to_check = "SP";
		}

	      EnumSystemLocales (enum_locale_proc, LCID_SUPPORTED);
	    }
	  g_free (p);
	}
      if (!setlocale_called)
	setlocale (LC_ALL, "");
#else
      if (!setlocale (LC_ALL, ""))
	g_warning ("Locale not supported by C library.\n\tUsing the fallback 'C' locale.");
#endif
    }

  gdk_pre_parse_libgtk_only ();
  gdk_event_handler_set ((GdkEventFunc)gtk_main_do_event, NULL, NULL);
  
#ifdef G_ENABLE_DEBUG
  env_string = g_getenv ("GTK_DEBUG");
  if (env_string != NULL)
    {
      gtk_debug_flags = g_parse_debug_string (env_string,
					      gtk_debug_keys,
					      G_N_ELEMENTS (gtk_debug_keys));
      env_string = NULL;
    }
#endif	/* G_ENABLE_DEBUG */

  env_string = g_getenv ("GTK_MODULES");
  if (env_string)
    gtk_modules_string = g_string_new (env_string);
}

static void
gettext_initialization (void)
{
#ifdef ENABLE_NLS
  bindtextdomain (GETTEXT_PACKAGE, GTK_LOCALEDIR);
  bindtextdomain (GETTEXT_PACKAGE "-properties", GTK_LOCALEDIR);
#    ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  bind_textdomain_codeset (GETTEXT_PACKAGE "-properties", "UTF-8");
#    endif
#endif  
}

static void
do_post_parse_initialization (int    *argc,
			      char ***argv)
{
  if (gtk_initialized)
    return;

  gettext_initialization ();

  if (g_fatal_warnings)
    {
      GLogLevelFlags fatal_mask;

      fatal_mask = g_log_set_always_fatal (G_LOG_FATAL_MASK);
      fatal_mask |= G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL;
      g_log_set_always_fatal (fatal_mask);
    }

  if (gtk_debug_flags & GTK_DEBUG_UPDATES)
    gdk_window_set_debug_updates (TRUE);

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

  gtk_type_init (0);
  _gtk_accel_map_init ();  
  _gtk_rc_init ();

  /* Set the 'initialized' flag.
   */
  gtk_initialized = TRUE;

  /* load gtk modules */
  if (gtk_modules_string)
    {
      _gtk_modules_init (argc, argv, gtk_modules_string->str);
      g_string_free (gtk_modules_string, TRUE);
    }
}


typedef struct
{
  gboolean open_default_display;
} OptionGroupInfo;

static gboolean
pre_parse_hook (GOptionContext *context,
		GOptionGroup   *group,
		gpointer	data,
		GError        **error)
{
  do_pre_parse_initialization (NULL, NULL);
  
  return TRUE;
}

static gboolean
post_parse_hook (GOptionContext *context,
		 GOptionGroup   *group,
		 gpointer	data,
		 GError        **error)
{
  OptionGroupInfo *info = data;

  
  do_post_parse_initialization (NULL, NULL);
  
  if (info->open_default_display)
    {
      if (gdk_display_open_default_libgtk_only () == NULL)
	{
	  const char *display_name = gdk_get_display_arg_name ();
	  g_set_error (error, 
		       G_OPTION_ERROR, 
		       G_OPTION_ERROR_FAILED,
		       "cannot open display: %s",
		       display_name ? display_name : "" );
	  
	  return FALSE;
	}
    }

  return TRUE;
}


/**
 * gtk_get_option_group:
 * @open_default_display: whether to open the default display 
 *    when parsing the commandline arguments
 * 
 * Returns a #GOptionGroup for the commandline arguments recognized
 * by GTK+ and GDK. You should add this group to your #GOptionContext 
 * with g_option_context_add_group(), if you are using 
 * g_option_context_parse() to parse your commandline arguments.
 *
 * Returns a #GOptionGroup for the commandline arguments recognized
 *   by GTK+
 *
 * Since: 2.6
 */
GOptionGroup *
gtk_get_option_group (gboolean open_default_display)
{
  GOptionGroup *group;
  OptionGroupInfo *info;

  gettext_initialization ();

  info = g_new0 (OptionGroupInfo, 1);
  info->open_default_display = open_default_display;
  
  group = g_option_group_new ("gtk", _("GTK+ Options"), _("Show GTK+ Options"), info, g_free);
  g_option_group_set_parse_hooks (group, pre_parse_hook, post_parse_hook);

  gdk_add_option_entries_libgtk_only (group);
  g_option_group_add_entries (group, gtk_args);
  g_option_group_set_translation_domain (group, GETTEXT_PACKAGE);
  
  return group;
}

/**
 * gtk_init_with_args:
 * @argc: a pointer to the number of command line arguments.
 * @argv: a pointer to the array of command line arguments.
 * @parameter_string: a string which is displayed in
 *    the first line of <option>--help</option> output, after 
 *    <literal><replaceable>programname</replaceable> [OPTION...]</literal>
 * @entries: a %NULL-terminated array of #GOptionEntry<!-- -->s
 *    describing the options of your program
 * @translation_domain: a translation domain to use for translating
 *    the <option>--help</option> output for the options in @entries
 *    with gettext(), or %NULL
 * @error: a return location for errors 
 *
 * This function does the same work as gtk_init_check(). 
 * Additionally, it allows you to add your own commandline options, 
 * and it automatically generates nicely formatted 
 * <option>--help</option> output. Note that your program will
 * be terminated after writing out the help output.
 *
 * Returns: %TRUE if the GUI has been successfully initialized, 
 *               %FALSE otherwise.
 * 
 * Since: 2.6
 */
gboolean
gtk_init_with_args (int            *argc,
		    char         ***argv,
		    char           *parameter_string,  
		    GOptionEntry   *entries,
		    char           *translation_domain,
		    GError        **error)
{
  GOptionContext *context;
  GOptionGroup *gtk_group;
  gboolean retval;

  if (gtk_initialized)
    return TRUE;

  gettext_initialization ();

  if (!check_setugid ())
    return FALSE;

  gtk_group = gtk_get_option_group (TRUE);
  
  context = g_option_context_new (parameter_string);
  g_option_context_add_group (context, gtk_group);
  
  if (entries)
    g_option_context_add_main_entries (context, entries, translation_domain);
  retval = g_option_context_parse (context, argc, argv, error);
  
  g_option_context_free (context);

  return retval;
}


/**
 * gtk_parse_args:
 * @argc: a pointer to the number of command line arguments.
 * @argv: a pointer to the array of command line arguments.
 * 
 * Parses command line arguments, and initializes global
 * attributes of GTK+, but does not actually open a connection
 * to a display. (See gdk_display_open(), gdk_get_display_arg_name())
 *
 * Any arguments used by GTK+ or GDK are removed from the array and
 * @argc and @argv are updated accordingly.
 *
 * You shouldn't call this function explicitely if you are using
 * gtk_init(), or gtk_init_check().
 *
 * Return value: %TRUE if initialization succeeded, otherwise %FALSE.
 **/
gboolean
gtk_parse_args (int    *argc,
		char ***argv)
{
  GOptionContext *option_context;
  GOptionGroup *gtk_group;
  GError *error = NULL;
  
  if (gtk_initialized)
    return TRUE;

  gettext_initialization ();

  if (!check_setugid ())
    return FALSE;

  option_context = g_option_context_new (NULL);
  g_option_context_set_ignore_unknown_options (option_context, TRUE);
  g_option_context_set_help_enabled (option_context, FALSE);
  gtk_group = gtk_get_option_group (FALSE);
  g_option_context_set_main_group (option_context, gtk_group);
  if (!g_option_context_parse (option_context, argc, argv, &error))
    {
      g_warning ("%s", error->message);
      g_error_free (error);
    }

  g_option_context_free (option_context);

  return TRUE;
}

#ifdef G_PLATFORM_WIN32
#undef gtk_init_check
#endif

/**
 * gtk_init_check:
 * @argc: Address of the <parameter>argc</parameter> parameter of your 
 *   main() function. Changed if any arguments were handled.
 * @argv: Address of the <parameter>argv</parameter> parameter of main(). 
 *   Any parameters understood by gtk_init() are stripped before return.
 * 
 * This function does the same work as gtk_init() with only 
 * a single change: It does not terminate the program if the GUI can't be 
 * initialized. Instead it returns %FALSE on failure.
 *
 * This way the application can fall back to some other means of communication 
 * with the user - for example a curses or command line interface.
 * 
 * Return value: %TRUE if the GUI has been successfully initialized, 
 *               %FALSE otherwise.
 **/
gboolean
gtk_init_check (int	 *argc,
		char   ***argv)
{
  if (!gtk_parse_args (argc, argv))
    return FALSE;

  return gdk_display_open_default_libgtk_only () != NULL;
}

#ifdef G_PLATFORM_WIN32
#undef gtk_init
#endif

/**
 * gtk_init:
 * @argc: Address of the <parameter>argc</parameter> parameter of your 
 *   main() function. Changed if any arguments were handled.
 * @argv: Address of the <parameter>argv</parameter> parameter of main(). 
 *   Any parameters understood by gtk_init() are stripped before return.
 * 
 * Call this function before using any other GTK+ functions in your GUI
 * applications.  It will initialize everything needed to operate the 
 * toolkit and parses some standard command line options. @argc and 
 * @argv are adjusted accordingly so your own code will 
 * never see those standard arguments. 
 *
 * Note that there are some alternative ways to initialize GTK+: 
 * if you are calling gtk_parse_args(), gtk_init_check(), 
 * gtk_init_with_args() or g_option_context_parse() with 
 * the option group returned by gtk_get_option_group(), you 
 * <emphasis>don't</emphasis> have to call gtk_init().
 *
 * <note><para>
 * This function will terminate your program if it was unable to initialize 
 * the GUI for some reason. If you want your program to fall back to a 
 * textual interface you want to call gtk_init_check() instead.
 * </para></note>
 **/
void
gtk_init (int *argc, char ***argv)
{
  if (!gtk_init_check (argc, argv))
    {
      const char *display_name_arg = gdk_get_display_arg_name ();
      g_warning ("cannot open display: %s", display_name_arg ? display_name_arg : " ");
      exit (1);
    }
}

#ifdef G_PLATFORM_WIN32

static void
check_sizeof_GtkWindow (size_t sizeof_GtkWindow)
{
  if (sizeof_GtkWindow != sizeof (GtkWindow))
    g_error ("Incompatible build!\n"
	     "The code using GTK+ thinks GtkWindow is of different\n"
             "size than it actually is in this build of GTK+.\n"
	     "On Windows, this probably means that you have compiled\n"
	     "your code with gcc without the -mms-bitfields switch,\n"
	     "or that you are using an unsupported compiler.");
}

/* In GTK+ 2.0 the GtkWindow struct actually is the same size in
 * gcc-compiled code on Win32 whether compiled with -fnative-struct or
 * not. Unfortunately this wan't noticed until after GTK+ 2.0.1. So,
 * from GTK+ 2.0.2 on, check some other struct, too, where the use of
 * -fnative-struct still matters. GtkBox is one such.
 */
static void
check_sizeof_GtkBox (size_t sizeof_GtkBox)
{
  if (sizeof_GtkBox != sizeof (GtkBox))
    g_error ("Incompatible build!\n"
	     "The code using GTK+ thinks GtkBox is of different\n"
             "size than it actually is in this build of GTK+.\n"
	     "On Windows, this probably means that you have compiled\n"
	     "your code with gcc without the -mms-bitfields switch,\n"
	     "or that you are using an unsupported compiler.");
}

/* These two functions might get more checks added later, thus pass
 * in the number of extra args.
 */
void
gtk_init_abi_check (int *argc, char ***argv, int num_checks, size_t sizeof_GtkWindow, size_t sizeof_GtkBox)
{
  check_sizeof_GtkWindow (sizeof_GtkWindow);
  if (num_checks >= 2)
    check_sizeof_GtkBox (sizeof_GtkBox);
  gtk_init (argc, argv);
}

gboolean
gtk_init_check_abi_check (int *argc, char ***argv, int num_checks, size_t sizeof_GtkWindow, size_t sizeof_GtkBox)
{
  check_sizeof_GtkWindow (sizeof_GtkWindow);
  if (num_checks >= 2)
    check_sizeof_GtkBox (sizeof_GtkBox);
  return gtk_init_check (argc, argv);
}

#endif

void
gtk_exit (gint errorcode)
{
  exit (errorcode);
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
 * program environment. This is the same as calling the C library function
 * <literal>setlocale (LC_ALL, "")</literal> but also takes care of the 
 * locale specific setup of the windowing system used by GDK.
 * 
 * Returns: a string corresponding to the locale set, typically in the
 * form lang_COUNTRY, where lang is an ISO-639 language code, and
 * COUNTRY is an ISO-3166 country code. On Unix, this form matches the
 * result of the setlocale(); it is also used on other machines, such as 
 * Windows, where the C library returns a different result. The string is 
 * owned by GTK+ and should not be modified or freed.
 **/
gchar *
gtk_set_locale (void)
{
  return gdk_set_locale ();
}

/**
 * _gtk_get_lc_ctype:
 *
 * Return the Unix-style locale string for the language currently in
 * effect. On Unix systems, this is the return value from
 * <literal>setlocale(LC_CTYPE, NULL)</literal>, and the user can
 * affect this through the environment variables LC_ALL, LC_CTYPE or
 * LANG (checked in that order). The locale strings typically is in
 * the form lang_COUNTRY, where lang is an ISO-639 language code, and
 * COUNTRY is an ISO-3166 country code. For instance, sv_FI for
 * Swedish as written in Finland or pt_BR for Portuguese as written in
 * Brazil.
 * 
 * On Windows, the C library doesn't use any such environment
 * variables, and setting them won't affect the behaviour of functions
 * like ctime(). The user sets the locale through the Regional Options 
 * in the Control Panel. The C library (in the setlocale() function) 
 * does not use country and language codes, but country and language 
 * names spelled out in English. 
 * However, this function does check the above environment
 * variables, and does return a Unix-style locale string based on
 * either said environment variables or the thread's current locale.
 *
 * Return value: a dynamically allocated string, free with g_free().
 */

gchar *
_gtk_get_lc_ctype (void)
{
#ifdef G_OS_WIN32
  /* Somebody might try to set the locale for this process using the
   * LANG or LC_ environment variables. The Microsoft C library
   * doesn't know anything about them. You set the locale in the
   * Control Panel. Setting these env vars won't have any affect on
   * locale-dependent C library functions like ctime(). But just for
   * kicks, do obey LC_ALL, LC_CTYPE and LANG in GTK. (This also makes
   * it easier to test GTK and Pango in various default languages, you
   * don't have to clickety-click in the Control Panel, you can simply
   * start the program with LC_ALL=something on the command line.)
   */
  gchar *p;

  p = getenv ("LC_ALL");
  if (p != NULL)
    return g_strdup (p);

  p = getenv ("LC_CTYPE");
  if (p != NULL)
    return g_strdup (p);

  p = getenv ("LANG");
  if (p != NULL)
    return g_strdup (p);

  return g_win32_getlocale ();
#else
  return g_strdup (setlocale (LC_CTYPE, NULL));
#endif
}

/**
 * gtk_get_default_language:
 *
 * Returns the #PangoLanguage for the default language currently in
 * effect. (Note that this can change over the life of an
 * application.)  The default language is derived from the current
 * locale. It determines, for example, whether GTK+ uses the
 * right-to-left or left-to-right text direction. See
 * _gtk_get_lc_ctype() for notes on behaviour on Windows.
 * 
 * Return value: the default language as a #PangoLanguage, must not be
 * freed
 **/
PangoLanguage *
gtk_get_default_language (void)
{
  gchar *lang;
  PangoLanguage *result;
  gchar *p;
  
  lang = _gtk_get_lc_ctype ();
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
  
  loop = g_main_loop_new (NULL, TRUE);
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

  if (g_main_loop_is_running (main_loops->data))
    {
      GDK_THREADS_LEAVE ();
      g_main_loop_run (loop);
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

  g_main_loop_unref (loop);

  gtk_main_loop_level--;

  /* Try storing all clipboard data we have */
  if (gtk_main_loop_level == 0)
    _gtk_clipboard_store_all ();
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

  g_main_loop_quit (main_loops->data);
}

gboolean
gtk_events_pending (void)
{
  gboolean result;
  
  GDK_THREADS_LEAVE ();  
  result = g_main_context_pending (NULL);
  GDK_THREADS_ENTER ();

  return result;
}

gboolean
gtk_main_iteration (void)
{
  GDK_THREADS_LEAVE ();
  g_main_context_iteration (NULL, TRUE);
  GDK_THREADS_ENTER ();

  if (main_loops)
    return !g_main_loop_is_running (main_loops->data);
  else
    return TRUE;
}

gboolean
gtk_main_iteration_do (gboolean blocking)
{
  GDK_THREADS_LEAVE ();
  g_main_context_iteration (NULL, blocking);
  GDK_THREADS_ENTER ();

  if (main_loops)
    return !g_main_loop_is_running (main_loops->data);
  else
    return TRUE;
}

/* private libgtk to libgdk interfaces
 */
gboolean gdk_pointer_grab_info_libgtk_only  (GdkDisplay *display,
					     GdkWindow **grab_window,
					     gboolean   *owner_events);
gboolean gdk_keyboard_grab_info_libgtk_only (GdkDisplay *display,
					     GdkWindow **grab_window,
					     gboolean   *owner_events);

static void
rewrite_events_translate (GdkWindow *old_window,
			  GdkWindow *new_window,
			  gdouble   *x,
			  gdouble   *y)
{
  gint old_origin_x, old_origin_y;
  gint new_origin_x, new_origin_y;

  gdk_window_get_origin	(old_window, &old_origin_x, &old_origin_y);
  gdk_window_get_origin	(new_window, &new_origin_x, &new_origin_y);

  *x += old_origin_x - new_origin_x;
  *y += old_origin_y - new_origin_y;
}

static GdkEvent *
rewrite_event_for_window (GdkEvent  *event,
			  GdkWindow *new_window)
{
  event = gdk_event_copy (event);

  switch (event->type)
    {
    case GDK_SCROLL:
      rewrite_events_translate (event->any.window,
				new_window,
				&event->scroll.x, &event->scroll.y);
      break;
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      rewrite_events_translate (event->any.window,
				new_window,
				&event->button.x, &event->button.y);
      break;
    case GDK_MOTION_NOTIFY:
      rewrite_events_translate (event->any.window,
				new_window,
				&event->motion.x, &event->motion.y);
      break;
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
    case GDK_PROXIMITY_IN:
    case GDK_PROXIMITY_OUT:
      break;

    default:
      return event;
    }

  g_object_unref (event->any.window);
  event->any.window = g_object_ref (new_window);

  return event;
}

/* If there is a pointer or keyboard grab in effect with owner_events = TRUE,
 * then what X11 does is deliver the event normally if it was going to this
 * client, otherwise, delivers it in terms of the grab window. This function
 * rewrites events to the effect that events going to the same window group
 * are delivered normally, otherwise, the event is delivered in terms of the
 * grab window.
 */
static GdkEvent *
rewrite_event_for_grabs (GdkEvent *event)
{
  GdkWindow *grab_window;
  GtkWidget *event_widget, *grab_widget;
  gboolean owner_events;
  GdkDisplay *display;

  switch (event->type)
    {
    case GDK_SCROLL:
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
    case GDK_MOTION_NOTIFY:
    case GDK_PROXIMITY_IN:
    case GDK_PROXIMITY_OUT:
      display = gdk_drawable_get_display (event->proximity.window);
      if (!gdk_pointer_grab_info_libgtk_only (display, &grab_window, &owner_events) ||
	  !owner_events)
	return NULL;
      break;

    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      display = gdk_drawable_get_display (event->key.window);
      if (!gdk_keyboard_grab_info_libgtk_only (display, &grab_window, &owner_events) ||
	  !owner_events)
	return NULL;
      break;

    default:
      return NULL;
    }

  event_widget = gtk_get_event_widget (event);
  gdk_window_get_user_data (grab_window, (void**) &grab_widget);

  if (grab_widget &&
      gtk_main_get_window_group (grab_widget) != gtk_main_get_window_group (event_widget))
    return rewrite_event_for_window (event, grab_window);
  else
    return NULL;
}

void 
gtk_main_do_event (GdkEvent *event)
{
  GtkWidget *event_widget;
  GtkWidget *grab_widget;
  GtkWindowGroup *window_group;
  GdkEvent *next_event;
  GdkEvent *rewritten_event = NULL;
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

  if (event->type == GDK_SETTING)
    {
      _gtk_settings_handle_event (&event->setting);
      return;
    }

  if (event->type == GDK_OWNER_CHANGE)
    {
      _gtk_clipboard_handle_event (&event->owner_change);
      return;
    }

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
	_gtk_selection_incr_event (event->any.window,
				   &event->property);

      return;
    }

  /* If pointer or keyboard grabs are in effect, munge the events
   * so that each window group looks like a separate app.
   */
  rewritten_event = rewrite_event_for_grabs (event);
  if (rewritten_event)
    {
      event = rewritten_event;
      event_widget = gtk_get_event_widget (event);
    }
  
  window_group = gtk_main_get_window_group (event_widget);

  /* Push the event onto a stack of current events for
   * gtk_current_event_get().
   */
  current_events = g_list_prepend (current_events, event);

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
      g_object_ref (event_widget);
      if ((!window_group->grabs || gtk_widget_get_toplevel (window_group->grabs->data) == event_widget) &&
	  !gtk_widget_event (event_widget, event))
	gtk_widget_destroy (event_widget);
      g_object_unref (event_widget);
      break;
      
    case GDK_DESTROY:
      /* Unexpected GDK_DESTROY from the outside, ignore for
       * child windows, handle like a GDK_DELETE for toplevels
       */
      if (!event_widget->parent)
	{
	  g_object_ref (event_widget);
	  if (!gtk_widget_event (event_widget, event) &&
	      GTK_WIDGET_REALIZED (event_widget))
	    gtk_widget_destroy (event_widget);
	  g_object_unref (event_widget);
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
    case GDK_GRAB_BROKEN:
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
	  g_object_ref (event_widget);
	  
	  gtk_widget_event (grab_widget, event);
	  if (event_widget == grab_widget)
	    GTK_PRIVATE_SET_FLAG (event_widget, GTK_LEAVE_PENDING);
	  
	  g_object_unref (event_widget);
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

  if (rewritten_event)
    gdk_event_free (rewritten_event);
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
    return gtk_window_get_group (GTK_WINDOW (toplevel));
  else
    return gtk_window_get_group (NULL);
}

typedef struct
{
  GtkWidget *old_grab_widget;
  GtkWidget *new_grab_widget;
  gboolean   was_grabbed;
  gboolean   is_grabbed;
} GrabNotifyInfo;

static void
gtk_grab_notify_foreach (GtkWidget *child,
			 gpointer   data)
                        
{
  GrabNotifyInfo *info = data;
 
  gboolean was_grabbed, is_grabbed, was_shadowed, is_shadowed;

  was_grabbed = info->was_grabbed;
  is_grabbed = info->is_grabbed;

  info->was_grabbed = info->was_grabbed || (child == info->old_grab_widget);
  info->is_grabbed = info->is_grabbed || (child == info->new_grab_widget);

  was_shadowed = info->old_grab_widget && !info->was_grabbed;
  is_shadowed = info->new_grab_widget && !info->is_grabbed;

  g_object_ref (child);
  
  if (was_shadowed != is_shadowed)
    _gtk_widget_grab_notify (child, was_shadowed);
  
  if ((was_shadowed || is_shadowed) && GTK_IS_CONTAINER (child))
    gtk_container_forall (GTK_CONTAINER (child), gtk_grab_notify_foreach, info);
      
  g_object_unref (child);
  
  info->was_grabbed = was_grabbed;
  info->is_grabbed = is_grabbed;
}

static void
gtk_grab_notify (GtkWindowGroup *group,
		 GtkWidget      *old_grab_widget,
		 GtkWidget      *new_grab_widget)
{
  GList *toplevels;
  GrabNotifyInfo info;

  if (old_grab_widget == new_grab_widget)
    return;

  info.old_grab_widget = old_grab_widget;
  info.new_grab_widget = new_grab_widget;

  g_object_ref (group);

  toplevels = gtk_window_list_toplevels ();
  g_list_foreach (toplevels, (GFunc)g_object_ref, NULL);
			    
  while (toplevels)
    {
      GtkWindow *toplevel = toplevels->data;
      toplevels = g_list_delete_link (toplevels, toplevels);

      info.was_grabbed = FALSE;
      info.is_grabbed = FALSE;

      if (group == gtk_window_get_group (toplevel))
	gtk_grab_notify_foreach (GTK_WIDGET (toplevel), &info);
      g_object_unref (toplevel);
    }

  g_object_unref (group);
}

void
gtk_grab_add (GtkWidget *widget)
{
  GtkWindowGroup *group;
  GtkWidget *old_grab_widget;
  
  g_return_if_fail (widget != NULL);
  
  if (!GTK_WIDGET_HAS_GRAB (widget) && GTK_WIDGET_IS_SENSITIVE (widget))
    {
      GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_GRAB);
      
      group = gtk_main_get_window_group (widget);

      if (group->grabs)
	old_grab_widget = (GtkWidget *)group->grabs->data;
      else
	old_grab_widget = NULL;

      g_object_ref (widget);
      group->grabs = g_slist_prepend (group->grabs, widget);

      gtk_grab_notify (group, old_grab_widget, widget);
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
  GtkWidget *new_grab_widget;
  
  g_return_if_fail (widget != NULL);
  
  if (GTK_WIDGET_HAS_GRAB (widget))
    {
      GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_GRAB);

      group = gtk_main_get_window_group (widget);
      group->grabs = g_slist_remove (group->grabs, widget);
      
      if (group->grabs)
	new_grab_widget = (GtkWidget *)group->grabs->data;
      else
	new_grab_widget = NULL;

      gtk_grab_notify (group, widget, new_grab_widget);
      
      g_object_unref (widget);
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
gtk_key_snooper_remove (guint snooper_id)
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
    {
      key_snoopers = g_slist_remove (key_snoopers, data);
      g_free (data);
    }
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

  quitf = g_slice_new (GtkQuitFunction);
  
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
  g_slice_free (GtkQuitFunction, quitf);
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
  g_signal_connect (object,
		    "destroy",
		    G_CALLBACK (gtk_widget_destroyed),
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
  return g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, function, data, NULL);
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
  args[0].type = G_TYPE_BOOLEAN;
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
  args[0].type = G_TYPE_INT;
  args[0].name = NULL;
  GTK_VALUE_INT (args[0]) = source;
  args[1].type = GDK_TYPE_INPUT_CONDITION;
  args[1].name = NULL;
  GTK_VALUE_FLAGS (args[1]) = condition;
  args[2].type = G_TYPE_NONE;
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
  if (event && event->any.window && 
      (event->type == GDK_DESTROY || !GDK_WINDOW_DESTROYED (event->any.window)))
    gdk_window_get_user_data (event->any.window, (void**) &widget);
  
  return widget;
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
      args[0].type = G_TYPE_BOOLEAN;
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

  g_object_ref (widget);
      
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

	  /* Scroll events are special cased here because it
	   * feels wrong when scrolling a GtkViewport, say,
	   * to have children of the viewport eat the scroll
	   * event
	   */
	  if (!GTK_WIDGET_IS_SENSITIVE (widget))
	    handled_event = event->type != GDK_SCROLL;
	  else
	    handled_event = gtk_widget_event (widget, event);
	      
	  tmp = widget->parent;
	  g_object_unref (widget);

	  widget = tmp;
	  
	  if (!handled_event && widget)
	    g_object_ref (widget);
	  else
	    break;
	}
    }
  else
    g_object_unref (widget);
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

#define __GTK_MAIN_C__
#include "gtkaliasdef.c"
