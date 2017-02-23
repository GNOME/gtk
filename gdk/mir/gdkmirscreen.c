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

  GdkWindow *root_window;
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
  //g_printerr ("gdk_mir_screen_get_display\n");
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
  //g_printerr ("gdk_mir_screen_get_root_window\n");
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
  //g_printerr ("gdk_mir_screen_get_n_monitors\n");
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
  //g_printerr ("gdk_mir_screen_get_primary_monitor\n");
  return 0; //?
}

static gint
gdk_mir_screen_get_monitor_width_mm	(GdkScreen *screen,
                                     gint       monitor_num)
{
  //g_printerr ("gdk_mir_screen_get_monitor_width_mm (%d)\n", monitor_num);
  const MirOutput *output = get_output (screen, monitor_num);

  return output ? mir_output_get_physical_width_mm (output) : 0;
}

static gint
gdk_mir_screen_get_monitor_height_mm (GdkScreen *screen,
                                      gint       monitor_num)
{
  //g_printerr ("gdk_mir_screen_get_monitor_height_mm (%d)\n", monitor_num);
  const MirOutput *output = get_output (screen, monitor_num);

  return output ? mir_output_get_physical_height_mm (output) : 0;
}

static gchar *
gdk_mir_screen_get_monitor_plug_name (GdkScreen *screen,
                                      gint       monitor_num)
{
  //g_printerr ("gdk_mir_screen_get_monitor_plug_name (%d)\n", monitor_num);
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
  //g_printerr ("gdk_mir_screen_get_monitor_geometry (%d)\n", monitor_num);
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
  //g_printerr ("gdk_mir_screen_get_monitor_workarea (%d)\n", monitor_num);
  // FIXME: Don't know what this is
  gdk_mir_screen_get_monitor_geometry (screen, monitor_num, dest);
}

