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
 *   mainwin = gtk_window_new ();
 *
 *   // Set up our GUI elements
 *
 *   // ...
 *
 *   // Show the application window
 *   gtk_widget_show (mainwin);
 *
 *   // Enter the main event loop, and wait for user interaction
 *   while (!done)
 *     g_main_context_iteration (NULL, TRUE);
 *
 *   // The user lost interest
 *   return 0;
 * }
 * ]|
 *
 * See #GMainLoop in the GLib documentation to learn more about
 * main loops and their features.
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
#include "gtkdropprivate.h"
#include "gtkmain.h"
#include "gtkmediafileprivate.h"
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

static gint pre_initialized = FALSE;
static gint gtk_initialized = FALSE;
static GList *current_events = NULL;

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
  { "interactive", GTK_DEBUG_INTERACTIVE },
  { "touchscreen", GTK_DEBUG_TOUCHSCREEN },
  { "actions", GTK_DEBUG_ACTIONS },
  { "resize", GTK_DEBUG_RESIZE },
  { "layout", GTK_DEBUG_LAYOUT },
  { "snapshot", GTK_DEBUG_SNAPSHOT },
  { "constraints", GTK_DEBUG_CONSTRAINTS },
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

  env_string = g_getenv ("GTK_DEBUG");
  if (env_string != NULL)
    {
#ifdef G_ENABLE_DEBUG
      debug_flags[0].flags = g_parse_debug_string (env_string,
                                                   gtk_debug_keys,
                                                   G_N_ELEMENTS (gtk_debug_keys));
      any_display_debug_flags_set = debug_flags[0].flags > 0;
#else
      g_warning ("GTK_DEBUG set but ignored because gtk isn't built with G_ENABLE_DEBUG");
#endif  /* G_ENABLE_DEBUG */
      env_string = NULL;
    }

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

  if (display == NULL)
    display = gdk_display_get_default ();

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
  if (gtk_get_any_display_debug_flag_set ())
    return gtk_get_display_debug_flags (gdk_display_get_default ());

  return 0;
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

