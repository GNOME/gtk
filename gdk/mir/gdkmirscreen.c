/*
 * Copyright © 2014 Canonical Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>

#include "gdkscreenprivate.h"
#include "gdkdisplayprivate.h"
#include "gdkinternals.h"

#include "gdkmir.h"
#include "gdkmir-private.h"

#define VISUAL_TYPE GDK_VISUAL_TRUE_COLOR

typedef struct GdkMirScreen      GdkMirScreen;
typedef struct GdkMirScreenClass GdkMirScreenClass;

#define GDK_TYPE_MIR_SCREEN              (gdk_mir_screen_get_type ())
#define GDK_MIR_SCREEN(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_MIR_SCREEN, GdkMirScreen))
#define GDK_MIR_SCREEN_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_MIR_SCREEN, GdkMirScreenClass))
#define GDK_IS_MIR_SCREEN(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_MIR_SCREEN))
#define GDK_IS_MIR_SCREEN_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_MIR_SCREEN))
#define GDK_MIR_SCREEN_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_MIR_SCREEN, GdkMirScreenClass))

struct GdkMirScreen
{
  GdkScreen parent_instance;

  /* Display this screen is running on */
  GdkDisplay *display;

  /* Current monitor configuration */
  MirDisplayConfig *display_config;

  /* Root window */
  GdkWindow *root_window;

  /* Settings */
  GHashTable *settings_objects;
  GHashTable *current_settings;
};

struct GdkMirScreenClass
{
  GdkScreenClass parent_class;
};

G_DEFINE_TYPE (GdkMirScreen, gdk_mir_screen, GDK_TYPE_SCREEN)

static MirConnection *
get_connection (GdkMirScreen *screen)
{
  return gdk_mir_display_get_mir_connection (GDK_DISPLAY (screen->display));
}

static void
get_screen_size (MirDisplayConfig *config,
                 gint             *width,
                 gint             *height)
{
  const MirOutput *output;
  const MirOutputMode *mode;
  gint right;
  gint bottom;
  gint i;

  *width = 0;
  *height = 0;

  if (!config)
    return;

  for (i = 0; i < mir_display_config_get_num_outputs (config); i++)
    {
      output = mir_display_config_get_output (config, i);

      if (!mir_output_is_enabled (output))
        continue;

      mode = mir_output_get_current_mode (output);

      right = mir_output_get_position_x (output) + mir_output_mode_get_width (mode);
      bottom = mir_output_get_position_y (output) + mir_output_mode_get_height (mode);

      if (right > *width)
        *width = right;

      if (bottom > *height)
        *height = bottom;
    }
}

static void
update_display_config (GdkMirScreen *screen)
{
  gdk_mir_display_get_mir_connection (GDK_DISPLAY (screen->display));
  mir_display_config_release (screen->display_config);
  screen->display_config = mir_connection_create_display_configuration (get_connection (screen));
}

static void
config_changed_cb (MirConnection *connection, void *data)
{
  GdkMirScreen *screen = data;
  gint old_width, old_height, new_width, new_height;

  get_screen_size (screen->display_config, &old_width, &old_height);
  update_display_config (screen);
  get_screen_size (screen->display_config, &new_width, &new_height);

  g_signal_emit_by_name (screen, "monitors-changed");
  if (old_width > 0 && (old_width != new_width || old_height != new_height))
    g_signal_emit_by_name (screen, "size-changed");
}

GdkScreen *
_gdk_mir_screen_new (GdkDisplay *display)
{
  GdkMirScreen *screen;

  screen = g_object_new (GDK_TYPE_MIR_SCREEN, NULL);
  screen->display = display;
  mir_connection_set_display_config_change_callback (get_connection (screen), config_changed_cb, screen);
  update_display_config (screen);

  return GDK_SCREEN (screen);
}

