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
#include "gdkwayland.h"
#include "gdkprivate-wayland.h"

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

typedef struct _GdkWaylandMonitor GdkWaylandMonitor;

struct _GdkWaylandScreen
{
  GdkScreen parent_instance;

  GdkDisplay *display;
  GdkWindow *root_window;

  int width, height;
  int width_mm, height_mm;

  /* Visual Part */
  GdkVisual *visual;

  /* Xinerama/RandR 1.2 */
  GPtrArray *monitors;
  gint       primary_monitor;

  GHashTable *settings;
  GsdXftSettings xft_settings;

  guint32    shell_capabilities;
};

struct _GdkWaylandScreenClass
{
  GdkScreenClass parent_class;

  void (* window_manager_changed) (GdkWaylandScreen *screen_wayland);
};

#define OUTPUT_VERSION_WITH_DONE 2

struct _GdkWaylandMonitor
{
  GdkWaylandScreen *screen;
  guint32       id;
  guint32       version;
  struct wl_output *output;
  GdkRectangle  geometry;
  int		width_mm;
  int		height_mm;
  char *	output_name;
  char *	manufacturer;
  int		refresh_rate;
  gint          scale;
};

GType _gdk_wayland_screen_get_type (void);

G_DEFINE_TYPE (GdkWaylandScreen, _gdk_wayland_screen, GDK_TYPE_SCREEN)

static void
free_monitor (gpointer data)
{
  GdkWaylandMonitor *monitor = data;

  if (monitor == NULL)
    return;

  g_free (monitor->output_name);
  g_free (monitor->manufacturer);

  g_free (monitor);
}

static void
deinit_multihead (GdkScreen *screen)
{
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (screen);

  g_ptr_array_free (screen_wayland->monitors, TRUE);

  screen_wayland->monitors = NULL;
}

static void
init_multihead (GdkScreen *screen)
{
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (screen);

  screen_wayland->monitors = g_ptr_array_new_with_free_func (free_monitor);
  screen_wayland->primary_monitor = 0;
}

static void
gdk_wayland_screen_dispose (GObject *object)
{
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (object);

  if (screen_wayland->root_window)
    _gdk_window_destroy (screen_wayland->root_window, FALSE);

  G_OBJECT_CLASS (_gdk_wayland_screen_parent_class)->dispose (object);
}