typedef struct {
  GMainLoop *store_loop;
  guint n_clipboards;
  guint timeout_id;
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

static gboolean
sync_timed_out_cb (ClipboardStore *store)
{
  store->timeout_id = 0;
  g_main_loop_quit (store->store_loop);
  return G_SOURCE_REMOVE;
}

void
gtk_main_sync (void)
{
  ClipboardStore store = { NULL, };
  GSList *displays, *l;
  GCancellable *cancel;
  
  /* Try storing all clipboard data we have */
  displays = gdk_display_manager_list_displays (gdk_display_manager_get ());
  if (displays == NULL)
    return;

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
  store.timeout_id = g_timeout_add_seconds (10, (GSourceFunc) sync_timed_out_cb, &store);
  g_source_set_name_by_id (store.timeout_id, "[gtk] gtk_main_sync clipboard store timeout");

  if (g_main_loop_is_running (store.store_loop))
    g_main_loop_run (store.store_loop);
  
  g_cancellable_cancel (cancel);
  g_object_unref (cancel);
  g_clear_handle_id (&store.timeout_id, g_source_remove);
  g_clear_pointer (&store.store_loop, g_main_loop_unref);
  
  /* Synchronize the recent manager singleton */
  _gtk_recent_manager_sync ();
}

static GdkEvent *
rewrite_event_for_surface (GdkEvent  *event,
			   GdkSurface *new_surface)
{
  GdkEventType type;
  double x, y;
  double dx, dy;

  type = gdk_event_get_event_type (event);

  switch ((guint) type)
    {
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
    case GDK_MOTION_NOTIFY:
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
    case GDK_TOUCHPAD_SWIPE:
    case GDK_TOUCHPAD_PINCH:
      gdk_event_get_position (event, &x, &y);
      gdk_surface_translate_coordinates (gdk_event_get_surface (event), new_surface, &x, &y);
      break;
    default:
      x = y = 0;
      break;
    }

  switch ((guint) type)
    {
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      return gdk_event_button_new (type,
                                   new_surface,
                                   gdk_event_get_device (event),
                                   gdk_event_get_source_device (event),
                                   gdk_event_get_device_tool (event),
                                   gdk_event_get_time (event),
                                   gdk_event_get_modifier_state (event),
                                   gdk_button_event_get_button (event),
                                   x, y,
                                   NULL); // FIXME copy axes
    case GDK_MOTION_NOTIFY:
      return gdk_event_motion_new (new_surface,
                                   gdk_event_get_device (event),
                                   gdk_event_get_source_device (event),
                                   gdk_event_get_device_tool (event),
                                   gdk_event_get_time (event),
                                   gdk_event_get_modifier_state (event),
                                   x, y,
                                   NULL); // FIXME copy axes
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
      return gdk_event_touch_new (type,
                                  gdk_event_get_event_sequence (event),
                                  new_surface,
                                  gdk_event_get_device (event),
                                  gdk_event_get_source_device (event),
                                  gdk_event_get_time (event),
                                  gdk_event_get_modifier_state (event),
                                  x, y,
                                  NULL, // FIXME copy axes
                                  gdk_touch_event_get_emulating_pointer (event));
    case GDK_TOUCHPAD_SWIPE:
      gdk_touchpad_event_get_deltas (event, &dx, &dy);
      return gdk_event_touchpad_swipe_new (new_surface,
                                           gdk_event_get_device (event),
                                           gdk_event_get_source_device (event),
                                           gdk_event_get_time (event),
                                           gdk_event_get_modifier_state (event),
                                           gdk_touchpad_event_get_gesture_phase (event),
                                           x, y,
                                           gdk_touchpad_event_get_n_fingers (event),
                                           dx, dy);
    case GDK_TOUCHPAD_PINCH:
      gdk_touchpad_event_get_deltas (event, &dx, &dy);
      return gdk_event_touchpad_pinch_new (new_surface,
                                           gdk_event_get_device (event),
                                           gdk_event_get_source_device (event),
                                           gdk_event_get_time (event),
                                           gdk_event_get_modifier_state (event),
                                           gdk_touchpad_event_get_gesture_phase (event),
                                           x, y,
                                           gdk_touchpad_event_get_n_fingers (event),
                                           dx, dy,
                                           gdk_touchpad_pinch_event_get_scale (event),
                                           gdk_touchpad_pinch_event_get_angle_delta (event));
    default:
      break;
    }

  return NULL;
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

  switch ((guint) gdk_event_get_event_type (event))
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
      display = gdk_event_get_display (event);
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

static gboolean
translate_event_coordinates (GdkEvent  *event,
                             double    *x,
                             double    *y,
                             GtkWidget *widget)
{
  GtkWidget *event_widget;
  graphene_point_t p;
  double event_x, event_y;

  *x = *y = 0;

  if (!gdk_event_get_position (event, &event_x, &event_y))
    return FALSE;

  event_widget = gtk_get_event_widget (event);

  if (!gtk_widget_compute_point (event_widget,
                                 widget,
                                 &GRAPHENE_POINT_INIT (event_x, event_y),
                                 &p))
    return FALSE;

  *x = p.x;
  *y = p.y;

  return TRUE;
}

static void
gtk_synthesize_crossing_events (GtkRoot         *toplevel,
                                GtkCrossingType  crossing_type,
                                GtkWidget       *old_target,
                                GtkWidget       *new_target,
                                GdkEvent        *event,
                                GdkCrossingMode  mode,
                                GdkDrop         *drop)
{
  GtkCrossingData crossing;
  GtkWidget *ancestor;
  GtkWidget *widget;
  GList *list, *l;
  double x, y;
  GtkWidget *prev;
  gboolean seen_ancestor;

  if (old_target == new_target)
    return;

  if (old_target && new_target)
    ancestor = gtk_widget_common_ancestor (old_target, new_target);
  else
    ancestor = NULL;

  crossing.type = crossing_type;
  crossing.mode = mode;
  crossing.old_target = old_target;
  crossing.old_descendent = NULL;
  crossing.new_target = new_target;
  crossing.new_descendent = NULL;
  crossing.drop = drop;

  crossing.direction = GTK_CROSSING_OUT;

  prev = NULL;
  seen_ancestor = FALSE;
  widget = old_target;
  while (widget)
    {
      crossing.old_descendent = prev;
      if (seen_ancestor)
        {
          crossing.new_descendent = new_target ? prev : NULL;
        }
      else if (widget == ancestor)
        {
          GtkWidget *w;

          crossing.new_descendent = NULL;
          for (w = new_target; w != ancestor; w = gtk_widget_get_parent (w))
            crossing.new_descendent = w;

          seen_ancestor = TRUE;
        }
      else
        {
          crossing.new_descendent = NULL;
        }
      check_crossing_invariants (widget, &crossing);
      translate_event_coordinates (event, &x, &y, widget);
      gtk_widget_handle_crossing (widget, &crossing, x, y);
      if (crossing_type == GTK_CROSSING_POINTER)
        gtk_widget_unset_state_flags (widget, GTK_STATE_FLAG_PRELIGHT);
      widget = gtk_widget_get_parent (widget);
    }

  list = NULL;
  for (widget = new_target; widget; widget = gtk_widget_get_parent (widget))
    list = g_list_prepend (list, widget);

  crossing.direction = GTK_CROSSING_IN;

  seen_ancestor = FALSE;
  for (l = list; l; l = l->next)
    {
      widget = l->data;
      if (l->next)
        crossing.new_descendent = l->next->data;
      else
        crossing.new_descendent = NULL;
      if (seen_ancestor)
        {
          crossing.old_descendent = NULL;
        }
      else if (widget == ancestor)
        {
          GtkWidget *w;

          crossing.old_descendent = NULL;
          for (w = old_target; w != ancestor; w = gtk_widget_get_parent (w))
            crossing.old_descendent = w;

          seen_ancestor = TRUE;
        }
      else
        {
          crossing.old_descendent = old_target ? crossing.new_descendent : NULL;
        }

      translate_event_coordinates (event, &x, &y, widget);
      gtk_widget_handle_crossing (widget, &crossing, x, y);
      if (crossing_type == GTK_CROSSING_POINTER)
        gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_PRELIGHT, FALSE);
    }

  g_list_free (list);
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

  gdk_event_get_position (event, &x, &y);
  gtk_window_update_pointer_focus (toplevel, device, sequence,
                                   new_target, x, y);

  return old_target;
}

static gboolean
is_pointing_event (GdkEvent *event)
{
  switch ((guint) gdk_event_get_event_type (event))
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
    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DROP_START:
      return TRUE;

    default:
      return FALSE;
    }
}

