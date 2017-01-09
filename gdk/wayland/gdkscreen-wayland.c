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
#include "gdkdisplay.h"
#include "gdkdisplay-wayland.h"
#include "gdkmonitor-wayland.h"
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


struct _GdkWaylandScreen
{
  GdkScreen parent_instance;

  GdkDisplay *display;
  GdkWindow *root_window;

  GHashTable *settings;
  GsdXftSettings xft_settings;

  guint32    shell_capabilities;
};

struct _GdkWaylandScreenClass
{
  GdkScreenClass parent_class;
};

#define OUTPUT_VERSION_WITH_DONE 2


GType _gdk_wayland_screen_get_type (void);

G_DEFINE_TYPE (GdkWaylandScreen, _gdk_wayland_screen, GDK_TYPE_SCREEN)

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

  g_hash_table_destroy (screen_wayland->settings);

  G_OBJECT_CLASS (_gdk_wayland_screen_parent_class)->finalize (object);
}

static GdkDisplay *
gdk_wayland_screen_get_display (GdkScreen *screen)
{
  return GDK_WAYLAND_SCREEN (screen)->display;
}

static GdkWindow *
gdk_wayland_screen_get_root_window (GdkScreen *screen)
{
  return GDK_WAYLAND_SCREEN (screen)->root_window;
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
  gboolean valid;
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
  { FALSE, "org.gnome.desktop.interface", "gtk-theme", "gtk-theme-name" , G_TYPE_STRING, { .s = "Adwaita" } },
  { FALSE, "org.gnome.desktop.interface", "icon-theme", "gtk-icon-theme-name", G_TYPE_STRING, { .s = "gnome" } },
  { FALSE, "org.gnome.desktop.interface", "cursor-theme", "gtk-cursor-theme-name", G_TYPE_STRING, { .s = "Adwaita" } },
  { FALSE, "org.gnome.desktop.interface", "cursor-size", "gtk-cursor-theme-size", G_TYPE_INT, { .i = 32 } },
  { FALSE, "org.gnome.desktop.interface", "font-name", "gtk-font-name", G_TYPE_STRING, { .s = "Cantarell 11" } },
  { FALSE, "org.gnome.desktop.interface", "cursor-blink", "gtk-cursor-blink", G_TYPE_BOOLEAN,  { .b = TRUE } },
  { FALSE, "org.gnome.desktop.interface", "cursor-blink-time", "gtk-cursor-blink-time", G_TYPE_INT, { .i = 1200 } },
  { FALSE, "org.gnome.desktop.interface", "cursor-blink-timeout", "gtk-cursor-blink-timeout", G_TYPE_INT, { .i = 3600 } },
  { FALSE, "org.gnome.desktop.interface", "gtk-im-module", "gtk-im-module", G_TYPE_STRING, { .s = "simple" } },
  { FALSE, "org.gnome.desktop.interface", "enable-animations", "gtk-enable-animations", G_TYPE_BOOLEAN, { .b = TRUE } },
  { FALSE, "org.gnome.desktop.interface", "gtk-enable-primary-paste", "gtk-enable-primary-paste", G_TYPE_BOOLEAN, { .b = TRUE } },
  { FALSE, "org.gnome.settings-daemon.peripherals.mouse", "double-click", "gtk-double-click-time", G_TYPE_INT, { .i = 400 } },
  { FALSE, "org.gnome.settings-daemon.peripherals.mouse", "drag-threshold", "gtk-dnd-drag-threshold", G_TYPE_INT, {.i = 8 } },
  { FALSE, "org.gnome.desktop.sound", "theme-name", "gtk-sound-theme-name", G_TYPE_STRING, { .s = "freedesktop" } },
  { FALSE, "org.gnome.desktop.sound", "event-sounds", "gtk-enable-event-sounds", G_TYPE_BOOLEAN, { .b = TRUE } },
  { FALSE, "org.gnome.desktop.sound", "input-feedback-sounds", "gtk-enable-input-feedback-sounds", G_TYPE_BOOLEAN, { . b = FALSE } },
  { FALSE, "org.gnome.desktop.privacy", "recent-files-max-age", "gtk-recent-files-max-age", G_TYPE_INT, { .i = 30 } },
  { FALSE, "org.gnome.desktop.privacy", "remember-recent-files",    "gtk-recent-files-enabled", G_TYPE_BOOLEAN, { .b = TRUE } },
  { FALSE, WM_SETTINGS_SCHEMA, "button-layout",    "gtk-decoration-layout", G_TYPE_STRING, { .s = "menu:close" } },
  { FALSE, CLASSIC_WM_SETTINGS_SCHEMA, "button-layout",   "gtk-decoration-layout", G_TYPE_STRING, { .s = "menu:close" } },
  { FALSE, "org.gnome.settings-daemon.plugins.xsettings", "antialiasing", "gtk-xft-antialias", G_TYPE_NONE, { .i = 0 } },
  { FALSE, "org.gnome.settings-daemon.plugins.xsettings", "hinting", "gtk-xft-hinting", G_TYPE_NONE, { .i = 0 } },
  { FALSE, "org.gnome.settings-daemon.plugins.xsettings", "hinting", "gtk-xft-hintstyle", G_TYPE_NONE, { .i = 0 } },
  { FALSE, "org.gnome.settings-daemon.plugins.xsettings", "rgba-order", "gtk-xft-rgba", G_TYPE_NONE, { .i = 0 } },
  { FALSE, "org.gnome.desktop.interface", "text-scaling-factor", "gtk-xft-dpi" , G_TYPE_NONE, { .i = 0 } },
  { FALSE, "org.gnome.desktop.wm.preferences", "action-double-click-titlebar", "gtk-titlebar-double-click", G_TYPE_STRING, { .s = "toggle-maximize" } },
  { FALSE, "org.gnome.desktop.wm.preferences", "action-middle-click-titlebar", "gtk-titlebar-middle-click", G_TYPE_STRING, { .s = "none" } },
  { FALSE, "org.gnome.desktop.wm.preferences", "action-right-click-titlebar", "gtk-titlebar-right-click", G_TYPE_STRING, { .s = "menu" } },
  { FALSE, "org.gnome.desktop.a11y", "always-show-text-caret", "gtk-keynav-use-caret", G_TYPE_BOOLEAN, { .b = FALSE } }
};

