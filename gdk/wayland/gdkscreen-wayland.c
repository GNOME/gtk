/*
 * Copyright © 2010 Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <gio/gio.h>
#include "gdkscreenprivate.h"
#include "gdkvisualprivate.h"
#include "gdkdisplay.h"
#include "gdkdisplay-wayland.h"
#include "gdkmonitor-wayland.h"
#include "gdkwayland.h"
#include "gdkprivate-wayland.h"
#include "gdk-private.h"

#include "wm-button-layout-translation.h"

typedef struct _GdkWaylandScreen      GdkWaylandScreen;
typedef struct _GdkWaylandScreenClass GdkWaylandScreenClass;

#define GDK_TYPE_WAYLAND_SCREEN              (_gdk_wayland_screen_get_type ())
#define GDK_WAYLAND_SCREEN(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WAYLAND_SCREEN, GdkWaylandScreen))
#define GDK_WAYLAND_SCREEN_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WAYLAND_SCREEN, GdkWaylandScreenClass))
#define GDK_IS_WAYLAND_SCREEN(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WAYLAND_SCREEN))
#define GDK_IS_WAYLAND_SCREEN_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WAYLAND_SCREEN))
#define GDK_WAYLAND_SCREEN_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WAYLAND_SCREEN, GdkWaylandScreenClass))

typedef struct {
        gboolean     antialias;
        gboolean     hinting;
        gint         dpi;
        const gchar *rgba;
        const gchar *hintstyle;
} GsdXftSettings;

typedef struct {
  guint  fontconfig_timestamp;
  gchar *modules;
} GsdExtSettings;

struct _GdkWaylandScreen
{
  GdkScreen parent_instance;

  GdkDisplay *display;
  GdkWindow *root_window;

  int width, height;
  int width_mm, height_mm;

  /* Visual Part */
  GdkVisual *visual;

  GHashTable *settings;
  GsdXftSettings xft_settings;
  GsdExtSettings dbus_settings;
  GDBusProxy *settings_portal;

  GDBusProxy *dbus_proxy;
  GCancellable *dbus_cancellable;
  gulong dbus_setting_change_id;

  guint32    shell_capabilities;
};

struct _GdkWaylandScreenClass
{
  GdkScreenClass parent_class;
};

#define OUTPUT_VERSION_WITH_DONE 2
#define NO_XDG_OUTPUT_DONE_SINCE_VERSION 3

#define GTK_SETTINGS_DBUS_PATH "/org/gtk/Settings"
#define GTK_SETTINGS_DBUS_NAME "org.gtk.Settings"

GType _gdk_wayland_screen_get_type (void);

G_DEFINE_TYPE (GdkWaylandScreen, _gdk_wayland_screen, GDK_TYPE_SCREEN)

static void
gdk_wayland_screen_dispose (GObject *object)
{
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (object);

  if (screen_wayland->dbus_proxy && screen_wayland->dbus_setting_change_id > 0)
    {
      g_signal_handler_disconnect (screen_wayland->dbus_proxy,
                                   screen_wayland->dbus_setting_change_id);
      screen_wayland->dbus_setting_change_id = 0;
    }

  g_cancellable_cancel (screen_wayland->dbus_cancellable);

  if (screen_wayland->root_window)
    _gdk_window_destroy (screen_wayland->root_window, FALSE);

  G_OBJECT_CLASS (_gdk_wayland_screen_parent_class)->dispose (object);
}

static void
gdk_wayland_screen_finalize (GObject *object)
{
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (object);

  g_clear_object (&screen_wayland->dbus_proxy);
  g_clear_object (&screen_wayland->dbus_cancellable);

  if (screen_wayland->root_window)
    g_object_unref (screen_wayland->root_window);

  g_object_unref (screen_wayland->visual);

  if (screen_wayland->settings)
    g_hash_table_destroy (screen_wayland->settings);

  g_clear_object (&screen_wayland->settings_portal);

  g_free (screen_wayland->dbus_settings.modules);

  G_OBJECT_CLASS (_gdk_wayland_screen_parent_class)->finalize (object);
}

static GdkDisplay *
gdk_wayland_screen_get_display (GdkScreen *screen)
{
  return GDK_WAYLAND_SCREEN (screen)->display;
}

static gint
gdk_wayland_screen_get_width (GdkScreen *screen)
{
  return GDK_WAYLAND_SCREEN (screen)->width;
}

static gint
gdk_wayland_screen_get_height (GdkScreen *screen)
{
  return GDK_WAYLAND_SCREEN (screen)->height;
}

static gint
gdk_wayland_screen_get_width_mm (GdkScreen *screen)
{
  return GDK_WAYLAND_SCREEN (screen)->width_mm;
}

static gint
gdk_wayland_screen_get_height_mm (GdkScreen *screen)
{
  return GDK_WAYLAND_SCREEN (screen)->height_mm;
}

static gint
gdk_wayland_screen_get_number (GdkScreen *screen)
{
  return 0;
}

static GdkWindow *
gdk_wayland_screen_get_root_window (GdkScreen *screen)
{
  return GDK_WAYLAND_SCREEN (screen)->root_window;
}

static GdkVisual *
gdk_wayland_screen_get_system_visual (GdkScreen * screen)
{
  return (GdkVisual *) GDK_WAYLAND_SCREEN (screen)->visual;
}

static GdkVisual *
gdk_wayland_screen_get_rgba_visual (GdkScreen *screen)
{
  return (GdkVisual *) GDK_WAYLAND_SCREEN (screen)->visual;
}

static gboolean
gdk_wayland_screen_is_composited (GdkScreen *screen)
{
  return TRUE;
}

static gchar *
gdk_wayland_screen_make_display_name (GdkScreen *screen)
{
  return g_strdup (gdk_display_get_name (GDK_WAYLAND_SCREEN (screen)->display));
}

static GdkWindow *
gdk_wayland_screen_get_active_window (GdkScreen *screen)
{
  return NULL;
}

static GList *
gdk_wayland_screen_get_window_stack (GdkScreen *screen)
{
  return NULL;
}

static void
gdk_wayland_screen_broadcast_client_message (GdkScreen *screen,
					     GdkEvent  *event)
{
}

static void
notify_setting (GdkScreen   *screen,
                const gchar *setting)
{
  GdkEvent event;

  event.type = GDK_SETTING;
  event.setting.window = gdk_screen_get_root_window (screen);
  event.setting.send_event = FALSE;
  event.setting.action = GDK_SETTING_ACTION_CHANGED;
  event.setting.name = (gchar *)setting;
  gdk_event_put (&event);
}

typedef enum
{
  GSD_FONT_ANTIALIASING_MODE_NONE,
  GSD_FONT_ANTIALIASING_MODE_GRAYSCALE,
  GSD_FONT_ANTIALIASING_MODE_RGBA
} GsdFontAntialiasingMode;

static int
get_antialiasing (const char *s)
{
  const char *names[] = { "none", "grayscale", "rgba" };
  int i;

  for (i = 0; i < G_N_ELEMENTS (names); i++)
    if (strcmp (s, names[i]) == 0)
      return i;

  return 0;
}

typedef enum
{
  GSD_FONT_HINTING_NONE,
  GSD_FONT_HINTING_SLIGHT,
  GSD_FONT_HINTING_MEDIUM,
  GSD_FONT_HINTING_FULL
} GsdFontHinting;

static int
get_hinting (const char *s)
{
  const char *names[] = { "none", "slight", "medium", "full" };
  int i;

  for (i = 0; i < G_N_ELEMENTS (names); i++)
    if (strcmp (s, names[i]) == 0)
      return i;

  return 0;
}

typedef enum
{
  GSD_FONT_RGBA_ORDER_RGBA,
  GSD_FONT_RGBA_ORDER_RGB,
  GSD_FONT_RGBA_ORDER_BGR,
  GSD_FONT_RGBA_ORDER_VRGB,
  GSD_FONT_RGBA_ORDER_VBGR
} GsdFontRgbaOrder;

static int
get_order (const char *s)
{
  const char *names[] = { "rgba", "rgb", "bgr", "vrgb", "vbgr" };
  int i;

  for (i = 0; i < G_N_ELEMENTS (names); i++)
    if (strcmp (s, names[i]) == 0)
      return i;

  return 0;
}

