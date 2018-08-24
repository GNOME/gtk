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

/**
 * SECTION:gtkmain
 * @Short_description: Library initialization, main event loop, and events
 * @Title: Main loop and Events
 * @See_also:See the GLib manual, especially #GMainLoop and signal-related
 *    functions such as g_signal_connect()
 *
 * Before using GTK, you need to initialize it; initialization connects to the
 * window system display, and parses some standard command line arguments. The
 * gtk_init() macro initializes GTK. gtk_init() exits the application if errors
 * occur; to avoid this, use gtk_init_check(). gtk_init_check() allows you to
 * recover from a failed GTK initialization - you might start up your
 * application in text mode instead.
 *
 * Like all GUI toolkits, GTK uses an event-driven programming model. When the
 * user is doing nothing, GTK sits in the “main loop” and
 * waits for input. If the user performs some action - say, a mouse click - then
 * the main loop “wakes up” and delivers an event to GTK. GTK forwards the
 * event to one or more widgets.
 *
 * When widgets receive an event, they frequently emit one or more
 * “signals”. Signals notify your program that "something
 * interesting happened" by invoking functions you’ve connected to the signal
 * with g_signal_connect(). Functions connected to a signal are often termed
 * “callbacks”.
 *
 * When your callbacks are invoked, you would typically take some action - for
 * example, when an Open button is clicked you might display a
 * #GtkFileChooserDialog. After a callback finishes, GTK will return to the
 * main loop and await more user input.
 *
 * ## Typical main() function for a GTK application
 *
 * |[<!-- language="C" -->
 * int
 * main (int argc, char **argv)
 * {
 *  GtkWidget *mainwin;
 *   // Initialize i18n support with bindtextdomain(), etc.
 *
 *   // ...
 *
 *   // Initialize the widget set
 *   gtk_init ();
 *
 *   // Create the main window
 *   mainwin = gtk_window_new (GTK_WINDOW_TOPLEVEL);
 *
 *   // Set up our GUI elements
 *
 *   // ...
 *
 *   // Show the application window
 *   gtk_widget_show (mainwin);
 *
 *   // Enter the main event loop, and wait for user interaction
 *   gtk_main ();
 *
 *   // The user lost interest
 *   return 0;
 * }
 * ]|
 *
 * It’s OK to use the GLib main loop directly instead of gtk_main(), though it
 * involves slightly more typing. See #GMainLoop in the GLib documentation.
 */

#include "config.h"

#include "gdk/gdk.h"
#include "gdk/gdk-private.h"
#include "gsk/gskprivate.h"

#include <locale.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>          /* For uid_t, gid_t */

#ifdef G_OS_WIN32
#define STRICT
#include <windows.h>
#undef STRICT
#endif

#include "gtkintl.h"

#include "gtkbox.h"
#include "gtkdebug.h"
#include "gtkdndprivate.h"
#include "gtkmain.h"
#include "gtkmediafileprivate.h"
#include "gtkmenu.h"
#include "gtkmodulesprivate.h"
#include "gtkprivate.h"
#include "gtkrecentmanager.h"
#include "gtksettingsprivate.h"
#include "gtktooltipprivate.h"
#include "gtkversion.h"
#include "gtkwidgetprivate.h"
#include "gtkwindowprivate.h"
#include "gtkwindowgroup.h"
#include "gtkprintbackendprivate.h"
#include "gtkimmodule.h"
#include "gtkroot.h"
#include "gtknative.h"

#include "a11y/gtkaccessibility.h"
#include "inspector/window.h"

static GtkWindowGroup *gtk_main_get_window_group (GtkWidget   *widget);

static guint gtk_main_loop_level = 0;
static gint pre_initialized = FALSE;
static gint gtk_initialized = FALSE;
static GList *current_events = NULL;
static GThread *initialized_thread = NULL;

static GSList *main_loops = NULL;      /* stack of currently executing main loops */

typedef struct {
  GdkDisplay *display;
  guint flags;
} DisplayDebugFlags;

#define N_DEBUG_DISPLAYS 4

DisplayDebugFlags debug_flags[N_DEBUG_DISPLAYS];
/* This is a flag to speed up development builds. We set it to TRUE when
 * any of the debug displays has debug flags >0, but we never set it back
 * to FALSE. This way we don't need to call gtk_widget_get_display() in
 * hot paths. */
gboolean any_display_debug_flags_set = FALSE;

#ifdef G_ENABLE_DEBUG
static const GDebugKey gtk_debug_keys[] = {
  { "text", GTK_DEBUG_TEXT },
  { "tree", GTK_DEBUG_TREE },
  { "keybindings", GTK_DEBUG_KEYBINDINGS },
  { "modules", GTK_DEBUG_MODULES },
  { "geometry", GTK_DEBUG_GEOMETRY },
  { "icontheme", GTK_DEBUG_ICONTHEME },
  { "printing", GTK_DEBUG_PRINTING} ,
  { "builder", GTK_DEBUG_BUILDER },
  { "size-request", GTK_DEBUG_SIZE_REQUEST },
  { "no-css-cache", GTK_DEBUG_NO_CSS_CACHE },
  { "baselines", GTK_DEBUG_BASELINES },
  { "interactive", GTK_DEBUG_INTERACTIVE },
  { "touchscreen", GTK_DEBUG_TOUCHSCREEN },
  { "actions", GTK_DEBUG_ACTIONS },
  { "resize", GTK_DEBUG_RESIZE },
  { "layout", GTK_DEBUG_LAYOUT },
  { "snapshot", GTK_DEBUG_SNAPSHOT }
};
#endif /* G_ENABLE_DEBUG */

/**
 * gtk_get_major_version:
 *
 * Returns the major version number of the GTK library.
 * (e.g. in GTK version 3.1.5 this is 3.)
 *
 * This function is in the library, so it represents the GTK library
 * your code is running against. Contrast with the #GTK_MAJOR_VERSION
 * macro, which represents the major version of the GTK headers you
 * have included when compiling your code.
 *
 * Returns: the major version number of the GTK library
 */
guint
gtk_get_major_version (void)
{
  return GTK_MAJOR_VERSION;
}

/**
 * gtk_get_minor_version:
 *
 * Returns the minor version number of the GTK library.
 * (e.g. in GTK version 3.1.5 this is 1.)
 *
 * This function is in the library, so it represents the GTK library
 * your code is are running against. Contrast with the
 * #GTK_MINOR_VERSION macro, which represents the minor version of the
 * GTK headers you have included when compiling your code.
 *
 * Returns: the minor version number of the GTK library
 */
guint
gtk_get_minor_version (void)
{
  return GTK_MINOR_VERSION;
}

/**
 * gtk_get_micro_version:
 *
 * Returns the micro version number of the GTK library.
 * (e.g. in GTK version 3.1.5 this is 5.)
 *
 * This function is in the library, so it represents the GTK library
 * your code is are running against. Contrast with the
 * #GTK_MICRO_VERSION macro, which represents the micro version of the
 * GTK headers you have included when compiling your code.
 *
 * Returns: the micro version number of the GTK library
 */
guint
gtk_get_micro_version (void)
{
  return GTK_MICRO_VERSION;
}

/**
 * gtk_get_binary_age:
 *
 * Returns the binary age as passed to `libtool`
 * when building the GTK library the process is running against.
 * If `libtool` means nothing to you, don't
 * worry about it.
 *
 * Returns: the binary age of the GTK library
 */
guint
gtk_get_binary_age (void)
{
  return GTK_BINARY_AGE;
}

/**
 * gtk_get_interface_age:
 *
 * Returns the interface age as passed to `libtool`
 * when building the GTK library the process is running against.
 * If `libtool` means nothing to you, don't
 * worry about it.
 *
 * Returns: the interface age of the GTK library
 */
guint
gtk_get_interface_age (void)
{
  return GTK_INTERFACE_AGE;
}

/**
 * gtk_check_version:
 * @required_major: the required major version
 * @required_minor: the required minor version
 * @required_micro: the required micro version
 *
 * Checks that the GTK library in use is compatible with the
 * given version. Generally you would pass in the constants
 * #GTK_MAJOR_VERSION, #GTK_MINOR_VERSION, #GTK_MICRO_VERSION
 * as the three arguments to this function; that produces
 * a check that the library in use is compatible with
 * the version of GTK the application or module was compiled
 * against.
 *
 * Compatibility is defined by two things: first the version
 * of the running library is newer than the version
 * @required_major.required_minor.@required_micro. Second
 * the running library must be binary compatible with the
 * version @required_major.required_minor.@required_micro
 * (same major version.)
 *
 * This function is primarily for GTK modules; the module
 * can call this function to check that it wasn’t loaded
 * into an incompatible version of GTK. However, such a
 * check isn’t completely reliable, since the module may be
 * linked against an old version of GTK and calling the
 * old version of gtk_check_version(), but still get loaded
 * into an application using a newer version of GTK.
 *
 * Returns: (nullable): %NULL if the GTK library is compatible with the
 *   given version, or a string describing the version mismatch.
 *   The returned string is owned by GTK and should not be modified
 *   or freed.
 */