static void
gdk_mir_screen_dispose (GObject *object)
{
  GdkMirScreen *screen = GDK_MIR_SCREEN (object);

  g_clear_pointer (&screen->current_settings, g_hash_table_unref);
  g_clear_pointer (&screen->settings_objects, g_hash_table_unref);

  G_OBJECT_CLASS (gdk_mir_screen_parent_class)->dispose (object);
}

static void
gdk_mir_screen_finalize (GObject *object)
{
  GdkMirScreen *screen = GDK_MIR_SCREEN (object);

  mir_connection_set_display_config_change_callback (get_connection (screen), NULL, NULL);
  mir_display_config_release (screen->display_config);
  g_clear_object (&screen->root_window);

  G_OBJECT_CLASS (gdk_mir_screen_parent_class)->finalize (object);
}

static GdkDisplay *
gdk_mir_screen_get_display (GdkScreen *screen)
{
  return GDK_DISPLAY (GDK_MIR_SCREEN (screen)->display);
}

static const MirOutput *
get_output (GdkScreen *screen,
            gint       monitor_num)
{
  MirDisplayConfig *config;
  const MirOutput *output;
  gint i;
  gint j;

  config = GDK_MIR_SCREEN (screen)->display_config;

  for (i = 0, j = 0; i < mir_display_config_get_num_outputs (config); i++)
    {
      output = mir_display_config_get_output (config, i);

      if (!mir_output_is_enabled (output))
        continue;

      if (j == monitor_num)
        return output;

      j++;
    }

  return NULL;
}

static GdkWindow *
gdk_mir_screen_get_root_window (GdkScreen *screen)
{
  GdkMirScreen *s = GDK_MIR_SCREEN (screen);
  gint width, height;

  if (s->root_window)
     return s->root_window;

  get_screen_size (GDK_MIR_SCREEN (screen)->display_config, &width, &height);

  s->root_window = _gdk_display_create_window (s->display);
  s->root_window->impl_window = s->root_window;
  s->root_window->window_type = GDK_WINDOW_ROOT;
  s->root_window->x = 0;
  s->root_window->y = 0;
  s->root_window->abs_x = 0;
  s->root_window->abs_y = 0;
  s->root_window->width = width;
  s->root_window->height = height;
  s->root_window->viewable = TRUE;
  s->root_window->impl = _gdk_mir_window_impl_new (s->display, s->root_window);

  return s->root_window;
}

static gint
gdk_mir_screen_get_n_monitors (GdkScreen *screen)
{
  MirDisplayConfig *config;
  gint count = 0;
  gint i;

  config = GDK_MIR_SCREEN (screen)->display_config;

  for (i = 0; i < mir_display_config_get_num_outputs (config); i++)
    if (mir_output_is_enabled (mir_display_config_get_output (config, i)))
      count++;

  return count;
}

static gint
gdk_mir_screen_get_primary_monitor (GdkScreen *screen)
{
  return 0; //?
}

static gint
gdk_mir_screen_get_monitor_width_mm	(GdkScreen *screen,
                                     gint       monitor_num)
{
  const MirOutput *output = get_output (screen, monitor_num);

  return output ? mir_output_get_physical_width_mm (output) : 0;
}

static gint
gdk_mir_screen_get_monitor_height_mm (GdkScreen *screen,
                                      gint       monitor_num)
{
  const MirOutput *output = get_output (screen, monitor_num);

  return output ? mir_output_get_physical_height_mm (output) : 0;
}