static gdouble
get_dpi_from_gsettings (GdkWaylandScreen *screen_wayland)
{
  GSettings *settings;
  gdouble factor;

  settings = g_hash_table_lookup (screen_wayland->settings,
                                  "org.gnome.desktop.interface");
  if (settings != NULL)
    factor = g_settings_get_double (settings, "text-scaling-factor");
  else
    factor = 1.0;

  return 96.0 * factor;
}

/* When using the Settings portal, we cache the value in
 * the fallback member, and we ignore the valid field
 */
typedef struct _TranslationEntry TranslationEntry;
struct _TranslationEntry {
  gboolean valid;
  const gchar *schema;
  const gchar *key;
  const gchar *setting;
  GType type;
  union {
    const char *s;
    gint         i;
    gboolean     b;
  } fallback;
};

static TranslationEntry * find_translation_entry_by_schema (const char *schema,
                                                            const char *key);
static void
update_xft_settings (GdkScreen *screen, gboolean notify)
{
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (screen);
  GSettings *settings;
  GsdFontAntialiasingMode antialiasing;
  GsdFontHinting hinting;
  GsdFontRgbaOrder order;
  gboolean use_rgba = FALSE;
  GsdXftSettings xft_settings;
  double dpi;

  if (screen_wayland->settings_portal)
    {
      TranslationEntry *entry;

      entry = find_translation_entry_by_schema ("org.gnome.desktop.interface", "font-antialiasing");

      if (entry->valid)
        {
          antialiasing = entry->fallback.i;

          entry = find_translation_entry_by_schema ("org.gnome.desktop.interface", "font-hinting");
          hinting = entry->fallback.i;

          entry = find_translation_entry_by_schema ("org.gnome.desktop.interface", "font-rgba-order");
          order = entry->fallback.i;
        }
      else
        {
          entry = find_translation_entry_by_schema ("org.gnome.settings-daemon.plugins.xsettings", "antialiasing");
          antialiasing = entry->fallback.i;

          entry = find_translation_entry_by_schema ("org.gnome.settings-daemon.plugins.xsettings", "hinting");
          hinting = entry->fallback.i;

          entry = find_translation_entry_by_schema ("org.gnome.settings-daemon.plugins.xsettings", "rgba-order");
          order = entry->fallback.i;
        }

      entry = find_translation_entry_by_schema ("org.gnome.desktop.interface", "text-scaling-factor");
      dpi = 96.0 * entry->fallback.i / 65536.0 * 1024; /* Xft wants 1/1024th of an inch */
    }
  else
    {
      TranslationEntry *entry;

      entry = find_translation_entry_by_schema ("org.gnome.desktop.interface", "font-antialiasing");

      if (entry && entry->valid)
        {
          settings = g_hash_table_lookup (screen_wayland->settings,
                                          "org.gnome.desktop.interface");
          antialiasing = g_settings_get_enum (settings, "font-antialiasing");
          hinting = g_settings_get_enum (settings, "font-hinting");
          order = g_settings_get_enum (settings, "font-rgba-order");
        }
      else if (g_hash_table_contains (screen_wayland->settings,
                                      "org.gnome.settings-daemon.plugins.xsettings"))
        {
          settings = g_hash_table_lookup (screen_wayland->settings,
                                          "org.gnome.settings-daemon.plugins.xsettings");
          antialiasing = g_settings_get_enum (settings, "antialiasing");
          hinting = g_settings_get_enum (settings, "hinting");
          order = g_settings_get_enum (settings, "rgba-order");
        }
      else
        {
          antialiasing = GSD_FONT_ANTIALIASING_MODE_GRAYSCALE;
          hinting = GSD_FONT_HINTING_MEDIUM;
          order = GSD_FONT_RGBA_ORDER_RGB;
        }

      dpi = get_dpi_from_gsettings (screen_wayland) * 1024;
    }

  xft_settings.antialias = (antialiasing != GSD_FONT_ANTIALIASING_MODE_NONE);
  xft_settings.hinting = (hinting != GSD_FONT_HINTING_NONE);
  xft_settings.dpi = dpi;
  xft_settings.rgba = "rgb";
  xft_settings.hintstyle = "hintfull";

  switch (hinting)
    {
    case GSD_FONT_HINTING_NONE:
      xft_settings.hintstyle = "hintnone";
      break;
    case GSD_FONT_HINTING_SLIGHT:
      xft_settings.hintstyle = "hintslight";
      break;
    case GSD_FONT_HINTING_MEDIUM:
      xft_settings.hintstyle = "hintmedium";
      break;
    case GSD_FONT_HINTING_FULL:
      xft_settings.hintstyle = "hintfull";
      break;
    }

  switch (order)
    {
    case GSD_FONT_RGBA_ORDER_RGBA:
      xft_settings.rgba = "rgba";
      break;
    case GSD_FONT_RGBA_ORDER_RGB:
      xft_settings.rgba = "rgb";
      break;
    case GSD_FONT_RGBA_ORDER_BGR:
      xft_settings.rgba = "bgr";
      break;
    case GSD_FONT_RGBA_ORDER_VRGB:
      xft_settings.rgba = "vrgb";
      break;
    case GSD_FONT_RGBA_ORDER_VBGR:
      xft_settings.rgba = "vbgr";
      break;
    }

  switch (antialiasing)
   {
   case GSD_FONT_ANTIALIASING_MODE_NONE:
     xft_settings.antialias = FALSE;
     break;
   case GSD_FONT_ANTIALIASING_MODE_GRAYSCALE:
     xft_settings.antialias = TRUE;
     break;
   case GSD_FONT_ANTIALIASING_MODE_RGBA:
     xft_settings.antialias = TRUE;
     use_rgba = TRUE;
   }

  if (!use_rgba)
    xft_settings.rgba = "none";

  if (screen_wayland->xft_settings.antialias != xft_settings.antialias)
    {
      screen_wayland->xft_settings.antialias = xft_settings.antialias;
      if (notify)
        notify_setting (screen, "gtk-xft-antialias");
    }

  if (screen_wayland->xft_settings.hinting != xft_settings.hinting)
    {
      screen_wayland->xft_settings.hinting = xft_settings.hinting;
      if (notify)
        notify_setting (screen, "gtk-xft-hinting");
    }

  if (screen_wayland->xft_settings.hintstyle != xft_settings.hintstyle)
    {
      screen_wayland->xft_settings.hintstyle = xft_settings.hintstyle;
      if (notify)
        notify_setting (screen, "gtk-xft-hintstyle");
    }

  if (screen_wayland->xft_settings.rgba != xft_settings.rgba)
    {
      screen_wayland->xft_settings.rgba = xft_settings.rgba;
      if (notify)
        notify_setting (screen, "gtk-xft-rgba");
    }

  if (screen_wayland->xft_settings.dpi != xft_settings.dpi)
    {
      double dpi = xft_settings.dpi / 1024.;
      const char *scale_env;
      double scale;

      screen_wayland->xft_settings.dpi = xft_settings.dpi;

      scale_env = g_getenv ("GDK_DPI_SCALE");
      if (scale_env)
        {
          scale = g_ascii_strtod (scale_env, NULL);
          if (scale != 0 && dpi > 0)
            dpi *= scale;
        }

      _gdk_screen_set_resolution (screen, dpi);

      if (notify)
        notify_setting (screen, "gtk-xft-dpi");
    }
}

#define WM_SETTINGS_SCHEMA "org.gnome.desktop.wm.preferences"
#define CLASSIC_WM_SETTINGS_SCHEMA "org.gnome.shell.extensions.classic-overrides"