static gboolean
is_key_event (GdkEvent *event)
{
  switch ((guint) gdk_event_get_event_type (event))
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
  switch ((guint) gdk_event_get_event_type (event))
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
  GdkEventType type;

  event_widget = gtk_get_event_widget (event);
  device = gdk_event_get_device (event);
  gdk_event_get_position (event, &x, &y);

  toplevel = GTK_WINDOW (gtk_widget_get_root (event_widget));
  native = GTK_WIDGET (gtk_widget_get_native (event_widget));

  type = gdk_event_get_event_type (event);
  sequence = gdk_event_get_event_sequence (event);

  switch ((guint) type)
    {
    case GDK_LEAVE_NOTIFY:
      if (gdk_crossing_event_get_mode (event) == GDK_CROSSING_NORMAL &&
          gtk_window_lookup_pointer_focus_implicit_grab (toplevel, device, NULL))
        {
          /* We have an implicit grab, wait for the corresponding
           * GDK_CROSSING_UNGRAB.
           */
          break;
        }
      G_GNUC_FALLTHROUGH;
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
      old_target = update_pointer_focus_state (toplevel, event, NULL);
      if (type == GDK_LEAVE_NOTIFY)
        gtk_synthesize_crossing_events (GTK_ROOT (toplevel), GTK_CROSSING_POINTER, old_target, NULL,
                                        event, gdk_crossing_event_get_mode (event), NULL);
      break;
    case GDK_DRAG_LEAVE:
      {
        GdkDrop *drop = gdk_drag_event_get_drop (event);
        old_target = update_pointer_focus_state (toplevel, event, NULL);
        gtk_drop_begin_event (drop, GDK_DRAG_LEAVE);
        gtk_synthesize_crossing_events (GTK_ROOT (toplevel), GTK_CROSSING_DROP, old_target, NULL,
                                        event, GDK_CROSSING_NORMAL, drop);
        gtk_drop_end_event (drop);
      }
      break;
    case GDK_ENTER_NOTIFY:
      if (gdk_crossing_event_get_mode (event) == GDK_CROSSING_GRAB ||
          gdk_crossing_event_get_mode (event) == GDK_CROSSING_UNGRAB)
        break;
      G_GNUC_FALLTHROUGH;
    case GDK_DRAG_ENTER:
    case GDK_DRAG_MOTION:
    case GDK_DROP_START:
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_MOTION_NOTIFY:
      target = gtk_window_lookup_pointer_focus_implicit_grab (toplevel, device, sequence);

      if (!target)
       target = gtk_widget_pick (native, x, y, GTK_PICK_DEFAULT);

      if (!target)
        target = GTK_WIDGET (native);

      old_target = update_pointer_focus_state (toplevel, event, target);

      if (type == GDK_MOTION_NOTIFY || type == GDK_ENTER_NOTIFY)
        {
          if (!gtk_window_lookup_pointer_focus_implicit_grab (toplevel, device,
                                                              sequence))
            {
              gtk_synthesize_crossing_events (GTK_ROOT (toplevel), GTK_CROSSING_POINTER, old_target, target,
                                              event, GDK_CROSSING_NORMAL, NULL);
            }

          gtk_window_maybe_update_cursor (toplevel, NULL, device);
        }
      else if ((old_target != target) &&
               (type == GDK_DRAG_ENTER || type == GDK_DRAG_MOTION || type == GDK_DROP_START))
        {
          GdkDrop *drop = gdk_drag_event_get_drop (event);
          gtk_drop_begin_event (drop, type);
          gtk_synthesize_crossing_events (GTK_ROOT (toplevel), GTK_CROSSING_DROP, old_target, target,
                                          event, GDK_CROSSING_NORMAL, gdk_drag_event_get_drop (event));
          gtk_drop_end_event (drop);
        }
      else if (type == GDK_TOUCH_BEGIN)
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
                                         type == GDK_BUTTON_PRESS ?
                                         target : NULL);

      if (type == GDK_BUTTON_RELEASE)
        {
          GtkWidget *new_target;
          new_target = gtk_widget_pick (GTK_WIDGET (native), x, y, GTK_PICK_DEFAULT);
          if (new_target == NULL)
            new_target = GTK_WIDGET (toplevel);
          gtk_synthesize_crossing_events (GTK_ROOT (toplevel), GTK_CROSSING_POINTER, target, new_target,
                                          event, GDK_CROSSING_UNGRAB, NULL);
          gtk_window_maybe_update_cursor (toplevel, NULL, device);
        }

      set_widget_active_state (target, type == GDK_BUTTON_RELEASE);

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

void
gtk_main_do_event (GdkEvent *event)
{
  GtkWidget *event_widget;
  GtkWidget *target_widget;
  GtkWidget *grab_widget = NULL;
  GtkWindowGroup *window_group;
  GdkEvent *rewritten_event = NULL;
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
    {
      target_widget = handle_pointing_event (event);
    }
  else if (is_key_event (event))
    {
      target_widget = handle_key_event (event);
    }
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

  window_group = gtk_main_get_window_group (target_widget);

  /* check whether there is a grab in effect... */
  grab_widget = gtk_window_group_get_current_grab (window_group);

  /* If the grab widget is an ancestor of the event widget
   * then we send the event to the original event widget.
   * This is the key to implementing modality.
   */
  if (!grab_widget ||
      ((gtk_widget_is_sensitive (target_widget) || gdk_event_get_event_type (event) == GDK_SCROLL) &&
       gtk_widget_is_ancestor (target_widget, grab_widget)))
    grab_widget = target_widget;

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
  switch ((guint)gdk_event_get_event_type (event))
    {
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

    case GDK_FOCUS_CHANGE:
    case GDK_GRAB_BROKEN:
      if (!_gtk_widget_captured_event (target_widget, event, target_widget))
        gtk_widget_event (target_widget, event, target_widget);
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
    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
      /* Crossing event propagation happens during picking */
      break;

    case GDK_DRAG_MOTION:
    case GDK_DROP_START:
      {
        GdkDrop *drop = gdk_drag_event_get_drop (event);
        gtk_drop_begin_event (drop, gdk_event_get_event_type (event));
        gtk_propagate_event (target_widget, event);
        gtk_drop_end_event (drop);
      }
      break;

    case GDK_EVENT_LAST:
    default:
      g_assert_not_reached ();
      break;
    }

  _gtk_tooltip_handle_event (target_widget, event);

 cleanup:
  tmp_list = current_events;
  current_events = g_list_remove_link (current_events, tmp_list);
  g_list_free_1 (tmp_list);

  if (rewritten_event)
    gdk_event_unref (rewritten_event);
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
    return gdk_event_ref (current_events->data);
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
    {
      *state = gdk_event_get_modifier_state (current_events->data);
      return TRUE;
    }
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
GtkWidget *
gtk_get_event_widget (GdkEvent *event)
{
  GdkSurface *surface;

  surface = gdk_event_get_surface (event);
  if (surface && !gdk_surface_is_destroyed (surface))
    return gtk_native_get_for_surface (surface);

  return NULL;
}

static gboolean
propagate_event_up (GtkWidget *widget,
                    GdkEvent  *event,
                    GtkWidget *topmost)
{
  gboolean handled_event = FALSE;
  GtkWidget *target = widget;

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
        handled_event = gdk_event_get_event_type (event) != GDK_SCROLL;
      else if (gtk_widget_get_realized (widget))
        handled_event = gtk_widget_event (widget, event, target);

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
  GtkWidget *target = widget;

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
          if (gdk_event_get_event_type (event) == GDK_SCROLL)
            break;
          else
            handled_event = TRUE;
        }
      else if (gtk_widget_get_realized (widget))
        handled_event = _gtk_widget_captured_event (widget, event, target);

      handled_event |= !gtk_widget_get_realized (widget);
    }
  g_list_free_full (widgets, (GDestroyNotify)g_object_unref);

  return handled_event;
}

