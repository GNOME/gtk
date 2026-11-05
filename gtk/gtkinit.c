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

#include "config.h"

#include "gdk/gdk.h"
#include "gdk/gdkdisplayprivate.h"
#include "gdk/gdkprofilerprivate.h"
#include "gdk/gdkdebugprivate.h"
#include "gsk/gskprivate.h"
#include "gsk/gskrendernodeprivate.h"
#include "gtkdebugprivate.h"
#include "gtknative.h"
#include "gtkinit.h"

#include <locale.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <sys/types.h>          /* For uid_t, gid_t */

#ifdef G_OS_WIN32
#include <windows.h>
#endif

#include <hb-glib.h>

#include <glib/gi18n-lib.h>

#include "gtkdebug.h"
#include "gtkdropprivate.h"
#include "gtkmain.h"
#include "gtkmediafileprivate.h"
#include "gtkmodulesprivate.h"
#include "gtkprivate.h"
#include "gtkrecentmanager.h"
#include "gtktooltipprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkwindowprivate.h"
#include "gtkwindowgroup.h"
#include "print/gtkprintbackendprivate.h"
#include "gtkimmoduleprivate.h"
#include "gtkroot.h"
#include "gtknative.h"
#include "gtkpopcountprivate.h"

#include "inspector/init.h"
#include "inspector/window.h"

#include "gdk/gdkeventsprivate.h"
#include "gdk/gdksurfaceprivate.h"

#ifdef GDK_WINDOWING_WAYLAND
#include "gdk/wayland/gdkwaylandprivate.h"
#endif

static int pre_initialized = FALSE;
static int gtk_initialized = FALSE;

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
 * Prevents [func@Gtk.init] and [func@Gtk.init_check] from calling `setlocale()`.
 *
 * You would want to use this function if you wanted to set the locale for
 * your program to something other than the user’s locale, or if you wanted
 * to set different values for different locale categories.
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

/**
 * gtk_disable_portals:
 *
 * Prevents GTK from using portals.
 *
 * This is equivalent to setting `GDK_DEBUG=no-portals` in the environment.
 *
 * This should only be used in portal implementations, apps must not call it.
 *
 * Since: 4.18
 */
void
gtk_disable_portals (void)
{
  if (pre_initialized)
    g_warning ("gtk_disable_portals() must be called before gtk_init()");

  gdk_disable_all_portals ();
}


/**
 * gtk_disable_portal_interfaces:
 * @portal_interfaces: (array zero-terminated=1) (not nullable):
 *     a %NULL-terminated array of portal interface names to disable
 *
 * Prevents GTK from using the specified portals.
 *
 * This should only be used in portal implementations, apps must not call it.
 *
 * Since: 4.22
 */
void
gtk_disable_portal_interfaces (const char **portal_interfaces)
{
  if (pre_initialized)
    g_warning ("gtk_disable_portal_interfaces() must be called before gtk_init()");

  gdk_disable_portals (portal_interfaces);
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
enum_locale_proc (LPSTR locale)
{
  LCID lcid;
  char iso639[10];
  char iso3166[10];
  char *endptr;


  lcid = strtoul (locale, &endptr, 16);
  if (*endptr == '\0' &&
      GetLocaleInfoA (lcid, LOCALE_SISO639LANGNAME, iso639, sizeof (iso639)) &&
      GetLocaleInfoA (lcid, LOCALE_SISO3166CTRYNAME, iso3166, sizeof (iso3166)))
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

          if (GetLocaleInfoA (lcid, LOCALE_SENGLANGUAGE, language, sizeof (language)) &&
              GetLocaleInfoA (lcid, LOCALE_SENGCOUNTRY, country, sizeof (country)))
            {
              char str[300];

              g_snprintf (str, sizeof (str), "%s_%s", language, country);

              if (setlocale (LC_ALL, str) != NULL)
                setlocale_called = TRUE;
            }

          return FALSE;
        }
    }

  return TRUE;
}

#endif

void
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

              EnumSystemLocalesA (enum_locale_proc, LCID_SUPPORTED);
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

#ifdef GDK_WINDOWING_WAYLAND
static void
prepare_wayland_socket (void)
{
  char *wayland_socket;

  wayland_socket = getenv ("WAYLAND_SOCKET");
  if (wayland_socket && *wayland_socket)
    {
      char *end;
      int wayland_socket_fd;

      errno = 0;
      wayland_socket_fd = strtol (wayland_socket, &end, 10);
      if (errno != 0 || wayland_socket == end || *end != '\0')
        {
          g_warning ("Invalid WAYLAND_SOCKET");
          return;
        }

      unsetenv ("WAYLAND_SOCKET");
      gdk_wayland_set_wayland_socket (wayland_socket_fd);
    }
}
#endif

static void
do_pre_parse_initialization (void)
{
  const char *env_string;
  double slowdown;

  if (pre_initialized)
    return;

  pre_initialized = TRUE;

  if (_gtk_module_has_mixed_deps (NULL))
    g_error ("GTK 2/3 symbols detected. Using GTK 2/3 and GTK 4 in the same process is not supported");

#ifdef GDK_WINDOWING_WAYLAND
  prepare_wayland_socket ();
#endif

  gdk_pre_parse ();

  gtk_debug_init ();

  env_string = g_getenv ("GTK_SLOWDOWN");
  if (env_string)
    {
      slowdown = g_ascii_strtod (env_string, NULL);
      _gtk_set_slowdown (slowdown);
    }

  /* Trigger fontconfig initialization early */
  pango_cairo_font_map_get_default ();
}