static TranslationEntry translations[] = {
  { FALSE, "org.gnome.desktop.interface", "gtk-theme", "gtk-theme-name" , G_TYPE_STRING, { .s = "Adwaita" } },
  { FALSE, "org.gnome.desktop.interface", "gtk-key-theme", "gtk-key-theme-name" , G_TYPE_STRING, { .s = "Default" } },
  { FALSE, "org.gnome.desktop.interface", "icon-theme", "gtk-icon-theme-name", G_TYPE_STRING, { .s = "gnome" } },
  { FALSE, "org.gnome.desktop.interface", "cursor-theme", "gtk-cursor-theme-name", G_TYPE_STRING, { .s = "Adwaita" } },
  { FALSE, "org.gnome.desktop.interface", "cursor-size", "gtk-cursor-theme-size", G_TYPE_INT, { .i = 24 } },
  { FALSE, "org.gnome.desktop.interface", "font-name", "gtk-font-name", G_TYPE_STRING, { .s = "Cantarell 11" } },
  { FALSE, "org.gnome.desktop.interface", "cursor-blink", "gtk-cursor-blink", G_TYPE_BOOLEAN,  { .b = TRUE } },
  { FALSE, "org.gnome.desktop.interface", "cursor-blink-time", "gtk-cursor-blink-time", G_TYPE_INT, { .i = 1200 } },
  { FALSE, "org.gnome.desktop.interface", "cursor-blink-timeout", "gtk-cursor-blink-timeout", G_TYPE_INT, { .i = 3600 } },
  { FALSE, "org.gnome.desktop.interface", "gtk-im-module", "gtk-im-module", G_TYPE_STRING, { .s = "simple" } },
  { FALSE, "org.gnome.desktop.interface", "enable-animations", "gtk-enable-animations", G_TYPE_BOOLEAN, { .b = TRUE } },
  { FALSE, "org.gnome.desktop.interface", "gtk-enable-primary-paste", "gtk-enable-primary-paste", G_TYPE_BOOLEAN, { .b = TRUE } },
  { FALSE, "org.gnome.desktop.interface", "overlay-scrolling", "gtk-overlay-scrolling", G_TYPE_BOOLEAN, { .b = TRUE } },
  { FALSE, "org.gnome.desktop.peripherals.mouse", "double-click", "gtk-double-click-time", G_TYPE_INT, { .i = 400 } },
  { FALSE, "org.gnome.desktop.peripherals.mouse", "drag-threshold", "gtk-dnd-drag-threshold", G_TYPE_INT, {.i = 8 } },
  { FALSE, "org.gnome.settings-daemon.peripherals.mouse", "double-click", "gtk-double-click-time", G_TYPE_INT, { .i = 400 } },
  { FALSE, "org.gnome.settings-daemon.peripherals.mouse", "drag-threshold", "gtk-dnd-drag-threshold", G_TYPE_INT, {.i = 8 } },
  { FALSE, "org.gnome.desktop.sound", "theme-name", "gtk-sound-theme-name", G_TYPE_STRING, { .s = "freedesktop" } },
  { FALSE, "org.gnome.desktop.sound", "event-sounds", "gtk-enable-event-sounds", G_TYPE_BOOLEAN, { .b = TRUE } },
  { FALSE, "org.gnome.desktop.sound", "input-feedback-sounds", "gtk-enable-input-feedback-sounds", G_TYPE_BOOLEAN, { . b = FALSE } },
  { FALSE, "org.gnome.desktop.privacy", "recent-files-max-age", "gtk-recent-files-max-age", G_TYPE_INT, { .i = 30 } },
  { FALSE, "org.gnome.desktop.privacy", "remember-recent-files",    "gtk-recent-files-enabled", G_TYPE_BOOLEAN, { .b = TRUE } },
  { FALSE, WM_SETTINGS_SCHEMA, "button-layout",    "gtk-decoration-layout", G_TYPE_STRING, { .s = "menu:close" } },
  { FALSE, CLASSIC_WM_SETTINGS_SCHEMA, "button-layout",   "gtk-decoration-layout", G_TYPE_STRING, { .s = "menu:close" } },
  { FALSE, "org.gnome.desktop.interface", "font-antialiasing", "gtk-xft-antialias", G_TYPE_NONE, { .i = 1 } },
  { FALSE, "org.gnome.desktop.interface", "font-hinting", "gtk-xft-hinting", G_TYPE_NONE, { .i = 1 } },
  { FALSE, "org.gnome.desktop.interface", "font-hinting", "gtk-xft-hintstyle", G_TYPE_NONE, { .i = 1 } },
  { FALSE, "org.gnome.desktop.interface", "font-rgba-order", "gtk-xft-rgba", G_TYPE_NONE, { .i = 0 } },
  { FALSE, "org.gnome.settings-daemon.plugins.xsettings", "antialiasing", "gtk-xft-antialias", G_TYPE_NONE, { .i = 1 } },
  { FALSE, "org.gnome.settings-daemon.plugins.xsettings", "hinting", "gtk-xft-hinting", G_TYPE_NONE, { .i = 1 } },
  { FALSE, "org.gnome.settings-daemon.plugins.xsettings", "hinting", "gtk-xft-hintstyle", G_TYPE_NONE, { .i = 1 } },
  { FALSE, "org.gnome.settings-daemon.plugins.xsettings", "rgba-order", "gtk-xft-rgba", G_TYPE_NONE, { .i = 0 } },
  { FALSE, "org.gnome.desktop.interface", "text-scaling-factor", "gtk-xft-dpi" , G_TYPE_NONE, { .i = 0 } },
  { FALSE, "org.gnome.desktop.wm.preferences", "action-double-click-titlebar", "gtk-titlebar-double-click", G_TYPE_STRING, { .s = "toggle-maximize" } },
  { FALSE, "org.gnome.desktop.wm.preferences", "action-middle-click-titlebar", "gtk-titlebar-middle-click", G_TYPE_STRING, { .s = "none" } },
  { FALSE, "org.gnome.desktop.wm.preferences", "action-right-click-titlebar", "gtk-titlebar-right-click", G_TYPE_STRING, { .s = "menu" } },
  { FALSE, "org.gnome.desktop.a11y", "always-show-text-caret", "gtk-keynav-use-caret", G_TYPE_BOOLEAN, { .b = FALSE } },
  { FALSE, "org.gnome.desktop.a11y.interface", "high-contrast", "high-contrast", G_TYPE_NONE, { .b = FALSE } },
  { FALSE, "org.gnome.fontconfig", "serial", "gtk-fontconfig-timestamp", G_TYPE_INT, { .i = 0 } }
};

static TranslationEntry *
find_translation_entry_by_schema (const char *schema,
                                  const gchar *key)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (translations); i++)
    {
      if (g_str_equal (schema, translations[i].schema) &&
          g_str_equal (key, translations[i].key))
        return &translations[i];
    }

  return NULL;
}

static TranslationEntry *
find_translation_entry_by_key (GSettings  *settings,
                               const char *key)
{
  char *schema;
  TranslationEntry *entry;

  g_object_get (settings, "schema", &schema, NULL);
  entry = find_translation_entry_by_schema (schema, key);
  g_free (schema);

  return entry;
}

static TranslationEntry *
find_translation_entry_by_setting (const gchar *setting)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (translations); i++)
    {
      if (g_str_equal (setting, translations[i].setting))
        return &translations[i];
    }

  return NULL;
}

static void
high_contrast_changed (GdkScreen *screen)
{
  notify_setting (screen, "gtk-theme-name");
  notify_setting (screen, "gtk-icon-theme-name");
}

static void
settings_changed (GSettings   *settings,
                  const gchar *key,
                  GdkScreen   *screen)
{
  TranslationEntry *entry;

  entry = find_translation_entry_by_key (settings, key);

  if (entry != NULL)
    {
      if (entry->type != G_TYPE_NONE)
        notify_setting (screen, entry->setting);
      else if (strcmp (key, "high-contrast") == 0)
        high_contrast_changed (screen);
      else
        update_xft_settings (screen, TRUE);
    }
}