const gchar*
gtk_check_version (guint required_major,
                   guint required_minor,
                   guint required_micro)
{
  gint gtk_effective_micro = 100 * GTK_MINOR_VERSION + GTK_MICRO_VERSION;
  gint required_effective_micro = 100 * required_minor + required_micro;

  if (required_major > GTK_MAJOR_VERSION)
    return "GTK version too old (major mismatch)";
  if (required_major < GTK_MAJOR_VERSION)
    return "GTK version too new (major mismatch)";
  if (required_effective_micro < gtk_effective_micro - GTK_BINARY_AGE)
    return "GTK version too new (micro mismatch)";
  if (required_effective_micro > gtk_effective_micro)
    return "GTK version too old (micro mismatch)";
  return NULL;
}

/* This checks to see if the process is running suid or sgid
 * at the current time. If so, we don’t allow GTK to be initialized.
 * This is meant to be a mild check - we only error out if we
 * can prove the programmer is doing something wrong, not if
 * they could be doing something wrong. For this reason, we
 * don’t use issetugid() on BSD or prctl (PR_GET_DUMPABLE).
 */
static gboolean
check_setugid (void)
{
/* this isn't at all relevant on MS Windows and doesn't compile ... --hb */
#ifndef G_OS_WIN32
  uid_t ruid, euid, suid; /* Real, effective and saved user ID's */
  gid_t rgid, egid, sgid; /* Real, effective and saved group ID's */
  
#ifdef HAVE_GETRESUID
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
                 "This is not a supported use of GTK. You must create a helper\n"
                 "program instead. For further details, see:\n\n"
                 "    http://www.gtk.org/setuid.html\n\n"
                 "Refusing to initialize GTK.");
      exit (1);
    }
#endif
  return TRUE;
}

static gboolean do_setlocale = TRUE;

/**
 * gtk_disable_setlocale:
 *
 * Prevents gtk_init(), gtk_init_check() and
 * gtk_parse_args() from automatically
 * calling `setlocale (LC_ALL, "")`. You would
 * want to use this function if you wanted to set the locale for
 * your program to something other than the user’s locale, or if
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
                    default:
                      break;
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
                    default:
                      break;
                    }
                }
            }

          SetThreadLocale (lcid);

          if (GetLocaleInfo (lcid, LOCALE_SENGLANGUAGE, language, sizeof (language)) &&
              GetLocaleInfo (lcid, LOCALE_SENGCOUNTRY, country, sizeof (country)))
            {
              char str[300];

              strcpy (str, language);
              strcat (str, "_");
              strcat (str, country);

              if (setlocale (LC_ALL, str) != NULL)
                setlocale_called = TRUE;
            }

          return FALSE;
        }
    }

  return TRUE;
}
  
#endif

static void
setlocale_initialization (void)
{
  static gboolean initialized = FALSE;

  if (initialized)
    return;
  initialized = TRUE;

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
                    iso3166_to_check = (char *) "SP";
                }
              else
                {
                  script_to_check = strchr (iso639_to_check, '@');
                  if (script_to_check != NULL)
                    *script_to_check++ = '\0';
                  /* LANG_SERBIAN == LANG_CROATIAN, recognize just "sr" */
                  if (strcmp (iso639_to_check, "sr") == 0)
                    iso3166_to_check = (char *) "SP";
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
}

/* Return TRUE if module_to_check causes version conflicts.
 * If module_to_check is NULL, check the main module.
 */
static gboolean
_gtk_module_has_mixed_deps (GModule *module_to_check)
{
  GModule *module;
  gpointer func;
  gboolean result;

  if (!module_to_check)
    module = g_module_open (NULL, 0);
  else
    module = module_to_check;

  if (g_module_symbol (module, "gtk_progress_get_type", &func))
    result = TRUE;
  else if (g_module_symbol (module, "gtk_misc_get_type", &func))
    result = TRUE;
  else
    result = FALSE;

  if (!module_to_check)
    g_module_close (module);

  return result;
}

static void
do_pre_parse_initialization (void)
{
  const gchar *env_string;
  double slowdown;

  if (pre_initialized)
    return;

  pre_initialized = TRUE;

  if (_gtk_module_has_mixed_deps (NULL))
    g_error ("GTK 2/3 symbols detected. Using GTK 2/3 and GTK 4 in the same process is not supported");

  gdk_pre_parse ();

#ifdef G_ENABLE_DEBUG
  env_string = g_getenv ("GTK_DEBUG");
  if (env_string != NULL)
    {
      debug_flags[0].flags = g_parse_debug_string (env_string,
                                                   gtk_debug_keys,
                                                   G_N_ELEMENTS (gtk_debug_keys));
      any_display_debug_flags_set = debug_flags[0].flags > 0;
      env_string = NULL;
    }
#endif  /* G_ENABLE_DEBUG */

  env_string = g_getenv ("GTK_SLOWDOWN");
  if (env_string)
    {
      slowdown = g_ascii_strtod (env_string, NULL);
      _gtk_set_slowdown (slowdown);
    }
}

static void
gettext_initialization (void)
{
  setlocale_initialization ();

#ifdef ENABLE_NLS
  bindtextdomain (GETTEXT_PACKAGE, _gtk_get_localedir ());
  bindtextdomain (GETTEXT_PACKAGE "-properties", _gtk_get_localedir ());
#    ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  bind_textdomain_codeset (GETTEXT_PACKAGE "-properties", "UTF-8");
#    endif
#endif
}

static void
default_display_notify_cb (GdkDisplayManager *dm)
{
  debug_flags[0].display = gdk_display_get_default ();
  _gtk_accessibility_init ();
}

static void
do_post_parse_initialization (void)
{
  GdkDisplayManager *display_manager;

  if (gtk_initialized)
    return;

  gettext_initialization ();

#ifdef SIGPIPE
  signal (SIGPIPE, SIG_IGN);
#endif

  gtk_widget_set_default_direction (gtk_get_locale_direction ());

  gsk_ensure_resources ();
  _gtk_ensure_resources ();

  gtk_initialized = TRUE;

#ifdef G_OS_UNIX
  gtk_print_backends_init ();
#endif
  gtk_im_modules_init ();
  gtk_media_file_extension_init ();

  display_manager = gdk_display_manager_get ();
  if (gdk_display_manager_get_default_display (display_manager) != NULL)
    default_display_notify_cb (display_manager);

  g_signal_connect (display_manager, "notify::default-display",
                    G_CALLBACK (default_display_notify_cb),
                    NULL);
}

guint
gtk_get_display_debug_flags (GdkDisplay *display)
{
  gint i;

  for (i = 0; i < N_DEBUG_DISPLAYS; i++)
    {
      if (debug_flags[i].display == display)
        return debug_flags[i].flags;
    }

  return 0;
}

gboolean
gtk_get_any_display_debug_flag_set (void)
{
  return any_display_debug_flags_set;
}

void
gtk_set_display_debug_flags (GdkDisplay *display,
                             guint       flags)
{
  gint i;

  for (i = 0; i < N_DEBUG_DISPLAYS; i++)
    {
      if (debug_flags[i].display == NULL)
        debug_flags[i].display = display;

      if (debug_flags[i].display == display)
        {
          debug_flags[i].flags = flags;
          if (flags > 0)
            any_display_debug_flags_set = TRUE;

          return;
        }
    }
}

/**
 * gtk_get_debug_flags:
 *
 * Returns the GTK debug flags.
 *
 * This function is intended for GTK modules that want
 * to adjust their debug output based on GTK debug flags.
 *
 * Returns: the GTK debug flags.
 */
guint
gtk_get_debug_flags (void)
{
  return gtk_get_display_debug_flags (gdk_display_get_default ());
}

/**
 * gtk_set_debug_flags:
 *
 * Sets the GTK debug flags.
 */
void
gtk_set_debug_flags (guint flags)
{
  gtk_set_display_debug_flags (gdk_display_get_default (), flags);
}

gboolean
gtk_simulate_touchscreen (void)
{
  static gint test_touchscreen;

  if (test_touchscreen == 0)
    test_touchscreen = g_getenv ("GTK_TEST_TOUCHSCREEN") != NULL ? 1 : -1;

  return test_touchscreen > 0 || (gtk_get_debug_flags () & GTK_DEBUG_TOUCHSCREEN) != 0;
 }

#ifdef G_PLATFORM_WIN32
#undef gtk_init_check
#endif

/**
 * gtk_init_check:
 *
 * This function does the same work as gtk_init() with only a single
 * change: It does not terminate the program if the windowing system
 * can’t be initialized. Instead it returns %FALSE on failure.
 *
 * This way the application can fall back to some other means of
 * communication with the user - for example a curses or command line
 * interface.
 *
 * Returns: %TRUE if the windowing system has been successfully
 *     initialized, %FALSE otherwise
 */