static gchar *
gdk_mir_screen_get_monitor_plug_name (GdkScreen *screen,
                                      gint       monitor_num)
{
  const MirOutput *output = get_output (screen, monitor_num);

  if (output)
    {
      switch (mir_output_get_type (output))
        {
          case mir_output_type_unknown:
            return g_strdup_printf ("None-%u", mir_output_get_id (output));
          case mir_output_type_vga:
            return g_strdup_printf ("VGA-%u", mir_output_get_id (output));
          case mir_output_type_dvii:
          case mir_output_type_dvid:
          case mir_output_type_dvia:
            return g_strdup_printf ("DVI-%u", mir_output_get_id (output));
          case mir_output_type_composite:
            return g_strdup_printf ("Composite-%u", mir_output_get_id (output));
          case mir_output_type_lvds:
            return g_strdup_printf ("LVDS-%u", mir_output_get_id (output));
          case mir_output_type_component:
            return g_strdup_printf ("CTV-%u", mir_output_get_id (output));
          case mir_output_type_ninepindin:
            return g_strdup_printf ("DIN-%u", mir_output_get_id (output));
          case mir_output_type_displayport:
            return g_strdup_printf ("DP-%u", mir_output_get_id (output));
          case mir_output_type_hdmia:
          case mir_output_type_hdmib:
            return g_strdup_printf ("HDMI-%u", mir_output_get_id (output));
          case mir_output_type_svideo:
          case mir_output_type_tv:
            return g_strdup_printf ("TV-%u", mir_output_get_id (output));
          case mir_output_type_edp:
            return g_strdup_printf ("eDP-%u", mir_output_get_id (output));
          case mir_output_type_virtual:
            return g_strdup_printf ("Virtual-%u", mir_output_get_id (output));
          case mir_output_type_dsi:
            return g_strdup_printf ("DSI-%u", mir_output_get_id (output));
          case mir_output_type_dpi:
            return g_strdup_printf ("DPI-%u", mir_output_get_id (output));
        }
    }

  return NULL;
}

static void
gdk_mir_screen_get_monitor_geometry (GdkScreen    *screen,
                                     gint          monitor_num,
                                     GdkRectangle *dest)
{
  const MirOutput *output;
  const MirOutputMode *mode;

  output = get_output (screen, monitor_num);

  if (output)
    {
      mode = mir_output_get_current_mode (output);

      dest->x = mir_output_get_position_x (output);
      dest->y = mir_output_get_position_y (output);
      dest->width = mir_output_mode_get_width (mode);
      dest->height = mir_output_mode_get_height (mode);
    }
  else
    {
      dest->x = 0;
      dest->y = 0;
      dest->width = 0;
      dest->height = 0;
    }
}

static void
gdk_mir_screen_get_monitor_workarea (GdkScreen    *screen,
                                     gint          monitor_num,
                                     GdkRectangle *dest)
{
  // FIXME: Don't know what this is
  gdk_mir_screen_get_monitor_geometry (screen, monitor_num, dest);
}

static void setting_changed (GSettings    *settings,
                             const gchar  *key,
                             GdkMirScreen *screen);

static GSettings *
get_settings (GdkMirScreen *screen,
              const gchar  *schema_id)
{
  GSettings *settings;
  GSettingsSchemaSource *source;
  GSettingsSchema *schema;

  settings = g_hash_table_lookup (screen->settings_objects, schema_id);

  if (settings)
    return g_object_ref (settings);

  source = g_settings_schema_source_get_default ();

  if (!source)
    {
      g_warning ("no schemas installed");
      return NULL;
    }

  schema = g_settings_schema_source_lookup (source, schema_id, TRUE);

  if (!schema)
    {
      g_warning ("schema not found: %s", schema_id);
      return NULL;
    }

  settings = g_settings_new_full (schema, NULL, NULL);
  g_signal_connect (settings, "changed", G_CALLBACK (setting_changed), screen);
  g_hash_table_insert (screen->settings_objects, g_strdup (schema_id), g_object_ref (settings));
  g_settings_schema_unref (schema);
  return settings;
}

static GVariant *
read_setting (GdkMirScreen *screen,
              const gchar  *schema_id,
              const gchar  *key)
{
  GSettings *settings;
  GVariant *variant;

  settings = get_settings (screen, schema_id);

  if (!settings)
    return NULL;

  variant = g_settings_get_value (settings, key);
  g_object_unref (settings);
  return variant;
}