static void
apply_portal_setting (TranslationEntry *entry,
                      GVariant         *value,
                      GdkScreen        *screen)
{
  switch (entry->type)
    {
    case G_TYPE_STRING:
      entry->fallback.s = g_intern_string (g_variant_get_string (value, NULL));
      break;
    case G_TYPE_INT:
      entry->fallback.i = g_variant_get_int32 (value);
      break;
    case G_TYPE_BOOLEAN:
      entry->fallback.b = g_variant_get_boolean (value);
      break;
    case G_TYPE_NONE:
      if (strcmp (entry->key, "antialiasing") == 0 ||
          strcmp (entry->key, "font-antialiasing") == 0)
        entry->fallback.i = get_antialiasing (g_variant_get_string (value, NULL));
      else if (strcmp (entry->key, "hinting") == 0 ||
               strcmp (entry->key, "font-hinting") == 0)
        entry->fallback.i = get_hinting (g_variant_get_string (value, NULL));
      else if (strcmp (entry->key, "rgba-order") == 0 ||
               strcmp (entry->key, "font-rgba-order") == 0)
        entry->fallback.i = get_order (g_variant_get_string (value, NULL));
      else if (strcmp (entry->key, "text-scaling-factor") == 0)
        entry->fallback.i = (int) (g_variant_get_double (value) * 65536.0);
      update_xft_settings (screen, TRUE);
      break;
    default:
      break;
    }
}

static void
settings_portal_changed (GDBusProxy *proxy,
                         const char *sender_name,
                         const char *signal_name,
                         GVariant   *parameters,
                         GdkScreen  *screen)
{
  if (strcmp (signal_name, "SettingChanged") == 0)
    {
      const char *namespace;
      const char *name;
      GVariant *value;
      TranslationEntry *entry;

      g_variant_get (parameters, "(&s&sv)", &namespace, &name, &value);

      entry = find_translation_entry_by_schema (namespace, name);
      if (entry != NULL)
        {
          char *a = g_variant_print (value, FALSE);
          g_debug ("Using changed portal setting %s %s: %s", namespace, name, a);
          g_free (a);
          apply_portal_setting (entry, value, screen);
          notify_setting (screen, entry->setting);
        }
      else
        g_debug ("Ignoring portal setting %s %s", namespace, name);

      g_variant_unref (value);
    }
}

static void fontconfig_dbus_proxy_open_cb (GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data);

#define PORTAL_BUS_NAME "org.freedesktop.portal.Desktop"
#define PORTAL_OBJECT_PATH "/org/freedesktop/portal/desktop"
#define PORTAL_SETTINGS_INTERFACE "org.freedesktop.portal.Settings"

static void
init_settings (GdkScreen *screen)
{
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (screen);
  GSettingsSchemaSource *source;
  GSettingsSchema *schema;
  GSettings *settings;
  gint i;

  if (gdk_should_use_portal ())
    {
      GVariant *ret;
      GError *error = NULL;
      const char *schema;
      GVariant *val;
      GVariantIter *iter;
      const char *patterns[] = { "org.gnome.*", NULL };

      screen_wayland->settings_portal = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                                       G_DBUS_PROXY_FLAGS_NONE,
                                                                       NULL,
                                                                       PORTAL_BUS_NAME,
                                                                       PORTAL_OBJECT_PATH,
                                                                       PORTAL_SETTINGS_INTERFACE,
                                                                       NULL,
                                                                       &error);
      if (error)
        {
          g_warning ("Settings portal not found: %s", error->message);
          g_error_free (error);

          goto fallback;
        }

      ret = g_dbus_proxy_call_sync (screen_wayland->settings_portal,
                                    "ReadAll",
                                    g_variant_new ("(^as)", patterns),
                                    G_DBUS_CALL_FLAGS_NONE,
                                    G_MAXINT,
                                    NULL,
                                    &error);
      if (error)
        {
          g_warning ("Failed to read portal settings: %s", error->message);
          g_error_free (error);
          g_clear_object (&screen_wayland->settings_portal);

          goto fallback;
        }

      g_variant_get (ret, "(a{sa{sv}})", &iter);

      if (g_variant_iter_n_children (iter) == 0)
        {
          g_debug ("Received no portal settings");
          g_clear_pointer (&iter, g_variant_iter_free);
          g_clear_pointer (&ret, g_variant_unref);
          g_clear_object (&screen_wayland->settings_portal);

          goto fallback;
        }

      while (g_variant_iter_loop (iter, "{s@a{sv}}", &schema, &val))
        {
          GVariantIter *iter2 = g_variant_iter_new (val);
          const char *key;
          GVariant *v;

          while (g_variant_iter_loop (iter2, "{sv}", &key, &v))
            {
              TranslationEntry *entry = find_translation_entry_by_schema (schema, key);
              if (entry)
                {
                  char *a = g_variant_print (v, FALSE);
                  g_debug ("Using portal setting for %s %s: %s\n", schema, key, a);
                  g_free (a);
                  entry->valid = TRUE;
                  apply_portal_setting (entry, v, screen);
                }
              else
                {
                  g_debug ("Ignoring portal setting for %s %s", schema, key);
                }
            }
          g_variant_iter_free (iter2);
        }
      g_variant_iter_free (iter);

      g_variant_unref (ret);

      g_signal_connect (screen_wayland->settings_portal, "g-signal",
                        G_CALLBACK (settings_portal_changed), screen_wayland);

      return;

fallback:
      g_debug ("Failed to use Settings portal; falling back to gsettings");
    }

  screen_wayland->dbus_cancellable = g_cancellable_new ();
  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            GTK_SETTINGS_DBUS_NAME,
                            GTK_SETTINGS_DBUS_PATH,
                            GTK_SETTINGS_DBUS_NAME,
                            screen_wayland->dbus_cancellable,
                            fontconfig_dbus_proxy_open_cb,
                            screen_wayland);

  screen_wayland->settings = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);

  source = g_settings_schema_source_get_default ();
  if (source == NULL)
    return;

  for (i = 0; i < G_N_ELEMENTS (translations); i++)
    {
      schema = g_settings_schema_source_lookup (source, translations[i].schema, TRUE);
      if (!schema)
        continue;

      if (g_hash_table_lookup (screen_wayland->settings, (gpointer)translations[i].schema) == NULL)
        {
          settings = g_settings_new_full (schema, NULL, NULL);
          g_signal_connect (settings, "changed",
                            G_CALLBACK (settings_changed), screen);
          g_hash_table_insert (screen_wayland->settings, (gpointer)translations[i].schema, settings);
        }

      if (g_settings_schema_has_key (schema, translations[i].key))
        translations[i].valid = TRUE;

      g_settings_schema_unref (schema);
    }

  update_xft_settings (screen, FALSE);
}

static void
gtk_shell_handle_capabilities (void              *data,
                               struct gtk_shell1 *shell,
                               uint32_t           capabilities)
{
  GdkScreen *screen = data;
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (data);

  screen_wayland->shell_capabilities = capabilities;

  notify_setting (screen, "gtk-shell-shows-app-menu");
  notify_setting (screen, "gtk-shell-shows-menubar");
  notify_setting (screen, "gtk-shell-shows-desktop");
}

struct gtk_shell1_listener gdk_screen_gtk_shell_listener = {
  gtk_shell_handle_capabilities
};

void
_gdk_wayland_screen_set_has_gtk_shell (GdkScreen *screen)
{
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (GDK_WAYLAND_SCREEN (screen)->display);

  gtk_shell1_add_listener (display_wayland->gtk_shell,
                           &gdk_screen_gtk_shell_listener,
                           screen);
}