gboolean
gtk_init_check (void)
{
  gboolean ret;

  if (gtk_initialized)
    return TRUE;

  gettext_initialization ();

  if (!check_setugid ())
    return FALSE;

  do_pre_parse_initialization ();
  do_post_parse_initialization ();

  initialized_thread = g_thread_self ();

  ret = gdk_display_open_default () != NULL;

  if (ret && (gtk_get_debug_flags () & GTK_DEBUG_INTERACTIVE))
    gtk_window_set_interactive_debugging (TRUE);

  return ret;
}

#ifdef G_PLATFORM_WIN32
#undef gtk_init
#endif

/**
 * gtk_init:
 *
 * Call this function before using any other GTK functions in your GUI
 * applications.  It will initialize everything needed to operate the
 * toolkit and parses some standard command line options.
 *
 * If you are using #GtkApplication, you don't have to call gtk_init()
 * or gtk_init_check(); the #GtkApplication::startup handler
 * does it for you.
 *
 * This function will terminate your program if it was unable to
 * initialize the windowing system for some reason. If you want
 * your program to fall back to a textual interface you want to
 * call gtk_init_check() instead.
 *
 * GTK calls `signal (SIGPIPE, SIG_IGN)`
 * during initialization, to ignore SIGPIPE signals, since these are
 * almost never wanted in graphical applications. If you do need to
 * handle SIGPIPE for some reason, reset the handler after gtk_init(),
 * but notice that other libraries (e.g. libdbus or gvfs) might do
 * similar things.
 */
void
gtk_init (void)
{
  if (!gtk_init_check ())
    {
      const char *display_name_arg = NULL;
      if (display_name_arg == NULL)
        display_name_arg = getenv ("DISPLAY");
      g_warning ("cannot open display: %s", display_name_arg ? display_name_arg : "");
      exit (1);
    }
}

#ifdef G_OS_WIN32

/* This is relevant when building with gcc for Windows (MinGW),
 * where we want to be struct packing compatible with MSVC,
 * i.e. use the -mms-bitfields switch.
 * For Cygwin there should be no need to be compatible with MSVC,
 * so no need to use G_PLATFORM_WIN32.
 */

static void
check_sizeof_GtkWindow (size_t sizeof_GtkWindow)
{
  if (sizeof_GtkWindow != sizeof (GtkWindow))
    g_error ("Incompatible build!\n"
             "The code using GTK thinks GtkWindow is of different\n"
             "size than it actually is in this build of GTK.\n"
             "On Windows, this probably means that you have compiled\n"
             "your code with gcc without the -mms-bitfields switch,\n"
             "or that you are using an unsupported compiler.");
}

/* In GTK 2.0 the GtkWindow struct actually is the same size in
 * gcc-compiled code on Win32 whether compiled with -fnative-struct or
 * not. Unfortunately this wan’t noticed until after GTK 2.0.1. So,
 * from GTK 2.0.2 on, check some other struct, too, where the use of
 * -fnative-struct still matters. GtkBox is one such.
 */
static void
check_sizeof_GtkBox (size_t sizeof_GtkBox)
{
  if (sizeof_GtkBox != sizeof (GtkBox))
    g_error ("Incompatible build!\n"
             "The code using GTK thinks GtkBox is of different\n"
             "size than it actually is in this build of GTK.\n"
             "On Windows, this probably means that you have compiled\n"
             "your code with gcc without the -mms-bitfields switch,\n"
             "or that you are using an unsupported compiler.");
}

/* These two functions might get more checks added later, thus pass
 * in the number of extra args.
 */
void
gtk_init_abi_check (int num_checks, size_t sizeof_GtkWindow, size_t sizeof_GtkBox)
{
  check_sizeof_GtkWindow (sizeof_GtkWindow);
  if (num_checks >= 2)
    check_sizeof_GtkBox (sizeof_GtkBox);
  gtk_init ();
}

gboolean
gtk_init_check_abi_check (int num_checks, size_t sizeof_GtkWindow, size_t sizeof_GtkBox)
{
  check_sizeof_GtkWindow (sizeof_GtkWindow);
  if (num_checks >= 2)
    check_sizeof_GtkBox (sizeof_GtkBox);
  return gtk_init_check ();
}

#endif

/**
 * gtk_is_initialized:
 *
 * Use this function to check if GTK has been initialized with gtk_init()
 * or gtk_init_check().
 *
 * Returns: the initialization status
 */
gboolean
gtk_is_initialized (void)
{
  return gtk_initialized;
}

/**
 * gtk_get_main_thread:
 *
 * Get the thread from which GTK was initialized.
 *
 * Returns: (transfer none): The #GThread initialized for GTK, must not be freed
 */
GThread *
gtk_get_main_thread (void)
{
  return initialized_thread;
}


/**
 * gtk_get_locale_direction:
 *
 * Get the direction of the current locale. This is the expected
 * reading direction for text and UI.
 *
 * This function depends on the current locale being set with
 * setlocale() and will default to setting the %GTK_TEXT_DIR_LTR
 * direction otherwise. %GTK_TEXT_DIR_NONE will never be returned.
 *
 * GTK sets the default text direction according to the locale
 * during gtk_init(), and you should normally use
 * gtk_widget_get_direction() or gtk_widget_get_default_direction()
 * to obtain the current direcion.
 *
 * This function is only needed rare cases when the locale is
 * changed after GTK has already been initialized. In this case,
 * you can use it to update the default text direction as follows:
 *
 * |[<!-- language="C" -->
 * setlocale (LC_ALL, new_locale);
 * direction = gtk_get_locale_direction ();
 * gtk_widget_set_default_direction (direction);
 * ]|
 *
 * Returns: the #GtkTextDirection of the current locale
 */
GtkTextDirection
gtk_get_locale_direction (void)
{
  /* Translate to default:RTL if you want your widgets
   * to be RTL, otherwise translate to default:LTR.
   * Do *not* translate it to "predefinito:LTR", if it
   * it isn't default:LTR or default:RTL it will not work
   */
  gchar            *e   = _("default:LTR");
  GtkTextDirection  dir = GTK_TEXT_DIR_LTR;

  if (g_strcmp0 (e, "default:RTL") == 0)
    dir = GTK_TEXT_DIR_RTL;
  else if (g_strcmp0 (e, "default:LTR") != 0)
    g_warning ("Whoever translated default:LTR did so wrongly. Defaulting to LTR.");

  return dir;
}

/**
 * gtk_get_default_language:
 *
 * Returns the #PangoLanguage for the default language currently in
 * effect. (Note that this can change over the life of an
 * application.) The default language is derived from the current
 * locale. It determines, for example, whether GTK uses the
 * right-to-left or left-to-right text direction.
 *
 * This function is equivalent to pango_language_get_default().
 * See that function for details.
 *
 * Returns: (transfer none): the default language as a #PangoLanguage,
 *     must not be freed
 */
PangoLanguage *
gtk_get_default_language (void)
{
  return pango_language_get_default ();
}

/**
 * gtk_main:
 *
 * Runs the main loop until gtk_main_quit() is called.
 *
 * You can nest calls to gtk_main(). In that case gtk_main_quit()
 * will make the innermost invocation of the main loop return.
 */
void
gtk_main (void)
{
  GMainLoop *loop;

  gtk_main_loop_level++;

  loop = g_main_loop_new (NULL, TRUE);
  main_loops = g_slist_prepend (main_loops, loop);

  if (g_main_loop_is_running (main_loops->data))
    g_main_loop_run (loop);

  main_loops = g_slist_remove (main_loops, loop);

  g_main_loop_unref (loop);

  gtk_main_loop_level--;

  if (gtk_main_loop_level == 0)
    gtk_main_sync ();
}

typedef struct {
  GMainLoop *store_loop;
  guint n_clipboards;
} ClipboardStore;

static void
clipboard_store_finished (GObject      *source,
                          GAsyncResult *result,
                          gpointer      data)
{
  ClipboardStore *store;
  GError *error = NULL;

  if (!gdk_clipboard_store_finish (GDK_CLIPBOARD (source), result, &error))
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_error_free (error);
          return;
        }

      g_error_free (error);
    }

  store = data;
  store->n_clipboards--;
  if (store->n_clipboards == 0)
    g_main_loop_quit (store->store_loop);
}