static TranslationEntry *
find_translation_entry_by_key (GSettings   *settings,
                               const gchar *key)
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

  update_xft_settings (screen);
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
                        enum gtk_shell1_capability test)
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

  return FALSE;
}

GdkScreen *
_gdk_wayland_screen_new (GdkDisplay *display)
{
  GdkScreen *screen;
  GdkWaylandScreen *screen_wayland;

  screen = g_object_new (GDK_TYPE_WAYLAND_SCREEN, NULL);

  screen_wayland = GDK_WAYLAND_SCREEN (screen);
  screen_wayland->display = display;

  screen_wayland->root_window =
    _gdk_wayland_screen_create_root_window (screen, 0, 0);

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
  screen_class->get_root_window = gdk_wayland_screen_get_root_window;
  screen_class->get_setting = gdk_wayland_screen_get_setting;
}

static void
_gdk_wayland_screen_init (GdkWaylandScreen *screen_wayland)
{
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

  gdk_monitor_set_position (GDK_MONITOR (monitor), x, y);
  gdk_monitor_set_physical_size (GDK_MONITOR (monitor), physical_width, physical_height);
  gdk_monitor_set_subpixel_layout (GDK_MONITOR (monitor), subpixel);
  gdk_monitor_set_manufacturer (GDK_MONITOR (monitor), make);
  gdk_monitor_set_model (GDK_MONITOR (monitor), model);

  if (GDK_MONITOR (monitor)->geometry.width != 0 && monitor->version < OUTPUT_VERSION_WITH_DONE)
    {
      GdkDisplay *display = GDK_MONITOR (monitor)->display;
      GdkWaylandScreen *screen = GDK_WAYLAND_SCREEN (gdk_display_get_default_screen (display));
      g_signal_emit_by_name (screen, "monitors-changed");
    }
}

static void
output_handle_done (void             *data,
                    struct wl_output *wl_output)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *)data;
  GdkDisplay *display = gdk_monitor_get_display (GDK_MONITOR (monitor));
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (gdk_display_get_default_screen (display));

  GDK_NOTE (MISC,
            g_message ("handle done output %d", monitor->id));

  if (!monitor->added)
    {
      monitor->added = TRUE;
      g_ptr_array_add (GDK_WAYLAND_DISPLAY (display)->monitors, monitor);
      gdk_display_monitor_added (display, GDK_MONITOR (monitor));
    }

  g_signal_emit_by_name (screen_wayland, "monitors-changed");
}

static void
output_handle_scale (void             *data,
                     struct wl_output *wl_output,
                     int32_t           scale)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *)data;

  GDK_NOTE (MISC,
            g_message ("handle scale output %d, scale %d", monitor->id, scale));

  gdk_monitor_set_scale_factor (GDK_MONITOR (monitor), scale);

  if (GDK_MONITOR (monitor)->geometry.width != 0 && monitor->version < OUTPUT_VERSION_WITH_DONE)
    {
      GdkScreen *screen = gdk_display_get_default_screen (GDK_MONITOR (monitor)->display);
      g_signal_emit_by_name (screen, "monitors-changed");
    }
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

  gdk_monitor_set_size (GDK_MONITOR (monitor), width, height);
  gdk_monitor_set_refresh_rate (GDK_MONITOR (monitor), refresh);

  if (width != 0 && monitor->version < OUTPUT_VERSION_WITH_DONE)
    {
      GdkScreen *screen = gdk_display_get_default_screen (GDK_MONITOR (monitor)->display);
      g_signal_emit_by_name (screen, "monitors-changed");
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
  GdkDisplay *display = gdk_screen_get_display (screen);
  GdkWaylandMonitor *monitor;

  monitor = g_object_new (GDK_TYPE_WAYLAND_MONITOR,
                          "display", display,
                          NULL);

  monitor->id = id;
  monitor->output = output;
  monitor->version = version;

  if (monitor->version < OUTPUT_VERSION_WITH_DONE)
    {
      g_ptr_array_add (GDK_WAYLAND_DISPLAY (display)->monitors, monitor);
      gdk_display_monitor_added (display, GDK_MONITOR (monitor));
    }

  wl_output_add_listener (output, &output_listener, monitor);
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
