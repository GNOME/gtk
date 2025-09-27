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

#include "version/gdkversionmacros.h"

#include "gdkresources.h"

#include "gdkconstructorprivate.h"
#include "gdkdebugprivate.h"
#include "gdkcontentformatsprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkglcontextprivate.h"
#include <glib/gi18n-lib.h>
#include "gdkprivate.h"
#include <glib/gprintf.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include <fribidi.h>

/* GTK has a general architectural assumption that gsize is pointer-sized
 * (equivalent to uintptr_t), making it non-portable to architectures like
 * CHERI where that isn't true. If a future release relaxes that
 * assumption, changes will be needed in numerous places.
 * See also https://gitlab.gnome.org/GNOME/glib/-/issues/2842 for the
 * equivalent in GLib, which would be a prerequisite. */
G_STATIC_ASSERT (sizeof (gsize) == sizeof (void *));
G_STATIC_ASSERT (G_ALIGNOF (gsize) == G_ALIGNOF (void *));

/**
 * GDK_WINDOWING_X11:
 *
 * The `GDK_WINDOWING_X11` macro is defined if the X11 backend
 * is supported.
 *
 * Use this macro to guard code that is specific to the X11 backend.
 */

/**
 * GDK_WINDOWING_WIN32:
 *
 * The `GDK_WINDOWING_WIN32` macro is defined if the Win32 backend
 * is supported.
 *
 * Use this macro to guard code that is specific to the Win32 backend.
 */

/**
 * GDK_WINDOWING_MACOS:
 *
 * The `GDK_WINDOWING_MACOS` macro is defined if the MacOS backend
 * is supported.
 *
 * Use this macro to guard code that is specific to the MacOS backend.
 */

/**
 * GDK_WINDOWING_WAYLAND:
 *
 * The `GDK_WINDOWING_WAYLAND` macro is defined if the Wayland backend
 * is supported.
 *
 * Use this macro to guard code that is specific to the Wayland backend.
 */

/**
 * GDK_DISABLE_DEPRECATION_WARNINGS:
 *
 * A macro that should be defined before including the gdk.h header.
 *
 * If it is defined, no compiler warnings will be produced for uses
 * of deprecated GDK APIs.
 */

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

gboolean
gdk_is_initialized (void)
{
  return gdk_initialized != 0;
}

static const GdkDebugKey gdk_debug_keys[] = {
  { "misc",            GDK_DEBUG_MISC, "Miscellaneous information" },
  { "events",          GDK_DEBUG_EVENTS, "Information about events" },
  { "dnd",             GDK_DEBUG_DND, "Information about Drag-and-Drop" },
  { "input",           GDK_DEBUG_INPUT, "Information about input (Windows)" },
  { "eventloop",       GDK_DEBUG_EVENTLOOP, "Information about event loop operation (Quartz)" },
  { "frames",          GDK_DEBUG_FRAMES, "Information about the frame clock" },
  { "settings",        GDK_DEBUG_SETTINGS, "Information about xsettings" },
  { "opengl",          GDK_DEBUG_OPENGL, "Information about OpenGL" },
  { "vulkan",          GDK_DEBUG_VULKAN, "Information about Vulkan" },
  { "selection",       GDK_DEBUG_SELECTION, "Information about selections" },
  { "clipboard",       GDK_DEBUG_CLIPBOARD, "Information about clipboards" },
  { "dmabuf",          GDK_DEBUG_DMABUF, "Information about dmabuf buffers" },
  { "d3d12",           GDK_DEBUG_D3D12, "Information about Direct3D12" },
  { "offload",         GDK_DEBUG_OFFLOAD, "Information about subsurfaces and graphics offload" },

  { "linear",          GDK_DEBUG_LINEAR, "Enable linear rendering" },
  { "hdr",             GDK_DEBUG_HDR, "Force HDR rendering" },
  { "portals",         GDK_DEBUG_PORTALS, "Force use of portals" },
  { "no-portals",      GDK_DEBUG_NO_PORTALS, "Disable use of portals" },
  { "force-offload",   GDK_DEBUG_FORCE_OFFLOAD, "Force graphics offload for all textures" },
  { "gl-debug",        GDK_DEBUG_GL_DEBUG, "Insert debugging information in OpenGL" },
  { "gl-prefer-gl",    GDK_DEBUG_GL_PREFER_GL, "Prefer GL over GLES API" },
  { "default-settings",GDK_DEBUG_DEFAULT_SETTINGS, "Force default values for xsettings" },
  { "high-depth",      GDK_DEBUG_HIGH_DEPTH, "Use high bit depth rendering if possible" },
  { "no-vsync",        GDK_DEBUG_NO_VSYNC, "Repaint instantly (uses 100% CPU with animations)" },
  { "color-mgmt",      GDK_DEBUG_COLOR_MANAGEMENT, "Enable color management" },
  { "session-mgmt",    GDK_DEBUG_SESSION_MANAGEMENT, "Enable session management" },
};