static void
set_value_from_entry (GdkScreen        *screen,
                      TranslationEntry *entry,
                      GValue           *value)
{
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (screen);
  GSettings *settings;

  if (screen_wayland->settings_portal)
    {
      switch (entry->type)
        {
        case G_TYPE_STRING:
          g_value_set_string (value, entry->fallback.s);
          break;
        case G_TYPE_INT:
          if (g_str_equal (entry->setting, "gtk-fontconfig-timestamp"))
            g_value_set_uint (value, (guint)entry->fallback.i);
          else
            g_value_set_int (value, entry->fallback.i);
          break;
        case G_TYPE_BOOLEAN:
          g_value_set_boolean (value, entry->fallback.b);
          break;
        case G_TYPE_NONE:
          if (g_str_equal (entry->setting, "gtk-xft-antialias"))
            g_value_set_int (value, screen_wayland->xft_settings.antialias);
          else if (g_str_equal (entry->setting, "gtk-xft-hinting"))
            g_value_set_int (value, screen_wayland->xft_settings.hinting);
          else if (g_str_equal (entry->setting, "gtk-xft-hintstyle"))
            g_value_set_static_string (value, screen_wayland->xft_settings.hintstyle);
          else if (g_str_equal (entry->setting, "gtk-xft-rgba"))
            g_value_set_static_string (value, screen_wayland->xft_settings.rgba);
          else if (g_str_equal (entry->setting, "gtk-xft-dpi"))
            g_value_set_int (value, screen_wayland->xft_settings.dpi);
          else
            g_assert_not_reached ();
          break;
        default:
          g_assert_not_reached ();
          break;
        }

      return;
    }

  settings = (GSettings *)g_hash_table_lookup (screen_wayland->settings, entry->schema);
  switch (entry->type)
    {
    case G_TYPE_STRING:
      if (settings && entry->valid)
        {
          gchar *s;
          s = g_settings_get_string (settings, entry->key);
          g_value_set_string (value, s);
          g_free (s);
        }
      else
        {
          g_value_set_static_string (value, entry->fallback.s);
        }
      break;
    case G_TYPE_INT:
      if (g_str_equal (entry->setting, "gtk-fontconfig-timestamp"))
        g_value_set_uint (value, screen_wayland->dbus_settings.fontconfig_timestamp);
      else
        g_value_set_int (value, settings && entry->valid
                                ? g_settings_get_int (settings, entry->key)
                                : entry->fallback.i);
      break;
    case G_TYPE_BOOLEAN:
      g_value_set_boolean (value, settings && entry->valid
                                  ? g_settings_get_boolean (settings, entry->key)
                                  : entry->fallback.b);
      break;
    case G_TYPE_NONE:
      if (g_str_equal (entry->setting, "gtk-xft-antialias"))
        g_value_set_int (value, screen_wayland->xft_settings.antialias);
      else if (g_str_equal (entry->setting, "gtk-xft-hinting"))
        g_value_set_int (value, screen_wayland->xft_settings.hinting);
      else if (g_str_equal (entry->setting, "gtk-xft-hintstyle"))
        g_value_set_static_string (value, screen_wayland->xft_settings.hintstyle);
      else if (g_str_equal (entry->setting, "gtk-xft-rgba"))
        g_value_set_static_string (value, screen_wayland->xft_settings.rgba);
      else if (g_str_equal (entry->setting, "gtk-xft-dpi"))
        g_value_set_int (value, screen_wayland->xft_settings.dpi);
      else
        g_assert_not_reached ();
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
set_decoration_layout_from_entry (GdkScreen        *screen,
                                  TranslationEntry *entry,
                                  GValue           *value)
{
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (screen);
  GSettings *settings = NULL;
  const char *session;

  if (screen_wayland->settings_portal)
    {
      g_value_set_string (value, entry->fallback.s);
      return;
    }

  /* Hack: until we get session-dependent defaults in GSettings,
   *       swap out the usual schema for the "classic" one when
   *       running in classic mode
   */
  session = g_getenv ("XDG_CURRENT_DESKTOP");
  if (session && strstr (session, "GNOME-Classic"))
    settings = (GSettings *)g_hash_table_lookup (screen_wayland->settings, CLASSIC_WM_SETTINGS_SCHEMA);

  if (settings == NULL)
    settings = (GSettings *)g_hash_table_lookup (screen_wayland->settings, WM_SETTINGS_SCHEMA);

  if (settings)
    {
      gchar *s = g_settings_get_string (settings, entry->key);

      translate_wm_button_layout_to_gtk (s);
      g_value_set_string (value, s);

      g_free (s);
    }
  else
    {
      g_value_set_static_string (value, entry->fallback.s);
    }
}

static void
set_theme_from_entry (GdkScreen        *screen,
                      TranslationEntry *entry,
                      GValue           *value)
{
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (screen);
  GSettings *settings = NULL;
  GSettingsSchema *schema = NULL;
  gboolean hc = FALSE;

  if (screen_wayland->settings_portal == NULL)
    {
      settings = (GSettings *)g_hash_table_lookup (screen_wayland->settings,
                                                   "org.gnome.desktop.a11y.interface");
    }

  if (settings)
    g_object_get (settings, "settings-schema", &schema, NULL);

  if (schema && g_settings_schema_has_key (schema, "high-contrast"))
    hc = g_settings_get_boolean (settings, "high-contrast");

  g_clear_pointer (&schema, g_settings_schema_unref);

  if (hc)
    g_value_set_static_string (value, "HighContrast");
  else
    set_value_from_entry (screen, entry, value);
}

static gboolean
set_capability_setting (GdkScreen                 *screen,
                        GValue                    *value,
                        enum gtk_shell1_capability test)
{
  GdkWaylandScreen *wayland_screen = GDK_WAYLAND_SCREEN (screen);
  int testbit = 1 << (test - 1);

  g_value_set_boolean (value, (wayland_screen->shell_capabilities & testbit) == testbit);

  return TRUE;
}

static gboolean
gdk_wayland_screen_get_setting (GdkScreen   *screen,
                                const gchar *name,
                                GValue      *value)
{
  GdkWaylandScreen *wayland_screen = GDK_WAYLAND_SCREEN (screen);
  TranslationEntry *entry;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);

  if (wayland_screen->settings != NULL &&
      g_hash_table_size (wayland_screen->settings) == 0)
    return FALSE;

  entry = find_translation_entry_by_setting (name);
  if (entry != NULL)
    {
      if (strcmp (name, "gtk-decoration-layout") == 0)
        set_decoration_layout_from_entry (screen, entry, value);
      else if (strcmp (name, "gtk-theme-name") == 0)
        set_theme_from_entry (screen, entry, value);
      else
        set_value_from_entry (screen, entry, value);
      return TRUE;
   }

  if (strcmp (name, "gtk-shell-shows-app-menu") == 0)
    return set_capability_setting (screen, value,
                                   GTK_SHELL1_CAPABILITY_GLOBAL_APP_MENU);

  if (strcmp (name, "gtk-shell-shows-menubar") == 0)
    return set_capability_setting (screen, value,
                                   GTK_SHELL1_CAPABILITY_GLOBAL_MENU_BAR);

  if (strcmp (name, "gtk-shell-shows-desktop") == 0)
    return set_capability_setting (screen, value,
                                   GTK_SHELL1_CAPABILITY_DESKTOP_ICONS);

  if (strcmp (name, "gtk-dialogs-use-header") == 0)
    {
      g_value_set_boolean (value, TRUE);
      return TRUE;
    }

  if (strcmp (name, "gtk-fontconfig-timestamp") == 0)
    {
      g_value_set_uint (value, wayland_screen->dbus_settings.fontconfig_timestamp);
      return TRUE;
    }

  if (strcmp (name, "gtk-modules") == 0)
    {
      g_value_set_string (value, wayland_screen->dbus_settings.modules);
      return TRUE;
    }

  return FALSE;
}

typedef struct _GdkWaylandVisual	GdkWaylandVisual;
typedef struct _GdkWaylandVisualClass	GdkWaylandVisualClass;

struct _GdkWaylandVisual
{
  GdkVisual visual;
};

struct _GdkWaylandVisualClass
{
  GdkVisualClass parent_class;
};

GType _gdk_wayland_visual_get_type (void);

G_DEFINE_TYPE (GdkWaylandVisual, _gdk_wayland_visual, GDK_TYPE_VISUAL)

static void
_gdk_wayland_visual_class_init (GdkWaylandVisualClass *klass)
{
}

static void
_gdk_wayland_visual_init (GdkWaylandVisual *visual)
{
}

static gint
gdk_wayland_screen_visual_get_best_depth (GdkScreen *screen)
{
  return 32;
}

static GdkVisualType
gdk_wayland_screen_visual_get_best_type (GdkScreen *screen)
{
  return GDK_VISUAL_TRUE_COLOR;
}

static GdkVisual*
gdk_wayland_screen_visual_get_best (GdkScreen *screen)
{
  return GDK_WAYLAND_SCREEN (screen)->visual;
}

static GdkVisual*
gdk_wayland_screen_visual_get_best_with_depth (GdkScreen *screen,
					       gint       depth)
{
  if (depth == 32)
    return GDK_WAYLAND_SCREEN (screen)->visual;
  else
    return NULL;
}

static GdkVisual*
gdk_wayland_screen_visual_get_best_with_type (GdkScreen     *screen,
					      GdkVisualType  visual_type)
{
  if (visual_type == GDK_VISUAL_TRUE_COLOR)
    return GDK_WAYLAND_SCREEN (screen)->visual;
  else
    return NULL;
}

static GdkVisual*
gdk_wayland_screen_visual_get_best_with_both (GdkScreen     *screen,
					      gint           depth,
					      GdkVisualType  visual_type)
{
  if (depth == 32 && visual_type == GDK_VISUAL_TRUE_COLOR)
    return GDK_WAYLAND_SCREEN (screen)->visual;
  else
    return NULL;
}

static void
gdk_wayland_screen_query_depths  (GdkScreen  *screen,
				  gint      **depths,
				  gint       *count)
{
  static gint static_depths[] = { 32 };

  *count = G_N_ELEMENTS(static_depths);
  *depths = static_depths;
}

static void
gdk_wayland_screen_query_visual_types (GdkScreen      *screen,
				       GdkVisualType **visual_types,
				       gint           *count)
{
  static GdkVisualType static_visual_types[] = { GDK_VISUAL_TRUE_COLOR };

  *count = G_N_ELEMENTS(static_visual_types);
  *visual_types = static_visual_types;
}

static GList *
gdk_wayland_screen_list_visuals (GdkScreen *screen)
{
  GList *list;
  GdkWaylandScreen *screen_wayland;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  screen_wayland = GDK_WAYLAND_SCREEN (screen);

  list = g_list_append (NULL, screen_wayland->visual);

  return list;
}

#define GDK_TYPE_WAYLAND_VISUAL              (_gdk_wayland_visual_get_type ())
#define GDK_WAYLAND_VISUAL(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WAYLAND_VISUAL, GdkWaylandVisual))

/* Currently, the Wayland backend only ever uses ARGB8888.
 */
static GdkVisual *
gdk_wayland_visual_new (GdkScreen *screen)
{
  GdkVisual *visual;

  visual = g_object_new (GDK_TYPE_WAYLAND_VISUAL, NULL);
  visual->screen = GDK_SCREEN (screen);
  visual->type = GDK_VISUAL_TRUE_COLOR;
  visual->depth = 32;
  visual->red_mask = 0xff0000;
  visual->green_mask = 0x00ff00;
  visual->blue_mask = 0x0000ff;
  visual->bits_per_rgb = 8;

  return visual;
}

static void
dbus_properties_change_cb (GDBusProxy         *proxy,
                           GVariant           *changed_properties,
                           const gchar* const *invalidated_properties,
                           gpointer            user_data)
{
  GdkWaylandScreen *screen_wayland = user_data;
  GVariant *value;
  gint64 timestamp;

  if (g_variant_n_children (changed_properties) <= 0)
    return;

  value = g_variant_lookup_value (changed_properties,
                                  "FontconfigTimestamp",
                                  G_VARIANT_TYPE_INT64);

  if (value != NULL)
    {
      timestamp = g_variant_get_int64 (value);
      timestamp = timestamp / G_TIME_SPAN_SECOND;

      if (timestamp > 0 && timestamp <= G_MAXUINT)
        screen_wayland->dbus_settings.fontconfig_timestamp = (guint)timestamp;
      else if (timestamp > G_MAXUINT)
        g_warning ("Could not handle fontconfig update: timestamp out of bound");

      notify_setting (GDK_SCREEN (screen_wayland), "gtk-fontconfig-timestamp");

      g_variant_unref (value);
    }

  value = g_variant_lookup_value (changed_properties,
                                  "Modules",
                                  G_VARIANT_TYPE_STRING);

  if (value != NULL)
    {
      g_free (screen_wayland->dbus_settings.modules);

      screen_wayland->dbus_settings.modules = g_variant_dup_string (value, NULL);

      notify_setting (GDK_SCREEN (screen_wayland), "gtk-modules");

      g_variant_unref (value);
    }
}

static void
fontconfig_dbus_proxy_open_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  GdkWaylandScreen *screen_wayland = user_data;
  GDBusProxy *proxy;
  GVariant *value;
  gint64 timestamp;

  proxy = g_dbus_proxy_new_for_bus_finish (result, NULL);

  if (proxy == NULL)
    return;

  screen_wayland->dbus_proxy = proxy;
  screen_wayland->dbus_setting_change_id =
    g_signal_connect (screen_wayland->dbus_proxy,
                      "g-properties-changed",
                      G_CALLBACK (dbus_properties_change_cb),
                      screen_wayland);

  value = g_dbus_proxy_get_cached_property (screen_wayland->dbus_proxy,
                                            "FontconfigTimestamp");

  if (value && g_variant_is_of_type (value, G_VARIANT_TYPE_INT64))
    {
      timestamp = g_variant_get_int64 (value);
      timestamp = timestamp / G_TIME_SPAN_SECOND;

      if (timestamp > 0 && timestamp <= G_MAXUINT)
        screen_wayland->dbus_settings.fontconfig_timestamp = (guint)timestamp;
      else if (timestamp > G_MAXUINT)
        g_warning ("Could not handle fontconfig init: timestamp out of bound");
    }

  if (value != NULL)
    g_variant_unref (value);

  value = g_dbus_proxy_get_cached_property (screen_wayland->dbus_proxy,
                                            "Modules");

  if (value && g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
    {
      g_free (screen_wayland->dbus_settings.modules);

      screen_wayland->dbus_settings.modules = g_variant_dup_string (value, NULL);
      notify_setting (GDK_SCREEN (screen_wayland), "gtk-modules");
    }

  if (value != NULL)
    g_variant_unref (value);
}

GdkScreen *
_gdk_wayland_screen_new (GdkDisplay *display)
{
  GdkScreen *screen;
  GdkWaylandScreen *screen_wayland;

  screen = g_object_new (GDK_TYPE_WAYLAND_SCREEN, NULL);

  screen_wayland = GDK_WAYLAND_SCREEN (screen);
  screen_wayland->display = display;
  screen_wayland->width = 0;
  screen_wayland->height = 0;

  screen_wayland->visual = gdk_wayland_visual_new (screen);

  screen_wayland->root_window =
    _gdk_wayland_screen_create_root_window (screen,
                                            screen_wayland->width,
                                            screen_wayland->height);

  init_settings (screen);

  return screen;
}

static void
_gdk_wayland_screen_class_init (GdkWaylandScreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkScreenClass *screen_class = GDK_SCREEN_CLASS (klass);

  object_class->dispose = gdk_wayland_screen_dispose;
  object_class->finalize = gdk_wayland_screen_finalize;

  screen_class->get_display = gdk_wayland_screen_get_display;
  screen_class->get_width = gdk_wayland_screen_get_width;
  screen_class->get_height = gdk_wayland_screen_get_height;
  screen_class->get_width_mm = gdk_wayland_screen_get_width_mm;
  screen_class->get_height_mm = gdk_wayland_screen_get_height_mm;
  screen_class->get_number = gdk_wayland_screen_get_number;
  screen_class->get_root_window = gdk_wayland_screen_get_root_window;
  screen_class->get_system_visual = gdk_wayland_screen_get_system_visual;
  screen_class->get_rgba_visual = gdk_wayland_screen_get_rgba_visual;
  screen_class->is_composited = gdk_wayland_screen_is_composited;
  screen_class->make_display_name = gdk_wayland_screen_make_display_name;
  screen_class->get_active_window = gdk_wayland_screen_get_active_window;
  screen_class->get_window_stack = gdk_wayland_screen_get_window_stack;
  screen_class->broadcast_client_message = gdk_wayland_screen_broadcast_client_message;
  screen_class->get_setting = gdk_wayland_screen_get_setting;
  screen_class->visual_get_best_depth = gdk_wayland_screen_visual_get_best_depth;
  screen_class->visual_get_best_type = gdk_wayland_screen_visual_get_best_type;
  screen_class->visual_get_best = gdk_wayland_screen_visual_get_best;
  screen_class->visual_get_best_with_depth = gdk_wayland_screen_visual_get_best_with_depth;
  screen_class->visual_get_best_with_type = gdk_wayland_screen_visual_get_best_with_type;
  screen_class->visual_get_best_with_both = gdk_wayland_screen_visual_get_best_with_both;
  screen_class->query_depths = gdk_wayland_screen_query_depths;
  screen_class->query_visual_types = gdk_wayland_screen_query_visual_types;
  screen_class->list_visuals = gdk_wayland_screen_list_visuals;
}

static void
_gdk_wayland_screen_init (GdkWaylandScreen *screen_wayland)
{
}

static void
update_screen_size (GdkWaylandScreen *screen_wayland)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (screen_wayland->display);
  gboolean emit_changed = FALSE;
  gint width, height;
  gint width_mm, height_mm;
  int i;

  width = height = 0;
  width_mm = height_mm = 0;
  for (i = 0; i < display_wayland->monitors->len; i++)
    {
      GdkMonitor *monitor = display_wayland->monitors->pdata[i];

      /* XXX: Largely assuming here that monitor areas
       * are contiguous and never overlap.
       */
      if (monitor->geometry.x > 0)
        width_mm += monitor->width_mm;
      else
        width_mm = MAX (width_mm, monitor->width_mm);

      if (monitor->geometry.y > 0)
        height_mm += monitor->height_mm;
      else
        height_mm = MAX (height_mm, monitor->height_mm);

      width = MAX (width, monitor->geometry.x + monitor->geometry.width);
      height = MAX (height, monitor->geometry.y + monitor->geometry.height);
    }

  if (screen_wayland->width_mm != width_mm ||
      screen_wayland->height_mm != height_mm)
    {
      emit_changed = TRUE;
      screen_wayland->width_mm = width_mm;
      screen_wayland->height_mm = height_mm;
    }

  if (screen_wayland->width != width ||
      screen_wayland->height != height)
    {
      emit_changed = TRUE;
      screen_wayland->width = width;
      screen_wayland->height = height;
    }

  if (emit_changed)
    g_signal_emit_by_name (screen_wayland, "size-changed");
}