static void
change_setting (GdkMirScreen *screen,
                const gchar  *name,
                GVariant     *variant)
{
  GVariant *old_variant;
  GdkEventSetting event;

  old_variant = g_hash_table_lookup (screen->current_settings, name);

  if (variant == old_variant)
    return;

  if (variant && old_variant && g_variant_equal (variant, old_variant))
    return;

  event.type = GDK_SETTING;
  event.window = gdk_screen_get_root_window (GDK_SCREEN (screen));
  event.send_event = FALSE;
  event.name = g_strdup (name);

  if (variant)
    {
      event.action = old_variant ? GDK_SETTING_ACTION_CHANGED : GDK_SETTING_ACTION_NEW;
      g_hash_table_insert (screen->current_settings, g_strdup (name), g_variant_ref_sink (variant));
    }
  else
    {
      event.action = GDK_SETTING_ACTION_DELETED;
      g_hash_table_remove (screen->current_settings, name);
    }

  gdk_event_put ((const GdkEvent *) &event);
  g_free (event.name);
}

static const struct
{
  const gchar *name;
  const gchar *schema_id;
  const gchar *key;
} SETTINGS_MAP[] = {
  {
    "gtk-double-click-time",
    "org.gnome.settings-daemon.peripherals.mouse",
    "double-click"
  },
  {
    "gtk-cursor-blink",
    "org.gnome.desktop.interface",
    "cursor-blink"
  },
  {
    "gtk-cursor-blink-time",
    "org.gnome.desktop.interface",
    "cursor-blink-time"
  },
  {
    "gtk-cursor-blink-timeout",
    "org.gnome.desktop.interface",
    "cursor-blink-timeout"
  },
  {
    "gtk-theme-name",
    "org.gnome.desktop.interface",
    "gtk-theme"
  },
  {
    "gtk-icon-theme-name",
    "org.gnome.desktop.interface",
    "icon-theme"
  },
  {
    "gtk-key-theme-name",
    "org.gnome.desktop.interface",
    "gtk-key-theme"
  },
  {
    "gtk-dnd-drag-threshold",
    "org.gnome.settings-daemon.peripherals.mouse",
    "drag-threshold"
  },
  {
    "gtk-font-name",
    "org.gnome.desktop.interface",
    "font-name"
  },
  {
    "gtk-xft-antialias",
    "org.gnome.settings-daemon.plugins.xsettings",
    "antialiasing"
  },
  {
    "gtk-xft-hinting",
    "org.gnome.settings-daemon.plugins.xsettings",
    "hinting"
  },
  {
    "gtk-xft-hintstyle",
    "org.gnome.settings-daemon.plugins.xsettings",
    "hinting"
  },
  {
    "gtk-xft-rgba",
    "org.gnome.settings-daemon.plugins.xsettings",
    "rgba-order"
  },
  {
    "gtk-xft-dpi",
    "org.gnome.desktop.interface",
    "text-scaling-factor"
  },
  {
    "gtk-cursor-theme-name",
    "org.gnome.desktop.interface",
    "cursor-theme"
  },
  {
    "gtk-cursor-theme-size",
    "org.gnome.desktop.interface",
    "cursor-size"
  },
  {
    "gtk-enable-animations",
    "org.gnome.desktop.interface",
    "enable-animations"
  },
  {
    "gtk-im-module",
    "org.gnome.desktop.interface",
    "gtk-im-module"
  },
  {
    "gtk-recent-files-max-age",
    "org.gnome.desktop.privacy",
    "recent-files-max-age"
  },
  {
    "gtk-sound-theme-name",
    "org.gnome.desktop.sound",
    "theme-name"
  },
  {
    "gtk-enable-input-feedback-sounds",
    "org.gnome.desktop.sound",
    "input-feedback-sounds"
  },
  {
    "gtk-enable-event-sounds",
    "org.gnome.desktop.sound",
    "event-sounds"
  },
  {
    "gtk-shell-shows-desktop",
    "org.gnome.desktop.background",
    "show-desktop-icons"
  },
  {
    "gtk-decoration-layout",
    "org.gnome.desktop.wm.preferences",
    "button-layout"
  },
  {
    "gtk-titlebar-double-click",
    "org.gnome.desktop.wm.preferences",
    "action-double-click-titlebar"
  },
  {
    "gtk-titlebar-middle-click",
    "org.gnome.desktop.wm.preferences",
    "action-middle-click-titlebar"
  },
  {
    "gtk-titlebar-right-click",
    "org.gnome.desktop.wm.preferences",
    "action-right-click-titlebar"
  },
  {
    "gtk-enable-primary-paste",
    "org.gnome.desktop.interface",
    "gtk-enable-primary-paste"
  },
  {
    "gtk-recent-files-enabled",
    "org.gnome.desktop.privacy",
    "remember-recent-files"
  },
  {
    "gtk-keynav-use-caret",
    "org.gnome.desktop.a11y",
    "always-show-text-caret"
  },
  { NULL }
};