static void
gettext_initialization (void)
{
  setlocale_initialization ();

  bindtextdomain (GETTEXT_PACKAGE, _gtk_get_localedir ());
#ifdef HAVE_BIND_TEXTDOMAIN_CODESET
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif
}

static void
default_display_notify_cb (GdkDisplayManager *dm)
{
  gtk_debug_init_display ();
}

static void
do_post_parse_initialization (void)
{
  GdkDisplayManager *display_manager;
  GtkTextDirection text_dir;
  gint64 before G_GNUC_UNUSED;

  if (gtk_initialized)
    return;

  before = GDK_PROFILER_CURRENT_TIME;

  gettext_initialization ();

#ifdef SIGPIPE
  signal (SIGPIPE, SIG_IGN);
#endif

  text_dir = gtk_get_locale_direction ();

  /* We always allow inverting the text direction, even in
   * production builds, as SDK/IDE tooling may provide the
   * ability for developers to test rtl/ltr inverted.
   */
  if (gtk_get_debug_flags () & GTK_DEBUG_INVERT_TEXT_DIR)
    text_dir = (text_dir == GTK_TEXT_DIR_LTR) ? GTK_TEXT_DIR_RTL : GTK_TEXT_DIR_LTR;

  gtk_widget_set_default_direction (text_dir);

  gdk_event_init_types ();

  gsk_ensure_resources ();
  gsk_render_node_init_types ();
  _gtk_ensure_resources ();

  gdk_profiler_end_mark (before, "Basic initialization", NULL);

  gtk_initialized = TRUE;

  before = GDK_PROFILER_CURRENT_TIME;
#ifdef G_OS_UNIX
  gtk_print_backends_init ();
#endif
  gtk_im_modules_init ();
  gtk_media_file_extension_init ();
  gdk_profiler_end_mark (before, "Init modules", NULL);

  before = GDK_PROFILER_CURRENT_TIME;
  display_manager = gdk_display_manager_get ();
  if (gdk_display_manager_get_default_display (display_manager) != NULL)
    default_display_notify_cb (display_manager);
  gdk_profiler_end_mark (before, "Create display", NULL);

  g_signal_connect (display_manager, "notify::default-display",
                    G_CALLBACK (default_display_notify_cb),
                    NULL);

  gtk_inspector_register_extension ();
}

#ifdef G_PLATFORM_WIN32
#undef gtk_init_check
#endif

/**
 * gtk_init_check:
 *
 * Initializes GTK.
 *
 * This function does the same work as [func@Gtk.init] with only a
 * single change: It does not terminate the program if the windowing
 * system can’t be initialized. Instead it returns false on failure.
 *
 * This way the application can fall back to some other means of
 * communication with the user - for example a curses or command line
 * interface.
 *
 * Returns: true if the windowing system has been successfully
 *   initialized, false otherwise
 */
gboolean
gtk_init_check (void)
{
  gboolean ret;

  if (gtk_initialized)
    return TRUE;

  if (gdk_profiler_is_running ())
    g_info ("Profiling is active");

  gettext_initialization ();

  if (!check_setugid ())
    return FALSE;

  do_pre_parse_initialization ();
  do_post_parse_initialization ();

  ret = gdk_display_open_default () != NULL;

  if (ret && (gtk_get_debug_flags () & GTK_DEBUG_INTERACTIVE))
    gtk_window_set_interactive_debugging (TRUE);

  if (ret && (gtk_get_debug_flags () & GTK_DEBUG_GENERAL_INFO))
    gtk_inspector_print_general_info (gdk_display_get_default ());

  return ret;
}

#ifdef G_PLATFORM_WIN32
#undef gtk_init
#endif

/**
 * gtk_init:
 *
 * Initializes GTK.
 *
 * This function must be called before using any other GTK functions
 * in your GUI applications.
 *
 * It will initialize everything needed to operate the toolkit. In particular,
 * it will open the default display (see [func@Gdk.Display.get_default]).
 *
 * If you are using [class@Gtk.Application], you usually don't have to call this
 * function; the [vfunc@Gio.Application.startup] handler does it for you. Though,
 * if you are using `GApplication` methods that will be invoked before `startup`,
 * such as `local_command_line`, you may need to initialize GTK explicitly.
 *
 * This function will terminate your program if it was unable to initialize
 * the windowing system for some reason. If you want your program to fall back
 * to a textual interface, call [func@Gtk.init_check] instead.
 *
 * GTK calls `signal (SIGPIPE, SIG_IGN)` during initialization, to ignore
 * SIGPIPE signals, since these are almost never wanted in graphical
 * applications. If you do need to handle SIGPIPE for some reason, reset
 * the handler after gtk_init(), but notice that other libraries (e.g.
 * libdbus or gvfs) might do similar things.
 */
void
gtk_init (void)
{
  if (!gtk_init_check ())
    {
      g_warning ("Failed to open display");
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
 * not. Unfortunately this wasn’t noticed until after GTK 2.0.1. So,
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
 * Returns whether GTK has been initialized.
 *
 * See [func@Gtk.init].
 *
 * Returns: the initialization status
 */
gboolean
gtk_is_initialized (void)
{
  return gtk_initialized;
}