void
gtk_main_sync (void)
{
  ClipboardStore store = { NULL, };
  GSList *displays, *l;
  GCancellable *cancel;
  guint store_timeout;
  
  /* Try storing all clipboard data we have */
  displays = gdk_display_manager_list_displays (gdk_display_manager_get ());
  cancel = g_cancellable_new ();

  for (l = displays; l; l = l->next)
    {
      GdkDisplay *display = l->data;
      GdkClipboard *clipboard = gdk_display_get_clipboard (display);

      gdk_clipboard_store_async (clipboard,
                                 G_PRIORITY_HIGH,
                                 cancel,
                                 clipboard_store_finished,
                                 &store);
      store.n_clipboards++;
    }
  g_slist_free (displays);

  store.store_loop = g_main_loop_new (NULL, TRUE);
  store_timeout = g_timeout_add_seconds (10, (GSourceFunc) g_main_loop_quit, store.store_loop);
  g_source_set_name_by_id (store_timeout, "[gtk] gtk_main_sync clipboard store timeout");

  if (g_main_loop_is_running (store.store_loop))
    g_main_loop_run (store.store_loop);
  
  g_cancellable_cancel (cancel);
  g_object_unref (cancel);
  g_source_remove (store_timeout);
  g_main_loop_unref (store.store_loop);
  store.store_loop = NULL;
  
  /* Synchronize the recent manager singleton */
  _gtk_recent_manager_sync ();
}

/**
 * gtk_main_level:
 *
 * Asks for the current nesting level of the main loop.
 *
 * Returns: the nesting level of the current invocation
 *     of the main loop
 */
guint
gtk_main_level (void)
{
  return gtk_main_loop_level;
}

/**
 * gtk_main_quit:
 *
 * Makes the innermost invocation of the main loop return
 * when it regains control.
 */
void
gtk_main_quit (void)
{
  g_return_if_fail (main_loops != NULL);

  g_main_loop_quit (main_loops->data);
}

/**
 * gtk_events_pending:
 *
 * Checks if any events are pending.
 *
 * This can be used to update the UI and invoke timeouts etc.
 * while doing some time intensive computation.
 *
 * ## Updating the UI during a long computation
 *
 * |[<!-- language="C" -->
 *  // computation going on...
 *
 *  while (gtk_events_pending ())
 *    gtk_main_iteration ();
 *
 *  // ...computation continued
 * ]|
 *
 * Returns: %TRUE if any events are pending, %FALSE otherwise
 */
gboolean
gtk_events_pending (void)
{
  return g_main_context_pending (NULL);
}

/**
 * gtk_main_iteration:
 *
 * Runs a single iteration of the mainloop.
 *
 * If no events are waiting to be processed GTK will block
 * until the next event is noticed. If you don’t want to block
 * look at gtk_main_iteration_do() or check if any events are
 * pending with gtk_events_pending() first.
 *
 * Returns: %TRUE if gtk_main_quit() has been called for the
 *     innermost mainloop
 */
gboolean
gtk_main_iteration (void)
{
  g_main_context_iteration (NULL, TRUE);

  if (main_loops)
    return !g_main_loop_is_running (main_loops->data);
  else
    return TRUE;
}

/**
 * gtk_main_iteration_do:
 * @blocking: %TRUE if you want GTK to block if no events are pending
 *
 * Runs a single iteration of the mainloop.
 * If no events are available either return or block depending on
 * the value of @blocking.
 *
 * Returns: %TRUE if gtk_main_quit() has been called for the
 *     innermost mainloop
 */
gboolean
gtk_main_iteration_do (gboolean blocking)
{
  g_main_context_iteration (NULL, blocking);

  if (main_loops)
    return !g_main_loop_is_running (main_loops->data);
  else
    return TRUE;
}

static void
rewrite_events_translate (GdkSurface *old_surface,
                          GdkSurface *new_surface,
                          gdouble   *x,
                          gdouble   *y)
{
  if (!gdk_surface_translate_coordinates (old_surface, new_surface, x, y))
    {
      *x = 0;
      *y = 0;
    }
}

static GdkEvent *
rewrite_event_for_surface (GdkEvent  *event,
			   GdkSurface *new_surface)
{
  event = gdk_event_copy (event);

  switch ((guint) event->any.type)
    {
    case GDK_SCROLL:
      rewrite_events_translate (event->any.surface,
                                new_surface,
                                &event->scroll.x, &event->scroll.y);
      break;
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      rewrite_events_translate (event->any.surface,
                                new_surface,
                                &event->button.x, &event->button.y);
      break;
    case GDK_MOTION_NOTIFY:
      rewrite_events_translate (event->any.surface,
                                new_surface,
                                &event->motion.x, &event->motion.y);
      break;
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
      rewrite_events_translate (event->any.surface,
                                new_surface,
                                &event->touch.x, &event->touch.y);
      break;
    case GDK_TOUCHPAD_SWIPE:
      rewrite_events_translate (event->any.surface,
                                new_surface,
                                &event->touchpad_swipe.x,
                                &event->touchpad_swipe.y);
      break;
    case GDK_TOUCHPAD_PINCH:
      rewrite_events_translate (event->any.surface,
                                new_surface,
                                &event->touchpad_pinch.x,
                                &event->touchpad_pinch.y);
      break;
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
    case GDK_PROXIMITY_IN:
    case GDK_PROXIMITY_OUT:
      break;

    default:
      return event;
    }

  g_object_unref (event->any.surface);
  event->any.surface = g_object_ref (new_surface);

  return event;
}

/* If there is a pointer or keyboard grab in effect with owner_events = TRUE,
 * then what X11 does is deliver the event normally if it was going to this
 * client, otherwise, delivers it in terms of the grab surface. This function
 * rewrites events to the effect that events going to the same window group
 * are delivered normally, otherwise, the event is delivered in terms of the
 * grab window.
 */
static GdkEvent *
rewrite_event_for_grabs (GdkEvent *event)
{
  GdkSurface *grab_surface;
  GtkWidget *event_widget, *grab_widget;
  gboolean owner_events;
  GdkDisplay *display;
  GdkDevice *device;

  switch ((guint) event->any.type)
    {
    case GDK_SCROLL:
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
    case GDK_MOTION_NOTIFY:
    case GDK_PROXIMITY_IN:
    case GDK_PROXIMITY_OUT:
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
    case GDK_TOUCHPAD_SWIPE:
    case GDK_TOUCHPAD_PINCH:
      display = gdk_surface_get_display (event->any.surface);
      device = gdk_event_get_device (event);

      if (!gdk_device_grab_info (display, device, &grab_surface, &owner_events) ||
          !owner_events)
        return NULL;
      break;
    default:
      return NULL;
    }

  event_widget = gtk_get_event_widget (event);
  grab_widget = gtk_native_get_for_surface (grab_surface);

  if (grab_widget &&
      gtk_main_get_window_group (grab_widget) != gtk_main_get_window_group (event_widget))
    return rewrite_event_for_surface (event, grab_surface);
  else
    return NULL;
}

static GtkWidget *
widget_get_popover_ancestor (GtkWidget *widget,
                             GtkWindow *window)
{
  GtkWidget *parent = gtk_widget_get_parent (widget);

  while (parent && parent != GTK_WIDGET (window))
    {
      widget = parent;
      parent = gtk_widget_get_parent (widget);
    }

  if (!parent || parent != GTK_WIDGET (window))
    return NULL;

  if (_gtk_window_is_popover_widget (GTK_WINDOW (window), widget))
    return widget;

  return NULL;
}

static gboolean
check_event_in_child_popover (GtkWidget *event_widget,
                              GtkWidget *grab_widget)
{
  GtkWidget *window, *popover = NULL, *popover_parent = NULL;

  if (grab_widget == event_widget)
    return FALSE;

  window = gtk_widget_get_ancestor (event_widget, GTK_TYPE_WINDOW);

  if (!window)
    return FALSE;

  popover = widget_get_popover_ancestor (event_widget, GTK_WINDOW (window));

  if (!popover)
    return FALSE;

  popover_parent = _gtk_window_get_popover_parent (GTK_WINDOW (window), popover);

  if (!popover_parent)
    return FALSE;

  return (popover_parent == grab_widget || gtk_widget_is_ancestor (popover_parent, grab_widget));
}

static GdkNotifyType
get_virtual_notify_type (GdkNotifyType notify_type)
{
  switch (notify_type)
    {
    case GDK_NOTIFY_ANCESTOR:
    case GDK_NOTIFY_INFERIOR:
      return GDK_NOTIFY_VIRTUAL;
    case GDK_NOTIFY_NONLINEAR:
      return GDK_NOTIFY_NONLINEAR_VIRTUAL;
    case GDK_NOTIFY_VIRTUAL:
    case GDK_NOTIFY_NONLINEAR_VIRTUAL:
    case GDK_NOTIFY_UNKNOWN:
    default:
      g_assert_not_reached ();
      return GDK_NOTIFY_UNKNOWN;
    }
}

/* Determine from crossing mode details whether the ultimate
 * target is us or a descendant. Keep this code in sync with
 * gtkeventcontrollerkey.c:update_focus
 */