gboolean
gtk_propagate_event_internal (GtkWidget *widget,
                              GdkEvent  *event,
                              GtkWidget *topmost)
{
  /* Propagate the event down and up */
  if (propagate_event_down (widget, event, topmost))
    return TRUE;

  if (propagate_event_up (widget, event, topmost))
    return TRUE;

  return FALSE;
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
 * Events received by GTK from GDK normally begin at a #GtkRoot widget.
 * Depending on the type of event, existence of modal dialogs, grabs, etc.,
 * the event may be propagated; if so, this function is used.
 *
 * All that said, you most likely don’t want to use any of these
 * functions; synthesizing events is rarely needed. There are almost
 * certainly better ways to achieve your goals. For example, use
 * gtk_widget_queue_draw() instead
 * of making up expose events.
 *
 * Returns: %TRUE if the event was handled
 */
gboolean
gtk_propagate_event (GtkWidget *widget,
                     GdkEvent  *event)
{
  GtkWindowGroup *window_group;
  GtkWidget *event_widget, *topmost = NULL;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  event_widget = gtk_get_event_widget (event);
  window_group = gtk_main_get_window_group (event_widget);

  /* check whether there is a grab in effect... */
  topmost = gtk_window_group_get_current_grab (window_group);

  return gtk_propagate_event_internal (widget, event, topmost);
}