static void
gdk_wayland_screen_finalize (GObject *object)
{
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (object);

  if (screen_wayland->root_window)
    g_object_unref (screen_wayland->root_window);

  g_object_unref (screen_wayland->visual);

  deinit_multihead (GDK_SCREEN (object));

  g_hash_table_destroy (screen_wayland->settings);

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

static gint
gdk_wayland_screen_get_n_monitors (GdkScreen *screen)
{
  return GDK_WAYLAND_SCREEN (screen)->monitors->len;
}

static gint
gdk_wayland_screen_get_primary_monitor (GdkScreen *screen)
{
  return GDK_WAYLAND_SCREEN (screen)->primary_monitor;
}

static gint
gdk_wayland_screen_get_monitor_width_mm	(GdkScreen *screen,
					 gint       monitor_num)
{
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (screen);
  GdkWaylandMonitor *monitor = g_ptr_array_index(screen_wayland->monitors, monitor_num);

  return monitor->width_mm;
}

static gint
gdk_wayland_screen_get_monitor_height_mm (GdkScreen *screen,
					  gint       monitor_num)
{
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (screen);
  GdkWaylandMonitor *monitor = g_ptr_array_index(screen_wayland->monitors, monitor_num);

  return monitor->height_mm;
}

static gchar *
gdk_wayland_screen_get_monitor_plug_name (GdkScreen *screen,
					  gint       monitor_num)
{
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (screen);
  GdkWaylandMonitor *monitor = g_ptr_array_index(screen_wayland->monitors, monitor_num);

  return g_strdup (monitor->output_name);
}

static void
gdk_wayland_screen_get_monitor_geometry (GdkScreen    *screen,
					 gint          monitor_num,
					 GdkRectangle *dest)
{
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (screen);
  GdkWaylandMonitor *monitor = g_ptr_array_index(screen_wayland->monitors, monitor_num);

  if (dest)
    *dest = monitor->geometry;
}

static gint
gdk_wayland_screen_get_monitor_scale_factor (GdkScreen *screen,
					     gint       monitor_num)
{
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (screen);
  GdkWaylandMonitor *monitor;

  if (monitor_num >= screen_wayland->monitors->len)
    return 1;

  monitor = g_ptr_array_index(screen_wayland->monitors, monitor_num);

  return monitor->scale;
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
  return NULL;
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

typedef enum
{
  GSD_FONT_HINTING_NONE,
  GSD_FONT_HINTING_SLIGHT,
  GSD_FONT_HINTING_MEDIUM,
  GSD_FONT_HINTING_FULL
} GsdFontHinting;

typedef enum
{
  GSD_FONT_RGBA_ORDER_RGBA,
  GSD_FONT_RGBA_ORDER_RGB,
  GSD_FONT_RGBA_ORDER_BGR,
  GSD_FONT_RGBA_ORDER_VRGB,
  GSD_FONT_RGBA_ORDER_VBGR
} GsdFontRgbaOrder;

static gdouble
get_dpi_from_gsettings (GdkWaylandScreen *screen_wayland)
{
        GSettings  *interface_settings;
        gdouble      factor;

        interface_settings = g_hash_table_lookup (screen_wayland->settings,
                                                  "org.gnome.desktop.interface");
        if (interface_settings != NULL)
          factor = g_settings_get_double (interface_settings, "text-scaling-factor");
        else
          factor = 1.0;

        return 96.0 * factor;
}

static void
update_xft_settings (GdkScreen *screen)
{
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (screen);
  GSettings *settings;
  GsdFontAntialiasingMode antialiasing;
  GsdFontHinting hinting;
  GsdFontRgbaOrder order;
  gboolean use_rgba = FALSE;
  GsdXftSettings xft_settings;

  settings = g_hash_table_lookup (screen_wayland->settings, "org.gnome.settings-daemon.plugins.xsettings");

  if (settings)
    {
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

  xft_settings.antialias = (antialiasing != GSD_FONT_ANTIALIASING_MODE_NONE);
  xft_settings.hinting = (hinting != GSD_FONT_HINTING_NONE);
  xft_settings.dpi = get_dpi_from_gsettings (screen_wayland) * 1024; /* Xft wants 1/1024ths of an inch */
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
      notify_setting (screen, "gtk-xft-antialias");
    }

  if (screen_wayland->xft_settings.hinting != xft_settings.hinting)
    {
      screen_wayland->xft_settings.hinting = xft_settings.hinting;
      notify_setting (screen, "gtk-xft-hinting");
    }

  if (screen_wayland->xft_settings.hintstyle != xft_settings.hintstyle)
    {
      screen_wayland->xft_settings.hintstyle = xft_settings.hintstyle;
      notify_setting (screen, "gtk-xft-hintstyle");
    }

  if (screen_wayland->xft_settings.rgba != xft_settings.rgba)
    {
      screen_wayland->xft_settings.rgba = xft_settings.rgba;
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

      notify_setting (screen, "gtk-xft-dpi");
    }
}

#define WM_SETTINGS_SCHEMA "org.gnome.desktop.wm.preferences"
#define CLASSIC_WM_SETTINGS_SCHEMA "org.gnome.shell.extensions.classic-overrides"

typedef struct _TranslationEntry TranslationEntry;
struct _TranslationEntry {
  const gchar *schema;
  const gchar *key;
  const gchar *setting;
  GType type;
  union {
    const gchar *s;
    gint         i;
    gboolean     b;
  } fallback;
};

static TranslationEntry translations[] = {
  { "org.gnome.desktop.interface", "gtk-theme", "gtk-theme-name" , G_TYPE_STRING, { .s = "Adwaita" } },
  { "org.gnome.desktop.interface", "icon-theme", "gtk-icon-theme-name", G_TYPE_STRING, { .s = "gnome" } },
  { "org.gnome.desktop.interface", "cursor-theme", "gtk-cursor-theme-name", G_TYPE_STRING, { .s = "Adwaita" } },
  { "org.gnome.desktop.interface", "cursor-size", "gtk-cursor-theme-size", G_TYPE_INT, { .i = 32 } },
  { "org.gnome.desktop.interface", "font-name", "gtk-font-name", G_TYPE_STRING, { .s = "Cantarell 11" } },
  { "org.gnome.desktop.interface", "cursor-blink", "gtk-cursor-blink", G_TYPE_BOOLEAN,  { .b = TRUE } },
  { "org.gnome.desktop.interface", "cursor-blink-time", "gtk-cursor-blink-time", G_TYPE_INT, { .i = 1200 } },
  { "org.gnome.desktop.interface", "cursor-blink-timeout", "gtk-cursor-blink-timeout", G_TYPE_INT, { .i = 3600 } },
  { "org.gnome.desktop.interface", "gtk-im-module", "gtk-im-module", G_TYPE_STRING, { .s = "simple" } },
  { "org.gnome.desktop.interface", "enable-animations", "gtk-enable-animations", G_TYPE_BOOLEAN, { .b = TRUE } },
  { "org.gnome.settings-daemon.peripherals.mouse", "double-click", "gtk-double-click-time", G_TYPE_INT, { .i = 250 } },
  { "org.gnome.settings-daemon.peripherals.mouse", "drag-threshold", "gtk-dnd-drag-threshold", G_TYPE_INT, {.i = 8 } },
  { "org.gnome.desktop.sound", "theme-name", "gtk-sound-theme-name", G_TYPE_STRING, { .s = "freedesktop" } },
  { "org.gnome.desktop.sound", "event-sounds", "gtk-enable-event-sounds", G_TYPE_BOOLEAN, { .b = TRUE } },
  { "org.gnome.desktop.sound", "input-feedback-sounds", "gtk-enable-input-feedback-sounds", G_TYPE_BOOLEAN, { . b = FALSE } },
  { "org.gnome.desktop.privacy", "recent-files-max-age", "gtk-recent-files-max-age", G_TYPE_INT, { .i = 30 } },
  { "org.gnome.desktop.privacy", "remember-recent-files",    "gtk-recent-files-enabled", G_TYPE_BOOLEAN, { .b = TRUE } },
  { WM_SETTINGS_SCHEMA, "button-layout",    "gtk-decoration-layout", G_TYPE_STRING, { .s = "menu:close" } },
  { CLASSIC_WM_SETTINGS_SCHEMA, "button-layout",   "gtk-decoration-layout", G_TYPE_STRING, { .s = "menu:close" } },
  { "org.gnome.settings-daemon.plugins.xsettings", "antialiasing", "gtk-xft-antialias", G_TYPE_NONE, { .i = 0 } },
  { "org.gnome.settings-daemon.plugins.xsettings", "hinting", "gtk-xft-hinting", G_TYPE_NONE, { .i = 0 } },
  { "org.gnome.settings-daemon.plugins.xsettings", "hinting", "gtk-xft-hintstyle", G_TYPE_NONE, { .i = 0 } },
  { "org.gnome.settings-daemon.plugins.xsettings", "rgba-order", "gtk-xft-rgba", G_TYPE_NONE, { .i = 0 } },
  { "org.gnome.desktop.interface", "text-scaling-factor", "gtk-xft-dpi" , G_TYPE_NONE, { .i = 0 } },

};

static TranslationEntry *
find_translation_entry_by_key (GSettings *settings, const gchar *key)
{
  guint i;
  gchar *schema;

  g_object_get (settings, "schema", &schema, NULL);

  for (i = 0; i < G_N_ELEMENTS (translations); i++)
    {
      if (g_str_equal (schema, translations[i].schema) &&
          g_str_equal (key, translations[i].key))
        {
          g_free (schema);
          return &translations[i];
        }
    }

  g_free (schema);

  return NULL;
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
      else
        update_xft_settings (screen);
    }
}

static void
init_settings (GdkScreen *screen)
{
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (screen);
  GSettingsSchemaSource *source;
  GSettingsSchema *schema;
  GSettings *settings;
  gint i;

  screen_wayland->settings = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);

  source = g_settings_schema_source_get_default ();

  for (i = 0; i < G_N_ELEMENTS (translations); i++)
    {
      if (g_hash_table_lookup (screen_wayland->settings, (gpointer)translations[i].schema) != NULL)
        continue;

      schema = g_settings_schema_source_lookup (source, translations[i].schema, FALSE);
      if (schema != NULL)
        {
          settings = g_settings_new_full (schema, NULL, NULL);
          g_signal_connect (settings, "changed",
                            G_CALLBACK (settings_changed), screen);
          g_hash_table_insert (screen_wayland->settings, (gpointer)translations[i].schema, settings);
          g_settings_schema_unref (schema);
        }
    }

  update_xft_settings (screen);
}