static gboolean
gdk_mir_screen_get_setting (GdkScreen   *screen,
                            const gchar *name,
                            GValue      *value)
{
  //g_printerr ("gdk_mir_screen_get_setting (\"%s\")\n", name);

  if (strcmp (name, "gtk-theme-name") == 0)
    {
      g_value_set_string (value, "Ambiance");
      return TRUE;
    }

  if (strcmp (name, "gtk-font-name") == 0)
    {
      g_value_set_string (value, "Ubuntu");
      return TRUE;
    }

  if (strcmp (name, "gtk-enable-animations") == 0)
    {
      g_value_set_boolean (value, TRUE);
      return TRUE;
    }

  if (strcmp (name, "gtk-xft-dpi") == 0)
    {
      g_value_set_int (value, 96 * 1024);
      return TRUE;
    }

  if (strcmp (name, "gtk-xft-antialias") == 0)
    {
      g_value_set_int (value, TRUE);
      return TRUE;
    }

  if (strcmp (name, "gtk-xft-hinting") == 0)
    {
      g_value_set_int (value, TRUE);
      return TRUE;
    }

  if (strcmp (name, "gtk-xft-hintstyle") == 0)
    {
      g_value_set_static_string (value, "hintfull");
      return TRUE;
    }

  if (strcmp (name, "gtk-xft-rgba") == 0)
    {
      g_value_set_static_string (value, "rgba");
      return TRUE;
    }

  if (g_str_equal (name, "gtk-modules"))
    {
      g_value_set_string (value, NULL);
      return TRUE;
    }

  if (g_str_equal (name, "gtk-application-prefer-dark-theme"))
    {
      g_value_set_boolean (value, FALSE);
      return TRUE;
    }

  if (g_str_equal (name, "gtk-key-theme-name"))
    {
      g_value_set_string (value, NULL);
      return TRUE;
    }

  if (g_str_equal (name, "gtk-double-click-time"))
    {
      g_value_set_int (value, 250);
      return TRUE;
    }

  if (g_str_equal (name, "gtk-double-click-distance"))
    {
      g_value_set_int (value, 5);
      return TRUE;
    }

  if (g_str_equal (name, "gtk-cursor-theme-name"))
    {
      g_value_set_string (value, "Raleigh");
      return TRUE;
    }

  if (g_str_equal (name, "gtk-cursor-theme-size"))
    {
      g_value_set_int (value, 128);
      return TRUE;
    }

  if (g_str_equal (name, "gtk-icon-theme-name"))
    {
      g_value_set_string (value, "hicolor");
      return TRUE;
    }

  if (g_str_equal (name, "gtk-shell-shows-app-menu"))
    {
      g_value_set_boolean (value, FALSE);
      return TRUE;
    }

  if (g_str_equal (name, "gtk-shell-shows-menubar"))
    {
      g_value_set_boolean (value, FALSE);
      return TRUE;
    }

  if (g_str_equal (name, "gtk-shell-shows-desktop"))
    {
      g_value_set_boolean (value, FALSE);
      return TRUE;
    }

  if (g_str_equal (name, "gtk-recent-files-enabled"))
    {
      g_value_set_boolean (value, FALSE);
      return TRUE;
    }

  if (g_str_equal (name, "gtk-alternative-sort-arrows"))
    {
      g_value_set_boolean (value, FALSE);
      return TRUE;
    }

  if (g_str_equal (name, "gtk-enable-accels"))
    {
      g_value_set_boolean (value, TRUE);
      return TRUE;
    }

  if (g_str_equal (name, "gtk-enable-mnemonics"))
    {
      g_value_set_boolean (value, TRUE);
      return TRUE;
    }

  if (g_str_equal (name, "gtk-menu-images"))
    {
      g_value_set_boolean (value, FALSE);
      return TRUE;
    }

  if (g_str_equal (name, "gtk-button-images"))
    {
      g_value_set_boolean (value, FALSE);
      return TRUE;
    }

  if (g_str_equal (name, "gtk-split-cursor"))
    {
      g_value_set_boolean (value, TRUE);
      return TRUE;
    }

  if (g_str_equal (name, "gtk-im-module"))
    {
      g_value_set_string (value, NULL);
      return TRUE;
    }

  if (g_str_equal (name, "gtk-menu-bar-accel"))
    {
      g_value_set_string (value, "F10");
      return TRUE;
    }

  if (g_str_equal (name, "gtk-cursor-blink"))
    {
      g_value_set_boolean (value, TRUE);
      return TRUE;
    }

  if (g_str_equal (name, "gtk-cursor-blink-time"))
    {
      g_value_set_int (value, 1200);
      return TRUE;
    }

  if (g_str_equal (name, "gtk-cursor-blink-timeout"))
    {
      g_value_set_int (value, 10);
      return TRUE;
    }

  if (g_str_equal (name, "gtk-entry-select-on-focus"))
    {
      g_value_set_boolean (value, FALSE);
      return TRUE;
    }

  if (g_str_equal (name, "gtk-error-bell"))
    {
      g_value_set_boolean (value, FALSE);
      return TRUE;
    }

  if (g_str_equal (name, "gtk-label-select-on-focus"))
    {
      g_value_set_boolean (value, FALSE);
      return TRUE;
    }

  if (g_str_equal (name, "gtk-decoration-layout"))
    {
      g_value_set_string (value, "menu:minimize,maximize,close");
      return TRUE;
    }

  if (g_str_equal (name, "gtk-dnd-drag-threshold"))
    {
      g_value_set_int (value, 8);
      return TRUE;
    }

  if (g_str_equal (name, "gtk-dialogs-use-header"))
    {
      g_value_set_boolean (value, FALSE);
      return TRUE;
    }

  if (g_str_equal (name, "gtk-long-press-time"))
    {
      g_value_set_uint (value, 500);
      return TRUE;
    }

  if (g_str_equal (name, "gtk-primary-button-warps-slider"))
    {
      g_value_set_boolean (value, TRUE);
      return TRUE;
    }

  if (g_str_equal (name, "gtk-recent-files-max-age"))
    {
      g_value_set_int (value, 30);
      return TRUE;
    }

  if (g_str_equal (name, "gtk-titlebar-double-click"))
    {
      g_value_set_string (value, "toggle-maximize");
      return TRUE;
    }

  g_warning ("unknown property %s", name);

  return FALSE;
}

static gint
gdk_mir_screen_get_monitor_scale_factor (GdkScreen *screen,
                                         gint       monitor_num)
{
  //g_printerr ("gdk_mir_screen_get_monitor_scale_factor (%d)\n", monitor_num);
  /* Don't support monitor scaling */
  return 1;
}

static void
gdk_mir_screen_init (GdkMirScreen *screen)
{
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