#ifdef G_ENABLE_DEBUG

static const char *
subpixel_to_string (int layout)
{
  int i;
  struct { int layout; const char *name; } layouts[] = {
    { WL_OUTPUT_SUBPIXEL_UNKNOWN, "unknown" },
    { WL_OUTPUT_SUBPIXEL_NONE, "none" },
    { WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB, "rgb" },
    { WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR, "bgr" },
    { WL_OUTPUT_SUBPIXEL_VERTICAL_RGB, "vrgb" },
    { WL_OUTPUT_SUBPIXEL_VERTICAL_BGR, "vbgr" },
    { 0xffffffff, NULL }
  };

  for (i = 0; layouts[i].name; i++)
    {
      if (layouts[i].layout == layout)
        return layouts[i].name;
    }
  return NULL;
}

static const char *
transform_to_string (int transform)
{
  int i;
  struct { int transform; const char *name; } transforms[] = {
    { WL_OUTPUT_TRANSFORM_NORMAL, "normal" },
    { WL_OUTPUT_TRANSFORM_90, "90" },
    { WL_OUTPUT_TRANSFORM_180, "180" },
    { WL_OUTPUT_TRANSFORM_270, "270" },
    { WL_OUTPUT_TRANSFORM_FLIPPED, "flipped" },
    { WL_OUTPUT_TRANSFORM_FLIPPED_90, "flipped 90" },
    { WL_OUTPUT_TRANSFORM_FLIPPED_180, "flipped 180" },
    { WL_OUTPUT_TRANSFORM_FLIPPED_270, "flipped 270" },
    { 0xffffffff, NULL }
  };

  for (i = 0; transforms[i].name; i++)
    {
      if (transforms[i].transform == transform)
        return transforms[i].name;
    }
  return NULL;
}