static gboolean
is_or_contains (gboolean      enter,
                GdkNotifyType detail)
{
  gboolean is = FALSE;
  gboolean contains = FALSE;

  switch (detail)
    {
    case GDK_NOTIFY_VIRTUAL:
    case GDK_NOTIFY_NONLINEAR_VIRTUAL:
      is = FALSE;
      contains = enter;
      break;
    case GDK_NOTIFY_ANCESTOR:
    case GDK_NOTIFY_NONLINEAR:
      is = enter;
      contains = FALSE;
      break;
    case GDK_NOTIFY_INFERIOR:
      is = enter;
      contains = !enter;
      break;
    case GDK_NOTIFY_UNKNOWN:
    default:
      g_warning ("Unknown focus change detail");
      break;
    }

  return is || contains;
}

static void
synth_crossing (GtkWidget       *widget,
                GtkWidget       *toplevel,
                gboolean         enter,
                GtkWidget       *target,
                GtkWidget       *related_target,
                GdkEvent        *source,
                GdkNotifyType    notify_type,
                GdkCrossingMode  crossing_mode)
{
  GdkEvent *event;
  GtkStateFlags flags;

  if (gdk_event_get_event_type (source) == GDK_FOCUS_CHANGE)
    {
      event = gdk_event_new (GDK_FOCUS_CHANGE);
      event->focus_change.in = enter;
      event->focus_change.mode = crossing_mode;
      event->focus_change.detail = notify_type;

      flags = GTK_STATE_FLAG_FOCUSED;
      if (!GTK_IS_WINDOW (toplevel) || gtk_window_get_focus_visible (GTK_WINDOW (toplevel)))
        flags |= GTK_STATE_FLAG_FOCUS_VISIBLE;
    }
  else
    {
      gdouble x, y;
      event = gdk_event_new (enter ? GDK_ENTER_NOTIFY : GDK_LEAVE_NOTIFY);
      if (related_target)
        {
          GdkSurface *surface;

          surface = gtk_native_get_surface (gtk_widget_get_native (related_target));
          event->crossing.child_surface = g_object_ref (surface);
        }
      gdk_event_get_coords (source, &x, &y);
      event->crossing.x = x;
      event->crossing.y = y;
      event->crossing.mode = crossing_mode;
      event->crossing.detail = notify_type;

      flags = GTK_STATE_FLAG_PRELIGHT;
    }

  gdk_event_set_target (event, G_OBJECT (target));
  gdk_event_set_related_target (event, G_OBJECT (related_target));
  gdk_event_set_device (event, gdk_event_get_device (source));
  gdk_event_set_source_device (event, gdk_event_get_source_device (source));

  event->any.surface = gtk_native_get_surface (gtk_widget_get_native (toplevel));
  if (event->any.surface)
    g_object_ref (event->any.surface);

  if (is_or_contains (enter, notify_type))
    gtk_widget_set_state_flags (widget, flags, FALSE);
  else
    gtk_widget_unset_state_flags (widget, flags);

  if (gdk_event_get_event_type (source) == GDK_FOCUS_CHANGE)
    {
      /* maintain focus chain */
      if (enter || notify_type == GDK_NOTIFY_INFERIOR)
        {
          GtkWidget *parent = gtk_widget_get_parent (widget);
          if (parent)
            gtk_widget_set_focus_child (parent, widget);
        }
      else if (!enter && notify_type != GDK_NOTIFY_INFERIOR)
        {
          GtkWidget *parent = gtk_widget_get_parent (widget);
          if (parent)
            gtk_widget_set_focus_child (parent, NULL);
        }

      /* maintain widget state */
      if (notify_type == GDK_NOTIFY_ANCESTOR ||
          notify_type == GDK_NOTIFY_INFERIOR ||
          notify_type == GDK_NOTIFY_NONLINEAR)
        gtk_widget_set_has_focus (widget, enter);
    }
    
  gtk_widget_event (widget, event);
  g_object_unref (event);
}

void
gtk_synthesize_crossing_events (GtkRoot         *toplevel,
                                GtkWidget       *old_target,
                                GtkWidget       *new_target,
                                GdkEvent        *event,
                                GdkCrossingMode  mode)
{
  GtkWidget *ancestor = NULL, *widget;
  GdkNotifyType enter_type, leave_type, notify_type;

  if (old_target && new_target)
    ancestor = gtk_widget_common_ancestor (old_target, new_target);

  if (ancestor == old_target)
    {
      leave_type = GDK_NOTIFY_INFERIOR;
      enter_type = GDK_NOTIFY_ANCESTOR;
    }
  else if (ancestor == new_target)
    {
      leave_type = GDK_NOTIFY_ANCESTOR;
      enter_type = GDK_NOTIFY_INFERIOR;
    }
  else
    enter_type = leave_type = GDK_NOTIFY_NONLINEAR;

  if (old_target)
    {
      widget = old_target;

      while (widget)
        {
          notify_type = (widget == old_target) ?
            leave_type : get_virtual_notify_type (leave_type);

          if (widget != ancestor || widget == old_target)
            synth_crossing (widget, GTK_WIDGET (toplevel), FALSE,
                            old_target, new_target, event, notify_type, mode);
          if (widget == ancestor || widget == GTK_WIDGET (toplevel))
            break;
          widget = gtk_widget_get_parent (widget);
        }
    }

  if (new_target)
    {
      GSList *widgets = NULL;

      widget = new_target;

      while (widget)
        {
          widgets = g_slist_prepend (widgets, widget);
          if (widget == ancestor || widget == GTK_WIDGET (toplevel))
            break;
          widget = gtk_widget_get_parent (widget);
        }

      while (widgets)
        {
          widget = widgets->data;
          widgets = g_slist_delete_link (widgets, widgets);
          notify_type = (widget == new_target) ?
            enter_type : get_virtual_notify_type (enter_type);

          if (widget != ancestor || widget == new_target)
            synth_crossing (widget, GTK_WIDGET (toplevel), TRUE,
                            new_target, old_target, event, notify_type, mode);
        }
    }
}


static GtkWidget *
update_pointer_focus_state (GtkWindow *toplevel,
                            GdkEvent  *event,
                            GtkWidget *new_target)
{
  GtkWidget *old_target = NULL;
  GdkEventSequence *sequence;
  GdkDevice *device;
  gdouble x, y;

  device = gdk_event_get_device (event);
  sequence = gdk_event_get_event_sequence (event);
  old_target = gtk_window_lookup_pointer_focus_widget (toplevel, device, sequence);
  if (old_target == new_target)
    return old_target;

  gdk_event_get_coords (event, &x, &y);
  gtk_window_update_pointer_focus (toplevel, device, sequence,
                                   new_target, x, y);

  return old_target;
}

static gboolean
is_pointing_event (GdkEvent *event)
{
  switch ((guint) event->any.type)
    {
    case GDK_MOTION_NOTIFY:
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
    case GDK_SCROLL:
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
    case GDK_TOUCHPAD_PINCH:
    case GDK_TOUCHPAD_SWIPE:
      return TRUE;
      break;
    default:
      return FALSE;
    }
}

static gboolean
is_key_event (GdkEvent *event)
{
  switch ((guint) event->any.type)
    {
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      return TRUE;
      break;
    default:
      return FALSE;
    }
}

static gboolean
is_focus_event (GdkEvent *event)
{
  switch ((guint) event->any.type)
    {
    case GDK_FOCUS_CHANGE:
      return TRUE;
      break;
    default:
      return FALSE;
    }
}


static inline void
set_widget_active_state (GtkWidget       *target,
                         const gboolean   release)
{
  GtkWidget *w;

  w = target;
  while (w)
    {
      if (release)
        gtk_widget_unset_state_flags (w, GTK_STATE_FLAG_ACTIVE);
      else
        gtk_widget_set_state_flags (w, GTK_STATE_FLAG_ACTIVE, FALSE);

      w = gtk_widget_get_parent (w);
    }
}