static guint
get_scaling_factor (GdkMirScreen *screen)
{
  GVariant *variant;
  guint scaling_factor;

  variant = read_setting (screen, "org.gnome.desktop.interface", "scaling-factor");

  if (!variant)
    {
      g_warning ("no scaling factor: org.gnome.desktop.interface.scaling-factor");
      variant = g_variant_ref_sink (g_variant_new_uint32 (0));
    }

  scaling_factor = g_variant_get_uint32 (variant);
  g_variant_unref (variant);

  if (scaling_factor)
    return scaling_factor;

  scaling_factor = 1;

  /* TODO: scaling_factor = 2 if HiDPI >= 2 * 96 */

  return scaling_factor;
}

static void
update_setting (GdkMirScreen *screen,
                const gchar  *name)
{
  GVariant *variant;
  GVariant *antialiasing_variant;
  gboolean gtk_xft_antialias;
  gboolean gtk_xft_hinting;
  gdouble text_scaling_factor;
  gint cursor_size;
  gint i;

  if (!g_strcmp0 (name, "gtk-modules"))
    {
      /* TODO: X-GTK-Module-Enabled-Schema, X-GTK-Module-Enabled-Key */
      /* TODO: org.gnome.settings-daemon.plugins.xsettings.enabled-gtk-modules */
      /* TODO: org.gnome.settings-daemon.plugins.xsettings.disabled-gtk-modules */
      return;
    }
  else
    {
      for (i = 0; SETTINGS_MAP[i].name; i++)
        if (!g_strcmp0 (name, SETTINGS_MAP[i].name))
          break;

      if (!SETTINGS_MAP[i].name)
        return;

      variant = read_setting (screen, SETTINGS_MAP[i].schema_id, SETTINGS_MAP[i].key);

      if (!variant)
        {
          g_warning ("no setting for %s: %s.%s", SETTINGS_MAP[i].name, SETTINGS_MAP[i].schema_id, SETTINGS_MAP[i].key);
          return;
        }
    }

  if (!g_strcmp0 (name, "gtk-xft-antialias"))
    {
      gtk_xft_antialias = g_strcmp0 (g_variant_get_string (variant, NULL), "none");
      g_variant_unref (variant);
      variant = g_variant_ref_sink (g_variant_new_int32 (gtk_xft_antialias ? 1 : 0));
    }
  else if (!g_strcmp0 (name, "gtk-xft-hinting"))
    {
      gtk_xft_hinting = g_strcmp0 (g_variant_get_string (variant, NULL), "none");
      g_variant_unref (variant);
      variant = g_variant_ref_sink (g_variant_new_int32 (gtk_xft_hinting ? 1 : 0));
    }
  else if (!g_strcmp0 (name, "gtk-xft-hintstyle"))
    {
      if (!g_strcmp0 (g_variant_get_string (variant, NULL), "none"))
        {
          g_variant_unref (variant);
          variant = g_variant_ref_sink (g_variant_new_string ("hintnone"));
        }
      else if (!g_strcmp0 (g_variant_get_string (variant, NULL), "slight"))
        {
          g_variant_unref (variant);
          variant = g_variant_ref_sink (g_variant_new_string ("hintslight"));
        }
      else if (!g_strcmp0 (g_variant_get_string (variant, NULL), "medium"))
        {
          g_variant_unref (variant);
          variant = g_variant_ref_sink (g_variant_new_string ("hintmedium"));
        }
      else if (!g_strcmp0 (g_variant_get_string (variant, NULL), "full"))
        {
          g_variant_unref (variant);
          variant = g_variant_ref_sink (g_variant_new_string ("hintfull"));
        }
      else
        {
          g_warning ("unknown org.gnome.settings-daemon.plugins.xsettings.hinting value: %s", g_variant_get_string (variant, NULL));
          g_variant_unref (variant);
          return;
        }
    }
  else if (!g_strcmp0 (name, "gtk-xft-rgba"))
    {
      antialiasing_variant = read_setting (screen, "org.gnome.settings-daemon.plugins.xsettings", "antialiasing");

      if (g_strcmp0 (g_variant_get_string (antialiasing_variant, NULL), "rgba"))
        {
          g_variant_unref (variant);
          variant = g_variant_ref_sink (g_variant_new_string ("none"));
        }
      else if (g_strcmp0 (g_variant_get_string (variant, NULL), "rgba"))
        {
          g_variant_unref (variant);
          variant = g_variant_ref_sink (g_variant_new_string ("rgb"));
        }

      g_variant_unref (antialiasing_variant);
    }
  else if (!g_strcmp0 (name, "gtk-xft-dpi"))
    {
      text_scaling_factor = g_variant_get_double (variant);
      g_variant_unref (variant);
      variant = g_variant_ref_sink (g_variant_new_int32 (1024 * get_scaling_factor (screen) * text_scaling_factor + 0.5));
    }
  else if (!g_strcmp0 (name, "gtk-cursor-theme-size"))
    {
      cursor_size = g_variant_get_int32 (variant);
      g_variant_unref (variant);
      variant = g_variant_ref_sink (g_variant_new_int32 (get_scaling_factor (screen) * cursor_size));
    }
  else if (!g_strcmp0 (name, "gtk-enable-animations"))
    {
      /* TODO: disable under vnc/vino/llvmpipe */
    }

  change_setting (screen, name, variant);
  g_variant_unref (variant);
}