static const GdkDebugKey gdk_feature_keys[] = {
  { "gl",         GDK_FEATURE_OPENGL,           "Disable OpenGL support" },
  { "gl-api",     GDK_FEATURE_GL_API,           "Disable non-GLES GL API" },
  { "gles-api",   GDK_FEATURE_GLES_API,         "Disable GLES GL API" },
  { "egl",        GDK_FEATURE_EGL,              "Disable EGL" },
  { "glx",        GDK_FEATURE_GLX,              "Disable GLX" },
  { "wgl",        GDK_FEATURE_WGL,              "Disable WGL" },
  { "vulkan",     GDK_FEATURE_VULKAN,           "Disable Vulkan support" },
  { "dmabuf",     GDK_FEATURE_DMABUF,           "Disable dmabuf support" },
  { "d3d11",      GDK_FEATURE_D3D11,            "Disable Direct3D 11" },
  { "d3d12",      GDK_FEATURE_D3D12,            "Disable Direct3D 12" },
  { "dcomp",      GDK_FEATURE_DCOMP,            "Disable Direct Composition" },
  { "offload",    GDK_FEATURE_OFFLOAD,          "Disable graphics offload" },
  { "threads",    GDK_FEATURE_THREADS,          "Disable threads where possible" },
  { "icon-nodes", GDK_FEATURE_ICON_NODES,       "Disable svg->node conversion for symbolic icons" },
};

static GdkFeatures gdk_features;

gboolean
gdk_has_feature (GdkFeatures features)
{
  return (features & gdk_features) == features;
}

#ifdef G_HAS_CONSTRUCTORS
#ifdef G_DEFINE_CONSTRUCTOR_NEEDS_PRAGMA
#pragma G_DEFINE_CONSTRUCTOR_PRAGMA_ARGS(stash_and_unset_environment)
#endif
G_DEFINE_CONSTRUCTOR(stash_and_unset_environment)
#endif

static char *startup_notification_id = NULL;
static char *xdg_activation_token = NULL;