static void
gtk_shell_handle_capabilities (void             *data,
			       struct gtk_shell *shell,
			       uint32_t          capabilities)
{
  GdkScreen *screen = data;
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (data);

  screen_wayland->shell_capabilities = capabilities;

  notify_setting (screen, "gtk-shell-shows-app-menu");
  notify_setting (screen, "gtk-shell-shows-menubar");
  notify_setting (screen, "gtk-shell-shows-desktop");
}

struct gtk_shell_listener gdk_screen_gtk_shell_listener = {
  gtk_shell_handle_capabilities
};

void
_gdk_wayland_screen_set_has_gtk_shell (GdkScreen *screen)
{
  GdkWaylandDisplay *wayland_display = GDK_WAYLAND_DISPLAY (GDK_WAYLAND_SCREEN (screen)->display);

  gtk_shell_add_listener (wayland_display->gtk_shell, &gdk_screen_gtk_shell_listener, screen);
}

static void
set_value_from_entry (GdkScreen        *screen,
                      TranslationEntry *entry,
                      GValue           *value)
{
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (screen);
  GSettings *settings;

  settings = (GSettings *)g_hash_table_lookup (screen_wayland->settings, entry->schema);
  switch (entry->type)
    {
    case G_TYPE_STRING:
      if (settings)
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
      g_value_set_int (value, settings != NULL
                              ? g_settings_get_int (settings, entry->key)
                              : entry->fallback.i);
      break;
    case G_TYPE_BOOLEAN:
      g_value_set_boolean (value, settings != NULL
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

static gboolean
set_capability_setting (GdkScreen                 *screen,
			GValue                    *value,
			enum gtk_shell_capability  test)
{
  GdkWaylandScreen *wayland_screen = GDK_WAYLAND_SCREEN (screen);

  g_value_set_boolean (value, (wayland_screen->shell_capabilities & test) == test);

  return TRUE;
}

static gboolean
gdk_wayland_screen_get_setting (GdkScreen   *screen,
				const gchar *name,
				GValue      *value)
{
  TranslationEntry *entry;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);

  entry = find_translation_entry_by_setting (name);
  if (entry != NULL)
    {
      if (strcmp (name, "gtk-decoration-layout") == 0)
        set_decoration_layout_from_entry (screen, entry, value);
      else
        set_value_from_entry (screen, entry, value);
      return TRUE;
   }

  if (strcmp (name, "gtk-shell-shows-app-menu") == 0)
    return set_capability_setting (screen, value, GTK_SHELL_CAPABILITY_GLOBAL_APP_MENU);

  if (strcmp (name, "gtk-shell-shows-menubar") == 0)
    return set_capability_setting (screen, value, GTK_SHELL_CAPABILITY_GLOBAL_MENU_BAR);

  if (strcmp (name, "gtk-shell-shows-desktop") == 0)
    return set_capability_setting (screen, value, GTK_SHELL_CAPABILITY_DESKTOP_ICONS);

  if (strcmp (name, "gtk-dialogs-use-header") == 0)
    {
      g_value_set_boolean (value, TRUE);
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
  return GDK_WAYLAND_SCREEN (screen)->visual;
}

static GdkVisual*
gdk_wayland_screen_visual_get_best_with_type (GdkScreen     *screen,
					      GdkVisualType  visual_type)
{
  return GDK_WAYLAND_SCREEN (screen)->visual;
}

static GdkVisual*
gdk_wayland_screen_visual_get_best_with_both (GdkScreen     *screen,
					      gint           depth,
					      GdkVisualType  visual_type)
{
  return GDK_WAYLAND_SCREEN (screen)->visual;
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

static GdkVisual *
gdk_wayland_visual_new (GdkScreen *screen)
{
  GdkVisual *visual;

  visual = g_object_new (GDK_TYPE_WAYLAND_VISUAL, NULL);
  visual->screen = GDK_SCREEN (screen);
  visual->type = GDK_VISUAL_TRUE_COLOR;
  visual->depth = 32;

  return visual;
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

  init_multihead (screen);

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
  screen_class->get_n_monitors = gdk_wayland_screen_get_n_monitors;
  screen_class->get_primary_monitor = gdk_wayland_screen_get_primary_monitor;
  screen_class->get_monitor_width_mm = gdk_wayland_screen_get_monitor_width_mm;
  screen_class->get_monitor_height_mm = gdk_wayland_screen_get_monitor_height_mm;
  screen_class->get_monitor_plug_name = gdk_wayland_screen_get_monitor_plug_name;
  screen_class->get_monitor_geometry = gdk_wayland_screen_get_monitor_geometry;
  screen_class->get_monitor_workarea = gdk_wayland_screen_get_monitor_geometry;
  screen_class->get_monitor_scale_factor = gdk_wayland_screen_get_monitor_scale_factor;
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
  gint width, height;
  gint i;

  width = height = 0;
  for (i = 0; i < screen_wayland->monitors->len; i++)
    {
      GdkWaylandMonitor *monitor = screen_wayland->monitors->pdata[i];

      width = MAX (width, monitor->geometry.x + monitor->geometry.width);
      height = MAX (height, monitor->geometry.y + monitor->geometry.height);
    }

  if (screen_wayland->width != width ||
      screen_wayland->height != height)
    {
      screen_wayland->width = width;
      screen_wayland->height = height;
      g_signal_emit_by_name (screen_wayland, "size-changed");
    }
}

static void
output_handle_geometry(void *data,
		       struct wl_output *wl_output,
		       int x, int y, int physical_width, int physical_height,
		       int subpixel, const char *make, const char *model,
		       int32_t transform)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *)data;

  monitor->geometry.x = x;
  monitor->geometry.y = y;

  monitor->width_mm = physical_width;
  monitor->height_mm = physical_height;

  monitor->manufacturer = g_strdup (make);
  monitor->output_name = g_strdup (model);

  if (monitor->geometry.width != 0 && monitor->version < OUTPUT_VERSION_WITH_DONE)
    {
      g_signal_emit_by_name (monitor->screen, "monitors-changed");
      update_screen_size (monitor->screen);
    }
}

static void
output_handle_done(void *data,
		   struct wl_output *wl_output)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *)data;

  g_signal_emit_by_name (monitor->screen, "monitors-changed");
  update_screen_size (monitor->screen);
}