static GtkWidget *
handle_pointing_event (GdkEvent *event)
{
  GtkWidget *target = NULL, *old_target = NULL, *event_widget;
  GtkWindow *toplevel;
  GdkEventSequence *sequence;
  GdkDevice *device;
  gdouble x, y;
  GtkWidget *native;

  event_widget = gtk_get_event_widget (event);
  device = gdk_event_get_device (event);
  if (!device || !gdk_event_get_coords (event, &x, &y))
    return event_widget;

  toplevel = GTK_WINDOW (gtk_widget_get_root (event_widget));
  native = GTK_WIDGET (gtk_widget_get_native (event_widget));

  sequence = gdk_event_get_event_sequence (event);

  switch ((guint) event->any.type)
    {
    case GDK_LEAVE_NOTIFY:
      if (event->crossing.mode == GDK_CROSSING_GRAB ||
          event->crossing.mode == GDK_CROSSING_UNGRAB)
        break;
      G_GNUC_FALLTHROUGH;
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
      old_target = update_pointer_focus_state (toplevel, event, NULL);

      if (event->any.type == GDK_LEAVE_NOTIFY)
        gtk_synthesize_crossing_events (GTK_ROOT (toplevel), old_target, NULL,
                                        event, event->crossing.mode);
      break;
    case GDK_ENTER_NOTIFY:
      if (event->crossing.mode == GDK_CROSSING_GRAB ||
          event->crossing.mode == GDK_CROSSING_UNGRAB)
        break;
      G_GNUC_FALLTHROUGH;
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_MOTION_NOTIFY:
      target = gtk_window_lookup_pointer_focus_implicit_grab (toplevel, device, sequence);

      if (!target)
       target = gtk_widget_pick (native, x, y, GTK_PICK_DEFAULT);

      if (!target)
        target = GTK_WIDGET (native);

      old_target = update_pointer_focus_state (toplevel, event, target);

      if (event->any.type == GDK_MOTION_NOTIFY || event->any.type == GDK_ENTER_NOTIFY)
        {
          if (!gtk_window_lookup_pointer_focus_implicit_grab (toplevel, device,
                                                              sequence))
            {
              gtk_synthesize_crossing_events (GTK_ROOT (toplevel), old_target, target,
                                              event, GDK_CROSSING_NORMAL);
            }

          gtk_window_maybe_update_cursor (toplevel, NULL, device);
        }

      if (event->any.type == GDK_TOUCH_BEGIN)
        gtk_window_set_pointer_focus_grab (toplevel, device, sequence, target);

      /* Let it take the effective pointer focus anyway, as it may change due
       * to implicit grabs.
       */
      target = NULL;
      break;
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      target = gtk_window_lookup_effective_pointer_focus_widget (toplevel,
                                                                 device,
                                                                 sequence);
      gtk_window_set_pointer_focus_grab (toplevel, device, sequence,
                                         event->any.type == GDK_BUTTON_PRESS ?
                                         target : NULL);

      if (event->any.type == GDK_BUTTON_RELEASE)
        {
          GtkWidget *new_target;
          new_target = gtk_widget_pick (GTK_WIDGET (native), x, y, GTK_PICK_DEFAULT);
          if (new_target == NULL)
            new_target = GTK_WIDGET (toplevel);
          gtk_synthesize_crossing_events (GTK_ROOT (toplevel), target, new_target, event,
                                          GDK_CROSSING_UNGRAB);
          gtk_window_maybe_update_cursor (toplevel, NULL, device);
        }

      set_widget_active_state (target, event->any.type == GDK_BUTTON_RELEASE);

      break;
    case GDK_SCROLL:
    case GDK_TOUCHPAD_PINCH:
    case GDK_TOUCHPAD_SWIPE:
      break;
    default:
      g_assert_not_reached ();
    }

  if (!target)
    target = gtk_window_lookup_effective_pointer_focus_widget (toplevel,
                                                               device,
                                                               sequence);
  return target ? target : old_target;
}

static GtkWidget *
handle_key_event (GdkEvent *event)
{
  GtkWidget *event_widget;
  GtkWidget *focus_widget;

  event_widget = gtk_get_event_widget (event);

  focus_widget = gtk_root_get_focus (gtk_widget_get_root (event_widget));
  return focus_widget ? focus_widget : event_widget;
}

/**
 * gtk_main_do_event:
 * @event: An event to process (normally passed by GDK)
 *
 * Processes a single GDK event.
 *
 * This is public only to allow filtering of events between GDK and GTK.
 * You will not usually need to call this function directly.
 *
 * While you should not call this function directly, you might want to
 * know how exactly events are handled. So here is what this function
 * does with the event:
 *
 * 1. Compress enter/leave notify events. If the event passed build an
 *    enter/leave pair together with the next event (peeked from GDK), both
 *    events are thrown away. This is to avoid a backlog of (de-)highlighting
 *    widgets crossed by the pointer.
 * 
 * 2. Find the widget which got the event. If the widget can’t be determined
 *    the event is thrown away unless it belongs to a INCR transaction.
 *
 * 3. Then the event is pushed onto a stack so you can query the currently
 *    handled event with gtk_get_current_event().
 * 
 * 4. The event is sent to a widget. If a grab is active all events for widgets
 *    that are not in the contained in the grab widget are sent to the latter
 *    with a few exceptions:
 *    - Deletion and destruction events are still sent to the event widget for
 *      obvious reasons.
 *    - Events which directly relate to the visual representation of the event
 *      widget.
 *    - Leave events are delivered to the event widget if there was an enter
 *      event delivered to it before without the paired leave event.
 *    - Drag events are not redirected because it is unclear what the semantics
 *      of that would be.
 * 
 * 5. After finishing the delivery the event is popped from the event stack.
 */
void
gtk_main_do_event (GdkEvent *event)
{
  GtkWidget *event_widget;
  GtkWidget *target_widget;
  GtkWidget *grab_widget = NULL;
  GtkWindowGroup *window_group;
  GdkEvent *rewritten_event = NULL;
  GdkDevice *device;
  GList *tmp_list;

  if (gtk_inspector_handle_event (event))
    return;

  /* Find the widget which got the event. We store the widget
   * in the user_data field of GdkSurface's. Ignore the event
   * if we don't have a widget for it.
   */
  event_widget = gtk_get_event_widget (event);
  if (!event_widget)
    return;

  target_widget = event_widget;

  /* If pointer or keyboard grabs are in effect, munge the events
   * so that each window group looks like a separate app.
   */
  rewritten_event = rewrite_event_for_grabs (event);
  if (rewritten_event)
    {
      event = rewritten_event;
      target_widget = gtk_get_event_widget (event);
    }

  /* Push the event onto a stack of current events for
   * gtk_current_event_get().
   */
  current_events = g_list_prepend (current_events, event);

  if (is_pointing_event (event))
    target_widget = handle_pointing_event (event);
  else if (is_key_event (event))
    target_widget = handle_key_event (event);
  else if (is_focus_event (event))
    {
      if (!GTK_IS_WINDOW (target_widget))
        {
          g_message ("Ignoring an unexpected focus event from GDK on a non-toplevel surface.");
          goto cleanup;
        }
    }

  if (!target_widget)
    goto cleanup;

  gdk_event_set_target (event, G_OBJECT (target_widget));

  window_group = gtk_main_get_window_group (target_widget);
  device = gdk_event_get_device (event);

  /* check whether there is a (device) grab in effect... */
  if (device)
    grab_widget = gtk_window_group_get_current_device_grab (window_group, device);

  if (!grab_widget)
    grab_widget = gtk_window_group_get_current_grab (window_group);

  /* If the grab widget is an ancestor of the event widget
   * then we send the event to the original event widget.
   * This is the key to implementing modality.
   */
  if (!grab_widget ||
      ((gtk_widget_is_sensitive (target_widget) || event->any.type == GDK_SCROLL) &&
       gtk_widget_is_ancestor (target_widget, grab_widget)))
    grab_widget = target_widget;

  /* popovers are not really a "child" of their "parent" in the widget/window
   * hierarchy sense, we however want to interact with popovers spawn by widgets
   * within grab_widget. If this is the case, we let the event go through
   * unaffected by the grab.
   */
  if (check_event_in_child_popover (target_widget, grab_widget))
    grab_widget = target_widget;

  /* If the widget receiving events is actually blocked by another
   * device GTK grab
   */
  if (device &&
      _gtk_window_group_widget_is_blocked_for_device (window_group, grab_widget, device))
    goto cleanup;

  /* Not all events get sent to the grabbing widget.
   * The delete, destroy, expose, focus change and resize
   * events still get sent to the event widget because
   * 1) these events have no meaning for the grabbing widget
   * and 2) redirecting these events to the grabbing widget
   * could cause the display to be messed up.
   *
   * Drag events are also not redirected, since it isn't
   * clear what the semantics of that would be.
   */
  switch ((guint)event->any.type)
    {
    case GDK_NOTHING:
      break;

    case GDK_DELETE:
      g_object_ref (target_widget);
      if (!gtk_window_group_get_current_grab (window_group) ||
          GTK_WIDGET (gtk_widget_get_root (gtk_window_group_get_current_grab (window_group))) == target_widget)
        {
          if (!GTK_IS_WINDOW (target_widget) ||
              !gtk_window_emit_close_request (GTK_WINDOW (target_widget)))
            gtk_widget_destroy (target_widget);
        }
      g_object_unref (target_widget);
      break;

    case GDK_DESTROY:
      /* Unexpected GDK_DESTROY from the outside, ignore for
       * child windows, handle like a GDK_DELETE for toplevels
       */
      if (!gtk_widget_get_parent (target_widget))
        {
          g_object_ref (target_widget);
          if (!gtk_widget_event (target_widget, event) &&
              gtk_widget_get_realized (target_widget))
            gtk_widget_destroy (target_widget);
          g_object_unref (target_widget);
        }
      break;

    case GDK_FOCUS_CHANGE:
    case GDK_GRAB_BROKEN:
      if (!_gtk_widget_captured_event (target_widget, event))
        gtk_widget_event (target_widget, event);
      break;

    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
    case GDK_SCROLL:
    case GDK_BUTTON_PRESS:
    case GDK_TOUCH_BEGIN:
    case GDK_MOTION_NOTIFY:
    case GDK_BUTTON_RELEASE:
    case GDK_PROXIMITY_IN:
    case GDK_PROXIMITY_OUT:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
    case GDK_TOUCHPAD_SWIPE:
    case GDK_TOUCHPAD_PINCH:
    case GDK_PAD_BUTTON_PRESS:
    case GDK_PAD_BUTTON_RELEASE:
    case GDK_PAD_RING:
    case GDK_PAD_STRIP:
    case GDK_PAD_GROUP_MODE:
      gtk_propagate_event (grab_widget, event);
      break;

    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      /* Crossing event propagation happens during picking */
      break;

    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DROP_START:
      _gtk_drag_dest_handle_event (target_widget, event);
      break;
    case GDK_EVENT_LAST:
    default:
      g_assert_not_reached ();
      break;
    }

  _gtk_tooltip_handle_event (event);

 cleanup:
  tmp_list = current_events;
  current_events = g_list_remove_link (current_events, tmp_list);
  g_list_free_1 (tmp_list);

  if (rewritten_event)
    g_object_unref (rewritten_event);
}