static void
stash_and_unset_environment (void)
{
  /* Copies environment variables and unsets them so they won't be inherited by
   * child processes and confuse things.
   *
   * Changing environment variables can be racy so we try to do this as early as
   * possible in the program flow and before any printing that might involve
   * environment variables.
   */
  struct {
    const char *key;
    char **dst;
  } vars[] = {
    { "DESKTOP_STARTUP_ID", &startup_notification_id },
    { "XDG_ACTIVATION_TOKEN", &xdg_activation_token },
  };
  size_t i;

  for (i = 0; i < G_N_ELEMENTS (vars); i++)
    *vars[i].dst = g_strdup (g_getenv (vars[i].key));

  for (i = 0; i < G_N_ELEMENTS (vars); i++)
    g_unsetenv (vars[i].key);

  for (i = 0; i < G_N_ELEMENTS (vars); i++)
    {
      if (*vars[i].dst == NULL)
        continue;

      if (!g_utf8_validate (*vars[i].dst, -1, NULL))
        {
          g_warning ("%s contains invalid UTF-8", vars[i].key);
          g_clear_pointer (vars[i].dst, g_free);
        }
    }
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

guint
gdk_parse_debug_var (const char        *variable,
                     const char        *docs,
                     const GdkDebugKey *keys,
                     guint              nkeys)
{
  guint i;
  guint result = 0;
  const char *string;
  const char *p;
  const char *q;
  gboolean invert;
  gboolean help;

  string = g_getenv (variable);
  if (string == NULL)
    return 0;

  p = string;
  invert = FALSE;
  help = FALSE;

  while (*p)
    {
      q = strpbrk (p, ":;, \t");
      if (!q)
        q = p + strlen (p);

      if (3 == q - p && g_ascii_strncasecmp ("all", p, q - p) == 0)
        {
          invert = TRUE;
        }
      else if (4 == q - p && g_ascii_strncasecmp ("help", p, q - p) == 0)
        {
          help = TRUE;
        }
      else
        {
          for (i = 0; i < nkeys; i++)
            {
              if (strlen (keys[i].key) == q - p &&
                  g_ascii_strncasecmp (keys[i].key, p, q - p) == 0)
                {
                  result |= keys[i].value;
                  break;
                }
            }
          if (i == nkeys)
            gdk_help_message ("Unrecognized value \"%.*s\". Try %s=help", (int) (q - p), p, variable);
         }

      p = q;
      if (*p)
        p++;
    }

  if (help)
    {
      int max_width = 4;
      for (i = 0; i < nkeys; i++)
        max_width = MAX (max_width, strlen (keys[i].key));
      max_width += 4;

      gdk_help_message ("%s", docs);
      gdk_help_message ("Supported %s values:", variable);
      for (i = 0; i < nkeys; i++)
        gdk_help_message ("  %s%*s%s", keys[i].key, (int)(max_width - strlen (keys[i].key)), " ", keys[i].help);
      gdk_help_message ("  %s%*s%s", "all", max_width - 3, " ", "Enable all values. Other given values are subtracted");
      gdk_help_message ("  %s%*s%s", "help", max_width - 4, " ", "Print this help");
      gdk_help_message ("\nMultiple values can be given, separated by : or space.");
    }

  if (invert)
    {
      guint all_flags = 0;

      for (i = 0; i < nkeys; i++)
        {
          all_flags |= keys[i].value;
        }

      result = all_flags & (~result);
    }

  return result;
}

void
gdk_pre_parse (void)
{
  GdkFeatures disabled_features;

  gdk_initialized = TRUE;

  gdk_ensure_resources ();

  _gdk_debug_flags = gdk_parse_debug_var ("GDK_DEBUG",
      "GDK_DEBUG can be set to values that make GDK print out different\n"
      "types of debugging information or change the behavior of GDK for\n"
      "debugging purposes.\n",
      gdk_debug_keys,
      G_N_ELEMENTS (gdk_debug_keys));

  disabled_features = gdk_parse_debug_var ("GDK_DISABLE",
      "GDK_DISABLE can be set to values which cause GDK to disable\n"
      "certain features.\n",
      gdk_feature_keys,
      G_N_ELEMENTS (gdk_feature_keys));

  gdk_features = GDK_ALL_FEATURES & ~disabled_features;

#ifndef G_HAS_CONSTRUCTORS
  stash_and_unset_environment ();
#endif

  gdk_content_init_serializers ();
  gdk_content_init_deserializers ();
}

/*< private >
 * gdk_display_open_default:
 *
 * Opens the default display specified by command line arguments or
 * environment variables, sets it as the default display, and returns
 * it. If the default display has previously been set, simply returns
 * that. An internal function that should not be used by applications.
 *
 * Returns: (nullable) (transfer none): the default display, if it
 *   could be opened, otherwise %NULL.
 */
GdkDisplay *
gdk_display_open_default (void)
{
  GdkDisplay *display;

  gdk_ensure_initialized ();

  display = gdk_display_get_default ();

  if (!display)
    display = gdk_display_open (NULL);

  return display;
}

/*< private >
 * gdk_get_startup_notification_id:
 *
 * Returns the original value of the XDG_ACTIVATION_TOKEN environment
 * variable if it was defined and valid, otherwise it returns the original
 * value of the DESKTOP_STARTUP_ID environment variable if it was defined
 * and valid, or %NULL if neither of them were defined and valid.
 *
 * Returns: (nullable) (transfer none): the original value of the
 *   XDG_ACTIVATION_TOKEN or DESKTOP_STARTUP_ID environment variable
 */
const char *
gdk_get_startup_notification_id (void)
{
  if (xdg_activation_token)
    return xdg_activation_token;

  return startup_notification_id;
}

gboolean
gdk_running_in_sandbox (void)
{
  return g_file_test ("/.flatpak-info", G_FILE_TEST_EXISTS);
}

#define DBUS_BUS_NAME "org.freedesktop.DBus"
#define DBUS_OBJECT_PATH "/org/freedesktop/DBus"
#define DBUS_BUS_INTERFACE "org.freedesktop.DBus"
#define PORTAL_BUS_NAME "org.freedesktop.portal.Desktop"
#define PORTAL_OBJECT_PATH "/org/freedesktop/portal/desktop"

static gboolean portals_disabled;

void
gdk_disable_portals (void)
{
  portals_disabled = TRUE;
}

static gboolean
environment_has_portals (void)
{
  static gboolean cached = FALSE;
  static gboolean has_portals = FALSE;
  GDBusConnection *bus = NULL;
  GVariant *result = NULL;
  GVariantIter *activatable_names = NULL;
  const char *name = NULL;
  GError *error = NULL;

  if (cached)
    return has_portals;

  bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  if (!bus)
    return FALSE;

  result = g_dbus_connection_call_sync (bus,
                                        DBUS_BUS_NAME,
                                        DBUS_OBJECT_PATH,
                                        DBUS_BUS_INTERFACE,
                                        "ListActivatableNames",
                                        NULL,
                                        G_VARIANT_TYPE ("(as)"),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        25000,
                                        NULL,
                                        &error);
  if (error != NULL)
    {
      g_warning ("Cannot list activatable names: %s", error->message);
      g_clear_error (&error);
      return FALSE;
    }

  g_variant_get (result, "(as)", &activatable_names);
  while (g_variant_iter_next (activatable_names, "&s", &name))
    if (g_str_equal (name, PORTAL_BUS_NAME))
      {
        has_portals = TRUE;
        break;
      }
  g_variant_iter_free (activatable_names);
  g_variant_unref (result);

  cached = TRUE;
  return has_portals;
}

static gboolean
check_portal_interface (const char *portal_interface,
                        guint       min_version)
{
  static GHashTable *versions = NULL;
  GDBusConnection *bus = NULL;
  guint version = 0;
  gpointer val;
  GError *error = NULL;

  /* Portal versions start at 1, and we use 0 as marker
   * for unsupported interfaces.
   */
  min_version = MAX (min_version, 1);

  bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  if (bus == NULL)
    return FALSE;

  if (!versions)
    versions = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  if (!g_hash_table_lookup_extended (versions, portal_interface, NULL, &val))
    {
      GVariant *result;

      result = g_dbus_connection_call_sync (bus,
                                            PORTAL_BUS_NAME,
                                            PORTAL_OBJECT_PATH,
                                            "org.freedesktop.DBus.Properties",
                                            "Get",
                                            g_variant_new ("(ss)", portal_interface, "version"),
                                            NULL,
                                            G_DBUS_CALL_FLAGS_NONE,
                                            25000,
                                            NULL,
                                            &error);

      if (error != NULL)
        {
          g_warning ("Cannot get portal %s version: %s", portal_interface, error->message);
          g_clear_error (&error);
          version = 0;
        }
      else
        {
          GVariant *v;

          g_variant_get (result, "(v)", &v);
          version = g_variant_get_uint32 (v);
          g_variant_unref (v);
          g_variant_unref (result);
        }

      val = GUINT_TO_POINTER (version);
      g_hash_table_insert (versions, g_strdup (portal_interface), val);
    }
  else
    {
      version = GPOINTER_TO_UINT (val);
    }

  g_object_unref (bus);

  return version >= min_version;
}

/* Here we decide whether we should use a given portal or not.
 * - If the GDK_DEBUG flags are set, they always win
 * - If we are in a sandbox, we always want to use portals
 * - Otherwise, we want to use the portal if it is available
 *
 * The upshot is: if this functions return true and using the
 * portal fails, we should treat it as an error, not fall back
 * to something else.
 */
gboolean
gdk_display_should_use_portal (GdkDisplay *display,
                               const char *portal_interface,
                               guint       min_version)
{
  if (gdk_display_get_debug_flags (display) & GDK_DEBUG_NO_PORTALS)
    return FALSE;

  if (gdk_display_get_debug_flags (display) & GDK_DEBUG_PORTALS)
    return TRUE;

  if (portals_disabled)
    return FALSE;

  if (gdk_running_in_sandbox ())
    return TRUE;

  if (!environment_has_portals ())
    return FALSE;

  if (portal_interface == NULL)
    return TRUE;

  return check_portal_interface (portal_interface, min_version);
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

PangoDirection
gdk_find_base_dir (const char *text,
                   int          length)
{
  PangoDirection dir = PANGO_DIRECTION_NEUTRAL;
  const char *p;

  g_return_val_if_fail (text != NULL || length == 0, PANGO_DIRECTION_NEUTRAL);

  p = text;
  while ((length < 0 || p < text + length) && *p)
    {
      gunichar wc = g_utf8_get_char (p);

      dir = gdk_unichar_direction (wc);

      if (dir != PANGO_DIRECTION_NEUTRAL)
        break;

      p = g_utf8_next_char (p);
    }

  return dir;
}

void
gdk_source_set_static_name_by_id (guint           tag,
                                  const char     *name)
{
  GSource *source;

  g_return_if_fail (tag > 0);

  source = g_main_context_find_source_by_id (NULL, tag);
  if (source == NULL)
    return;

  g_source_set_static_name (source, name);
}