static void
output_handle_scale(void *data,
		    struct wl_output *wl_output,
		    int32_t factor)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *)data;

  monitor->scale = factor;
  g_signal_emit_by_name (monitor->screen, "monitors-changed");
}

static void
output_handle_mode(void *data,
                   struct wl_output *wl_output,
                   uint32_t flags,
                   int width,
                   int height,
                   int refresh)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *)data;

  if ((flags & WL_OUTPUT_MODE_CURRENT) == 0)
    return;

  monitor->geometry.width = width;
  monitor->geometry.height = height;
  monitor->refresh_rate = refresh;

  if (monitor->geometry.width != 0 && monitor->version < OUTPUT_VERSION_WITH_DONE)
    {
      g_signal_emit_by_name (monitor->screen, "monitors-changed");
      update_screen_size (monitor->screen);
    }
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
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (screen);
  GdkWaylandMonitor *monitor = g_new0(GdkWaylandMonitor, 1);

  monitor->id = id;
  monitor->output = output;
  monitor->version = version;
  monitor->screen = screen_wayland;
  monitor->scale = 1;
  g_ptr_array_add(screen_wayland->monitors, monitor);

  wl_output_add_listener(output, &output_listener, monitor);
}

void
_gdk_wayland_screen_remove_output (GdkScreen *screen,
                                   guint32    id)
{
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (screen);
  int i;

  for (i = 0; i < screen_wayland->monitors->len; i++)
    {
      GdkWaylandMonitor *monitor = screen_wayland->monitors->pdata[i];

      if (monitor->id == id)
        {
          g_ptr_array_remove (screen_wayland->monitors, monitor);

          g_signal_emit_by_name (screen_wayland, "monitors-changed");
          update_screen_size (screen_wayland);
          break;
        }
    }
}

int
_gdk_wayland_screen_get_output_refresh_rate (GdkScreen        *screen,
                                             struct wl_output *output)
{
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (screen);
  int i;

  for (i = 0; i < screen_wayland->monitors->len; i++)
    {
      GdkWaylandMonitor *monitor = screen_wayland->monitors->pdata[i];

      if (monitor->output == output)
        return monitor->refresh_rate;
    }

  return 0;
}

guint32
_gdk_wayland_screen_get_output_scale (GdkScreen        *screen,
				      struct wl_output *output)
{
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (screen);
  int i;

  for (i = 0; i < screen_wayland->monitors->len; i++)
    {
      GdkWaylandMonitor *monitor = screen_wayland->monitors->pdata[i];

      if (monitor->output == output)
        return monitor->scale;
    }

  return 0;
}