static GtkWindowGroup *
gtk_main_get_window_group (GtkWidget *widget)
{
  GtkWidget *toplevel = NULL;

  if (widget)
    toplevel = GTK_WIDGET (gtk_widget_get_root (widget));

  if (GTK_IS_WINDOW (toplevel))
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
  gboolean   from_grab;
  GList     *notified_surfaces;
  GdkDevice *device;
} GrabNotifyInfo;

static void
synth_crossing_for_grab_notify (GtkWidget       *from,
                                GtkWidget       *to,
                                GrabNotifyInfo  *info,
                                GList           *devices,
                                GdkCrossingMode  mode)
{
  while (devices)
    {
      GdkDevice *device = devices->data;
      GdkSurface *from_surface, *to_surface;

      /* Do not propagate events more than once to
       * the same surfaces if non-multidevice aware.
       */
      if (!from)
        from_surface = NULL;
      else
        {
          from_surface = _gtk_widget_get_device_surface (from, device);

          if (from_surface &&
              !gdk_surface_get_support_multidevice (from_surface) &&
              g_list_find (info->notified_surfaces, from_surface))
            from_surface = NULL;
        }

      if (!to)
        to_surface = NULL;
      else
        {
          to_surface = _gtk_widget_get_device_surface (to, device);

          if (to_surface &&
              !gdk_surface_get_support_multidevice (to_surface) &&
              g_list_find (info->notified_surfaces, to_surface))
            to_surface = NULL;
        }

      if (from_surface || to_surface)
        {
          _gtk_widget_synthesize_crossing ((from_surface) ? from : NULL,
                                           (to_surface) ? to : NULL,
                                           device, mode);

          if (from_surface)
            info->notified_surfaces = g_list_prepend (info->notified_surfaces, from_surface);

          if (to_surface)
            info->notified_surfaces = g_list_prepend (info->notified_surfaces, to_surface);
        }

      devices = devices->next;
    }
}

static void
gtk_grab_notify_foreach (GtkWidget *child,
                         gpointer   data)
{
  GrabNotifyInfo *info = data;
  gboolean was_grabbed, is_grabbed, was_shadowed, is_shadowed;
  GList *devices;

  was_grabbed = info->was_grabbed;
  is_grabbed = info->is_grabbed;

  info->was_grabbed = info->was_grabbed || (child == info->old_grab_widget);
  info->is_grabbed = info->is_grabbed || (child == info->new_grab_widget);

  was_shadowed = info->old_grab_widget && !info->was_grabbed;
  is_shadowed = info->new_grab_widget && !info->is_grabbed;

  g_object_ref (child);

  if (was_shadowed || is_shadowed)
    {
      GtkWidget *p;

      for (p = _gtk_widget_get_first_child (child);
           p != NULL;
           p = _gtk_widget_get_next_sibling (p))
        {
          gtk_grab_notify_foreach (p, info);
        }
    }

  if (info->device &&
      _gtk_widget_get_device_surface (child, info->device))
    {
      /* Device specified and is on widget */
      devices = g_list_prepend (NULL, info->device);
    }
  else
    devices = _gtk_widget_list_devices (child);

  if (is_shadowed)
    {
      _gtk_widget_set_shadowed (child, TRUE);
      if (!was_shadowed && devices &&
          gtk_widget_is_sensitive (child))
        synth_crossing_for_grab_notify (child, info->new_grab_widget,
                                        info, devices,
                                        GDK_CROSSING_GTK_GRAB);
    }
  else
    {
      _gtk_widget_set_shadowed (child, FALSE);
      if (was_shadowed && devices &&
          gtk_widget_is_sensitive (child))
        synth_crossing_for_grab_notify (info->old_grab_widget, child,
                                        info, devices,
                                        info->from_grab ? GDK_CROSSING_GTK_GRAB :
                                        GDK_CROSSING_GTK_UNGRAB);
    }

  if (was_shadowed != is_shadowed)
    _gtk_widget_grab_notify (child, was_shadowed);

  g_object_unref (child);
  g_list_free (devices);

  info->was_grabbed = was_grabbed;
  info->is_grabbed = is_grabbed;
}

static void
gtk_grab_notify (GtkWindowGroup *group,
                 GdkDevice      *device,
                 GtkWidget      *old_grab_widget,
                 GtkWidget      *new_grab_widget,
                 gboolean        from_grab)
{
  GList *toplevels;
  GrabNotifyInfo info = { 0 };

  if (old_grab_widget == new_grab_widget)
    return;

  info.old_grab_widget = old_grab_widget;
  info.new_grab_widget = new_grab_widget;
  info.from_grab = from_grab;
  info.device = device;

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

  g_list_free (info.notified_surfaces);
  g_object_unref (group);
}

/**
 * gtk_grab_add: (method)
 * @widget: The widget that grabs keyboard and pointer events
 *
 * Makes @widget the current grabbed widget.
 *
 * This means that interaction with other widgets in the same
 * application is blocked and mouse as well as keyboard events
 * are delivered to this widget.
 *
 * If @widget is not sensitive, it is not set as the current
 * grabbed widget and this function does nothing.
 */
void
gtk_grab_add (GtkWidget *widget)
{
  GtkWindowGroup *group;
  GtkWidget *old_grab_widget;

  g_return_if_fail (widget != NULL);

  if (!gtk_widget_has_grab (widget) && gtk_widget_is_sensitive (widget))
    {
      _gtk_widget_set_has_grab (widget, TRUE);

      group = gtk_main_get_window_group (widget);

      old_grab_widget = gtk_window_group_get_current_grab (group);

      g_object_ref (widget);
      _gtk_window_group_add_grab (group, widget);

      gtk_grab_notify (group, NULL, old_grab_widget, widget, TRUE);
    }
}

/**
 * gtk_grab_get_current:
 *
 * Queries the current grab of the default window group.
 *
 * Returns: (transfer none) (nullable): The widget which currently
 *     has the grab or %NULL if no grab is active
 */
GtkWidget*
gtk_grab_get_current (void)
{
  GtkWindowGroup *group;

  group = gtk_main_get_window_group (NULL);

  return gtk_window_group_get_current_grab (group);
}

/**
 * gtk_grab_remove: (method)
 * @widget: The widget which gives up the grab
 *
 * Removes the grab from the given widget.
 *
 * You have to pair calls to gtk_grab_add() and gtk_grab_remove().
 *
 * If @widget does not have the grab, this function does nothing.
 */
void
gtk_grab_remove (GtkWidget *widget)
{
  GtkWindowGroup *group;
  GtkWidget *new_grab_widget;

  g_return_if_fail (widget != NULL);

  if (gtk_widget_has_grab (widget))
    {
      _gtk_widget_set_has_grab (widget, FALSE);

      group = gtk_main_get_window_group (widget);
      _gtk_window_group_remove_grab (group, widget);
      new_grab_widget = gtk_window_group_get_current_grab (group);

      gtk_grab_notify (group, NULL, widget, new_grab_widget, FALSE);

      g_object_unref (widget);
    }
}

/**
 * gtk_device_grab_add:
 * @widget: a #GtkWidget
 * @device: a #GdkDevice to grab on.
 * @block_others: %TRUE to prevent other devices to interact with @widget.
 *
 * Adds a GTK grab on @device, so all the events on @device and its
 * associated pointer or keyboard (if any) are delivered to @widget.
 * If the @block_others parameter is %TRUE, any other devices will be
 * unable to interact with @widget during the grab.
 */