#endif

static gboolean
screen_has_xdg_output_support (GdkScreen *screen)
{
  GdkDisplay *display = gdk_screen_get_display (screen);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

  return (display_wayland->xdg_output_manager != NULL);
}

static gboolean
monitor_has_xdg_output (GdkWaylandMonitor *monitor)
{
  return (monitor->xdg_output != NULL);
}

static gboolean
should_update_monitor (GdkWaylandMonitor *monitor)
{
  return (GDK_MONITOR (monitor)->geometry.width != 0 &&
          monitor->version < OUTPUT_VERSION_WITH_DONE);
}

static gboolean
should_expect_xdg_output_done (GdkWaylandMonitor *monitor)
{
  GdkDisplay *display = GDK_MONITOR (monitor)->display;
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

  return (monitor_has_xdg_output (monitor) &&
          display_wayland->xdg_output_version < NO_XDG_OUTPUT_DONE_SINCE_VERSION);
}

static void
apply_monitor_change (GdkWaylandMonitor *monitor)
{
  GDK_NOTE (MISC,
            g_message ("monitor %d changed position %d %d, size %d %d",
                       monitor->id, monitor->output_geometry.x,
                       monitor->output_geometry.y,
                       monitor->output_geometry.width,
                       monitor->output_geometry.height));

  GdkDisplay *display = GDK_MONITOR (monitor)->display;
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (gdk_display_get_default_screen (display));

  GdkRectangle logical_geometry;
  gboolean needs_scaling = FALSE;

  if (monitor_has_xdg_output (monitor) &&
      monitor->xdg_output_geometry.width != 0  &&
      monitor->xdg_output_geometry.height != 0)
    {
      logical_geometry = monitor->xdg_output_geometry;
      needs_scaling =
        logical_geometry.width == monitor->output_geometry.width ||
        logical_geometry.height == monitor->output_geometry.height;
    }
  else
    {
      logical_geometry = monitor->output_geometry;
      needs_scaling = TRUE;
    }

  if (needs_scaling)
    {
      int scale = gdk_monitor_get_scale_factor (GDK_MONITOR (monitor));
      logical_geometry.y /= scale;
      logical_geometry.x /= scale;
      logical_geometry.width /= scale;
      logical_geometry.height /= scale;
    }

  gdk_monitor_set_position (GDK_MONITOR (monitor), logical_geometry.x, logical_geometry.y);
  gdk_monitor_set_size (GDK_MONITOR (monitor), logical_geometry.width, logical_geometry.height);
  gdk_monitor_set_connector (GDK_MONITOR (monitor), monitor->name);
  monitor->wl_output_done = FALSE;
  monitor->xdg_output_done = FALSE;

  g_signal_emit_by_name (screen_wayland, "monitors-changed");
  update_screen_size (screen_wayland);
}

static void
xdg_output_handle_logical_position (void                  *data,
                                    struct zxdg_output_v1 *xdg_output,
                                    int32_t                x,
                                    int32_t                y)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *) data;

  GDK_NOTE (MISC,
            g_message ("handle logical position xdg-output %d, position %d %d",
                       monitor->id, x, y));

  monitor->xdg_output_geometry.x = x;
  monitor->xdg_output_geometry.y = y;
}

static void
xdg_output_handle_logical_size (void                  *data,
                                struct zxdg_output_v1 *xdg_output,
                                int32_t                width,
                                int32_t                height)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *) data;

  GDK_NOTE (MISC,
            g_message ("handle logical size xdg-output %d, size %d %d",
                       monitor->id, width, height));

  monitor->xdg_output_geometry.width = width;
  monitor->xdg_output_geometry.height = height;
}

static void
xdg_output_handle_done (void                  *data,
                        struct zxdg_output_v1 *xdg_output)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *) data;

  GDK_NOTE (MISC,
            g_message ("handle done xdg-output %d", monitor->id));

  monitor->xdg_output_done = TRUE;
  if (monitor->wl_output_done && should_expect_xdg_output_done (monitor))
    apply_monitor_change (monitor);
}

static void
xdg_output_handle_name (void                  *data,
                        struct zxdg_output_v1 *xdg_output,
                        const char            *name)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *) data;

  GDK_NOTE (MISC,
            g_message ("handle name xdg-output %d", monitor->id));

  monitor->name = g_strdup (name);
}

static void
xdg_output_handle_description (void                  *data,
                               struct zxdg_output_v1 *xdg_output,
                               const char            *description)
{
#ifdef G_ENABLE_DEBUG
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *) data;

  GDK_NOTE (MISC,
            g_message ("handle description xdg-output %d", monitor->id));