static void
setting_changed (GSettings    *settings,
                 const gchar  *key,
                 GdkMirScreen *screen)
{
  gchar *schema_id;
  gint i;

  g_object_get (settings, "schema-id", &schema_id, NULL);

  for (i = 0; SETTINGS_MAP[i].name; i++)
    if (!g_strcmp0 (schema_id, SETTINGS_MAP[i].schema_id) && !g_strcmp0 (key, SETTINGS_MAP[i].key))
      update_setting (screen, SETTINGS_MAP[i].name);

  if (!g_strcmp0 (schema_id, "org.gnome.settings-daemon.plugins.xsettings"))
    {
      if (!g_strcmp0 (key, "enabled-gtk-modules"))
        update_setting (screen, "gtk-modules");
      else if (!g_strcmp0 (key, "disabled-gtk-modules"))
        update_setting (screen, "gtk-modules");
      else if (!g_strcmp0 (key, "antialiasing"))
        update_setting (screen, "rgba-order");
    }
  else if (!g_strcmp0 (schema_id, "org.gnome.desktop.interface"))
    {
      if (!g_strcmp0 (key, "scaling-factor"))
        {
          update_setting (screen, "gtk-xft-dpi");
          update_setting (screen, "gtk-cursor-theme-size");
        }
    }

  g_free (schema_id);
}