void
gtk_device_grab_add (GtkWidget *widget,
                     GdkDevice *device,
                     gboolean   block_others)
{
  GtkWindowGroup *group;
  GtkWidget *old_grab_widget;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GDK_IS_DEVICE (device));

  group = gtk_main_get_window_group (widget);
  old_grab_widget = gtk_window_group_get_current_device_grab (group, device);

  if (old_grab_widget != widget)
    _gtk_window_group_add_device_grab (group, widget, device, block_others);

  gtk_grab_notify (group, device, old_grab_widget, widget, TRUE);
}

/**
 * gtk_device_grab_remove:
 * @widget: a #GtkWidget
 * @device: a #GdkDevice
 *
 * Removes a device grab from the given widget.
 *
 * You have to pair calls to gtk_device_grab_add() and
 * gtk_device_grab_remove().
 */
void
gtk_device_grab_remove (GtkWidget *widget,
                        GdkDevice *device)
{
  GtkWindowGroup *group;
  GtkWidget *new_grab_widget;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (GDK_IS_DEVICE (device));

  group = gtk_main_get_window_group (widget);
  _gtk_window_group_remove_device_grab (group, widget, device);
  new_grab_widget = gtk_window_group_get_current_device_grab (group, device);

  gtk_grab_notify (group, device, widget, new_grab_widget, FALSE);
}

/**
 * gtk_get_current_event:
 *
 * Obtains a reference of the event currently being processed by GTK.
 *
 * For example, if you are handling a #GtkButton::clicked signal,
 * the current event will be the #GdkEventButton that triggered
 * the ::clicked signal.
 *
 * Returns: (transfer full) (nullable): a reference of the current event, or
 *     %NULL if there is no current event. The returned event must be
 *     freed with g_object_unref().
 */
GdkEvent*
gtk_get_current_event (void)
{
  if (current_events)
    return g_object_ref (current_events->data);
  else
    return NULL;
}

/**
 * gtk_get_current_event_time:
 *
 * If there is a current event and it has a timestamp,
 * return that timestamp, otherwise return %GDK_CURRENT_TIME.
 *
 * Returns: the timestamp from the current event,
 *     or %GDK_CURRENT_TIME.
 */
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
 * @state: (out): a location to store the state of the current event
 *
 * If there is a current event and it has a state field, place
 * that state field in @state and return %TRUE, otherwise return
 * %FALSE.
 *
 * Returns: %TRUE if there was a current event and it
 *     had a state field
 */
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
 * gtk_get_current_event_device:
 *
 * If there is a current event and it has a device, return that
 * device, otherwise return %NULL.
 *
 * Returns: (transfer none) (nullable): a #GdkDevice, or %NULL
 */
GdkDevice *
gtk_get_current_event_device (void)
{
  if (current_events)
    return gdk_event_get_device (current_events->data);
  else
    return NULL;
}

/**
 * gtk_get_event_widget:
 * @event: a #GdkEvent
 *
 * If @event is %NULL or the event was not associated with any widget,
 * returns %NULL, otherwise returns the widget that received the event
 * originally.
 *
 * Returns: (transfer none) (nullable): the widget that originally
 *     received @event, or %NULL
 */
GtkWidget*
gtk_get_event_widget (const GdkEvent *event)
{
  GtkWidget *widget;

  widget = NULL;
  if (event && event->any.surface &&
      (event->any.type == GDK_DESTROY || !gdk_surface_is_destroyed (event->any.surface)))
    widget = gtk_native_get_for_surface (event->any.surface);

  return widget;
}

/**
 * gtk_get_event_target:
 * @event: a #GdkEvent
 *
 * If @event is %NULL or the event was not associated with any widget,
 * returns %NULL, otherwise returns the widget that is the deepmost
 * receiver of the event.
 *
 * Returns: (transfer none) (nullable): the target widget, or %NULL
 */
GtkWidget *
gtk_get_event_target (const GdkEvent *event)
{
  return GTK_WIDGET (gdk_event_get_target (event));
}

/**
 * gtk_get_event_target_with_type:
 * @event: a #GdkEvent
 * @type: the type to look for
 *
 * If @event is %NULL or the event was not associated with any widget,
 * returns %NULL, otherwise returns first widget found from the event
 * target to the toplevel that matches @type.
 *
 * Returns: (transfer none) (nullable): the widget in the target stack
 * with the given type, or %NULL
 */
GtkWidget *
gtk_get_event_target_with_type (GdkEvent *event,
                                GType     type)
{
  GtkWidget *target;

  target = gtk_get_event_target (event);
  while (target && !g_type_is_a (G_OBJECT_TYPE (target), type))
    target = gtk_widget_get_parent (target);

  return target;
}

static gboolean
propagate_event_up (GtkWidget *widget,
                    GdkEvent  *event,
                    GtkWidget *topmost)
{
  gboolean handled_event = FALSE;

  /* Propagate event up the widget tree so that
   * parents can see the button and motion
   * events of the children.
   */
  while (TRUE)
    {
      GtkWidget *tmp;

      g_object_ref (widget);

      /* Scroll events are special cased here because it
       * feels wrong when scrolling a GtkViewport, say,
       * to have children of the viewport eat the scroll
       * event
       */
      if (!gtk_widget_is_sensitive (widget))
        handled_event = event->any.type != GDK_SCROLL;
      else if (gtk_widget_get_realized (widget))
        handled_event = gtk_widget_event (widget, event);

      handled_event |= !gtk_widget_get_realized (widget);

      tmp = gtk_widget_get_parent (widget);
      g_object_unref (widget);

      if (widget == topmost)
        break;

      widget = tmp;

      if (handled_event || !widget)
        break;
    }

  return handled_event;
}

static gboolean
propagate_event_down (GtkWidget *widget,
                      GdkEvent  *event,
                      GtkWidget *topmost)
{
  gint handled_event = FALSE;
  GList *widgets = NULL;
  GList *l;

  widgets = g_list_prepend (widgets, g_object_ref (widget));
  while (widget && widget != topmost)
    {
      widget = gtk_widget_get_parent (widget);
      if (!widget)
        break;

      widgets = g_list_prepend (widgets, g_object_ref (widget));

      if (widget == topmost)
        break;
    }

  for (l = widgets; l && !handled_event; l = l->next)
    {
      widget = (GtkWidget *)l->data;

      if (!gtk_widget_is_sensitive (widget))
        {
          /* stop propagating on SCROLL, but don't handle the event, so it
           * can propagate up again and reach its handling widget
           */
          if (event->any.type == GDK_SCROLL)
            break;
          else
            handled_event = TRUE;
        }
      else if (gtk_widget_get_realized (widget))
        handled_event = _gtk_widget_captured_event (widget, event);

      handled_event |= !gtk_widget_get_realized (widget);
    }
  g_list_free_full (widgets, (GDestroyNotify)g_object_unref);

  return handled_event;
}

void
gtk_propagate_event_internal (GtkWidget *widget,
                              GdkEvent  *event,
                              GtkWidget *topmost)
{
  /* Propagate the event down and up */
  if (!propagate_event_down (widget, event, topmost))
    propagate_event_up (widget, event, topmost);
}

/**
 * gtk_propagate_event:
 * @widget: a #GtkWidget
 * @event: an event
 *
 * Sends an event to a widget, propagating the event to parent widgets
 * if the event remains unhandled. This function will emit the event
 * through all the hierarchy of @widget through all propagation phases.
 *
 * Events received by GTK from GDK normally begin in gtk_main_do_event().
 * Depending on the type of event, existence of modal dialogs, grabs, etc.,
 * the event may be propagated; if so, this function is used.
 *
 * gtk_propagate_event() calls gtk_widget_event() on each widget it
 * decides to send the event to. So gtk_widget_event() is the lowest-level
 * function; it simply emits the #GtkWidget::event and possibly an
 * event-specific signal on a widget. gtk_propagate_event() is a bit
 * higher-level, and gtk_main_do_event() is the highest level.
 *
 * All that said, you most likely don’t want to use any of these
 * functions; synthesizing events is rarely needed. There are almost
 * certainly better ways to achieve your goals. For example, use
 * gtk_widget_queue_draw() instead
 * of making up expose events.
 */
void
gtk_propagate_event (GtkWidget *widget,
                     GdkEvent  *event)
{
  GtkWindowGroup *window_group;
  GtkWidget *event_widget, *topmost = NULL;
  GdkDevice *device;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (event != NULL);

  event_widget = gtk_get_event_widget (event);
  window_group = gtk_main_get_window_group (event_widget);
  device = gdk_event_get_device (event);

  /* check whether there is a (device) grab in effect... */
  if (device)
    topmost = gtk_window_group_get_current_device_grab (window_group, device);
  if (!topmost)
    topmost = gtk_window_group_get_current_grab (window_group);

  gtk_propagate_event_internal (widget, event, topmost);
}