#endif
}

static const struct zxdg_output_v1_listener xdg_output_listener = {
    xdg_output_handle_logical_position,
    xdg_output_handle_logical_size,
    xdg_output_handle_done,
    xdg_output_handle_name,
    xdg_output_handle_description,
};

static void
gdk_wayland_screen_get_xdg_output (GdkWaylandMonitor *monitor)
{
  GdkDisplay *display = GDK_MONITOR (monitor)->display;
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

  GDK_NOTE (MISC,
            g_message ("get xdg-output for monitor %d", monitor->id));

  monitor->xdg_output =
    zxdg_output_manager_v1_get_xdg_output (display_wayland->xdg_output_manager,
                                           monitor->output);

  zxdg_output_v1_add_listener (monitor->xdg_output,
                               &xdg_output_listener,
                               monitor);
}

static void
output_handle_geometry (void             *data,
                        struct wl_output *wl_output,
                        int               x,
                        int               y,
                        int               physical_width,
                        int               physical_height,
                        int               subpixel,
                        const char       *make,
                        const char       *model,
                        int32_t           transform)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *)data;

  GDK_NOTE (MISC,
            g_message ("handle geometry output %d, position %d %d, phys. size %d %d, subpixel layout %s, manufacturer %s, model %s, transform %s",
                       monitor->id, x, y, physical_width, physical_height, subpixel_to_string (subpixel), make, model, transform_to_string (transform)));

  monitor->output_geometry.x = x;
  monitor->output_geometry.y = y;

  switch (transform)
    {
    case WL_OUTPUT_TRANSFORM_90:
    case WL_OUTPUT_TRANSFORM_270:
    case WL_OUTPUT_TRANSFORM_FLIPPED_90:
    case WL_OUTPUT_TRANSFORM_FLIPPED_270:
      gdk_monitor_set_physical_size (GDK_MONITOR (monitor),
                                     physical_height, physical_width);
      break;
    default:
      gdk_monitor_set_physical_size (GDK_MONITOR (monitor),
                                     physical_width, physical_height);
    }

  gdk_monitor_set_subpixel_layout (GDK_MONITOR (monitor), subpixel);
  gdk_monitor_set_manufacturer (GDK_MONITOR (monitor), make);
  gdk_monitor_set_model (GDK_MONITOR (monitor), model);

  if (should_update_monitor (monitor) || !monitor_has_xdg_output (monitor))
    apply_monitor_change (monitor);
}

static void
output_handle_done (void             *data,
                    struct wl_output *wl_output)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *)data;

  GDK_NOTE (MISC,
            g_message ("handle done output %d", monitor->id));

  monitor->wl_output_done = TRUE;

  if (!should_expect_xdg_output_done (monitor) || monitor->xdg_output_done)
    apply_monitor_change (monitor);
}

static void
output_handle_scale (void             *data,
                     struct wl_output *wl_output,
                     int32_t           scale)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *)data;

  GDK_NOTE (MISC,
            g_message ("handle scale output %d, scale %d", monitor->id, scale));

  /* Set the scale from wl_output protocol, regardless of xdg-output support */
  gdk_monitor_set_scale_factor (GDK_MONITOR (monitor), scale);

  if (should_update_monitor (monitor))
    apply_monitor_change (monitor);
}

static void
output_handle_mode (void             *data,
                    struct wl_output *wl_output,
                    uint32_t          flags,
                    int               width,
                    int               height,
                    int               refresh)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *)data;

  GDK_NOTE (MISC,
            g_message ("handle mode output %d, size %d %d, rate %d",
                       monitor->id, width, height, refresh));

  if ((flags & WL_OUTPUT_MODE_CURRENT) == 0)
    return;

  monitor->output_geometry.width = width;
  monitor->output_geometry.height = height;
  gdk_monitor_set_refresh_rate (GDK_MONITOR (monitor), refresh);

  if (should_update_monitor (monitor) || !monitor_has_xdg_output (monitor))
    apply_monitor_change (monitor);
}

static const struct wl_output_listener output_listener =
{
  output_handle_geometry,
  output_handle_mode,
  output_handle_done,
  output_handle_scale,
};

void
_gdk_wayland_screen_add_output (GdkScreen        *screen,
                                guint32           id,
                                struct wl_output *output,
				guint32           version)
{
  GdkDisplay *display = gdk_screen_get_display (screen);
  GdkWaylandMonitor *monitor;

  monitor = g_object_new (GDK_TYPE_WAYLAND_MONITOR,
                          "display", display,
                          NULL);

  monitor->id = id;
  monitor->output = output;
  monitor->version = version;

  g_ptr_array_add (GDK_WAYLAND_DISPLAY (display)->monitors, monitor);
  gdk_display_monitor_added (display, GDK_MONITOR (monitor));
  wl_output_add_listener (output, &output_listener, monitor);

  GDK_NOTE (MISC,
            g_message ("xdg_output_manager %p",
                       GDK_WAYLAND_DISPLAY (display)->xdg_output_manager));

  if (screen_has_xdg_output_support (screen))
    gdk_wayland_screen_get_xdg_output (monitor);
}

struct wl_output *
_gdk_wayland_screen_get_wl_output (GdkScreen *screen,
                                   gint monitor_num)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (GDK_WAYLAND_SCREEN (screen)->display);
  GdkWaylandMonitor *monitor;

  monitor = display_wayland->monitors->pdata[monitor_num];

  return monitor->output;
}

static GdkWaylandMonitor *
get_monitor_for_id (GdkWaylandScreen *screen_wayland,
                    guint32           id)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (screen_wayland->display);
  int i;

  for (i = 0; i < display_wayland->monitors->len; i++)
    {
      GdkWaylandMonitor *monitor = display_wayland->monitors->pdata[i];

      if (monitor->id == id)
        return monitor;
    }

  return NULL;
}

static GdkWaylandMonitor *
get_monitor_for_output (GdkWaylandScreen *screen_wayland,
                        struct wl_output *output)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (screen_wayland->display);
  int i;

  for (i = 0; i < display_wayland->monitors->len; i++)
    {
      GdkWaylandMonitor *monitor = display_wayland->monitors->pdata[i];

      if (monitor->output == output)
        return monitor;
    }

  return NULL;
}

void
_gdk_wayland_screen_remove_output (GdkScreen *screen,
                                   guint32    id)
{
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (screen);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (screen_wayland->display);
  GdkWaylandMonitor *monitor;

  monitor = get_monitor_for_id (screen_wayland, id);
  if (monitor != NULL)
    {
      g_object_ref (monitor);
      g_ptr_array_remove (display_wayland->monitors, monitor);
      gdk_display_monitor_removed (GDK_DISPLAY (display_wayland), GDK_MONITOR (monitor));
      g_object_unref (monitor);
      g_signal_emit_by_name (screen_wayland, "monitors-changed");
      update_screen_size (screen_wayland);
    }
}

int
_gdk_wayland_screen_get_output_refresh_rate (GdkScreen        *screen,
                                             struct wl_output *output)
{
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (screen);
  GdkWaylandMonitor *monitor;

  monitor = get_monitor_for_output (screen_wayland, output);
  if (monitor != NULL)
    return gdk_monitor_get_refresh_rate (GDK_MONITOR (monitor));

  return 0;
}

guint32
_gdk_wayland_screen_get_output_scale (GdkScreen        *screen,
				      struct wl_output *output)
{
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (screen);
  GdkWaylandMonitor *monitor;

  monitor = get_monitor_for_output (screen_wayland, output);
  if (monitor != NULL)
    return gdk_monitor_get_scale_factor (GDK_MONITOR (monitor));

  return 0;
}

void
_gdk_wayland_screen_init_xdg_output (GdkScreen *screen)
{
  GdkDisplay *display = gdk_screen_get_display (screen);
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  int i;

  GDK_NOTE (MISC,
            g_message ("init xdg-output support, %d monitor(s) already present",
                       display_wayland->monitors->len));

  for (i = 0; i < display_wayland->monitors->len; i++)
    gdk_wayland_screen_get_xdg_output (display_wayland->monitors->pdata[i]);
}