static const gchar * const KNOWN_SETTINGS[] =
{
  "gtk-double-click-time",
  "gtk-double-click-distance",
  "gtk-cursor-blink",
  "gtk-cursor-blink-time",
  "gtk-cursor-blink-timeout",
  "gtk-split-cursor",
  "gtk-theme-name",
  "gtk-icon-theme-name",
  "gtk-key-theme-name",
  "gtk-dnd-drag-threshold",
  "gtk-font-name",
  "gtk-modules",
  "gtk-xft-antialias",
  "gtk-xft-hinting",
  "gtk-xft-hintstyle",
  "gtk-xft-rgba",
  "gtk-xft-dpi",
  "gtk-cursor-theme-name",
  "gtk-cursor-theme-size",
  "gtk-alternative-button-order",
  "gtk-alternative-sort-arrows",
  "gtk-enable-animations",
  "gtk-error-bell",
  "gtk-print-backends",
  "gtk-print-preview-command",
  "gtk-enable-accels",
  "gtk-im-module",
  "gtk-recent-files-max-age",
  "gtk-fontconfig-timestamp",
  "gtk-sound-theme-name",
  "gtk-enable-input-feedback-sounds",
  "gtk-enable-event-sounds",
  "gtk-primary-button-warps-slider",
  "gtk-application-prefer-dark-theme",
  "gtk-entry-select-on-focus",
  "gtk-entry-password-hint-timeout",
  "gtk-label-select-on-focus",
  "gtk-shell-shows-app-menu",
  "gtk-shell-shows-menubar",
  "gtk-shell-shows-desktop",
  "gtk-decoration-layout",
  "gtk-titlebar-double-click",
  "gtk-titlebar-middle-click",
  "gtk-titlebar-right-click",
  "gtk-dialogs-use-header",
  "gtk-enable-primary-paste",
  "gtk-recent-files-enabled",
  "gtk-long-press-time",
  "gtk-keynav-use-caret",
  NULL
};

static gboolean
gdk_mir_screen_get_setting (GdkScreen   *screen,
                            const gchar *name,
                            GValue      *value)
{
  GdkMirScreen *mir_screen;
  GVariant *variant;

  mir_screen = GDK_MIR_SCREEN (screen);
  variant = g_hash_table_lookup (mir_screen->current_settings, name);

  if (!variant)
    update_setting (mir_screen, name);

  variant = g_hash_table_lookup (mir_screen->current_settings, name);

  if (!variant)
    {
      if (!g_strv_contains (KNOWN_SETTINGS, name))
        g_warning ("unknown setting: %s", name);

      return FALSE;
    }

  g_dbus_gvariant_to_gvalue (variant, value);
  return TRUE;
}

static gint
gdk_mir_screen_get_monitor_scale_factor (GdkScreen *screen,
                                         gint       monitor_num)
{
  /* Don't support monitor scaling */
  return 1;
}

static void
gdk_mir_screen_init (GdkMirScreen *screen)
{
  screen->settings_objects = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  screen->current_settings = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_variant_unref);
}

static void
gdk_mir_screen_class_init (GdkMirScreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkScreenClass *screen_class = GDK_SCREEN_CLASS (klass);

  object_class->dispose = gdk_mir_screen_dispose;
  object_class->finalize = gdk_mir_screen_finalize;

  screen_class->get_display = gdk_mir_screen_get_display;
  screen_class->get_root_window = gdk_mir_screen_get_root_window;
  screen_class->get_n_monitors = gdk_mir_screen_get_n_monitors;
  screen_class->get_primary_monitor = gdk_mir_screen_get_primary_monitor;
  screen_class->get_monitor_width_mm = gdk_mir_screen_get_monitor_width_mm;
  screen_class->get_monitor_height_mm = gdk_mir_screen_get_monitor_height_mm;
  screen_class->get_monitor_plug_name = gdk_mir_screen_get_monitor_plug_name;
  screen_class->get_monitor_geometry = gdk_mir_screen_get_monitor_geometry;
  screen_class->get_monitor_workarea = gdk_mir_screen_get_monitor_workarea;
  screen_class->get_setting = gdk_mir_screen_get_setting;
  screen_class->get_monitor_scale_factor = gdk_mir_screen_get_monitor_scale_factor;
}
