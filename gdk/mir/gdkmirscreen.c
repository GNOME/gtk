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
#include "gdkvisualprivate.h"
#include "gdkinternals.h"

#include "gdkmir.h"
#include "gdkmir-private.h"

#define VISUAL_TYPE GDK_VISUAL_TRUE_COLOR
#define VISUAL_DEPTH 32

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
  MirDisplayConfiguration *display_config;

  GdkVisual *visual;

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
get_screen_size (MirDisplayConfiguration *config, gint *width, gint *height)
{
  uint32_t i;

  *width = 0;
  *height = 0;

  if (!config)
    return;

  for (i = 0; i < config->num_outputs; i++)
    {
      MirDisplayOutput *o = &config->outputs[i];
      gint w, h;

      if (!o->used)
        continue;

      w = o->position_x + o->modes[o->current_mode].horizontal_resolution;
      if (w > *width)
        *width = w;
      h = o->position_y + o->modes[o->current_mode].vertical_resolution;
      if (h > *height)
        *height = h;
    }
}

static void
update_display_config (GdkMirScreen *screen)
{
  gdk_mir_display_get_mir_connection (GDK_DISPLAY (screen->display));
  mir_display_config_destroy (screen->display_config);
  screen->display_config = mir_connection_create_display_config (get_connection (screen));
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
  mir_connection_set_display_config_change_callback (get_connection (screen), config_changed_cb, display);
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
  G_OBJECT_CLASS (gdk_mir_screen_parent_class)->finalize (object);
}

static GdkDisplay *
gdk_mir_screen_get_display (GdkScreen *screen)
{
  //g_printerr ("gdk_mir_screen_get_display\n");
  return GDK_DISPLAY (GDK_MIR_SCREEN (screen)->display);
}

static MirDisplayOutput *
get_output (GdkScreen *screen, gint monitor_num)
{
  MirDisplayConfiguration *config;
  uint32_t i, j;

  config = GDK_MIR_SCREEN (screen)->display_config;

  for (i = 0, j = 0; i < config->num_outputs; i++)
    {
      if (!config->outputs[i].used)
        continue;

      if (j == monitor_num)
        return &config->outputs[i];
      j++;
    }

  return NULL;
}

static gint
gdk_mir_screen_get_width (GdkScreen *screen)
{
  g_printerr ("gdk_mir_screen_get_width\n");
  gint width, height;
  get_screen_size (GDK_MIR_SCREEN (screen)->display_config, &width, &height);
  return width;
}

static gint
gdk_mir_screen_get_height (GdkScreen *screen)
{
  g_printerr ("gdk_mir_screen_get_height\n");
  gint width, height;
  get_screen_size (GDK_MIR_SCREEN (screen)->display_config, &width, &height);
  return height;
}

static gint
gdk_mir_screen_get_width_mm (GdkScreen *screen)
{
  g_printerr ("gdk_mir_screen_get_width_mm\n");
  // FIXME: A combination of all screens?
  return get_output (screen, 0)->physical_width_mm;
}

static gint
gdk_mir_screen_get_height_mm (GdkScreen *screen)
{
  g_printerr ("gdk_mir_screen_get_height_mm\n");
  // FIXME: A combination of all screens?
  return get_output (screen, 0)->physical_height_mm;
}

static gint
gdk_mir_screen_get_number (GdkScreen *screen)
{
  //g_printerr ("gdk_mir_screen_get_number\n");
  /* There is only one screen... */
  return 0;
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
  s->root_window->impl = _gdk_mir_window_impl_new ();
  s->root_window->impl_window = s->root_window;
  s->root_window->visual = s->visual;
  s->root_window->window_type = GDK_WINDOW_ROOT;
  s->root_window->depth = VISUAL_DEPTH;
  s->root_window->x = 0;
  s->root_window->y = 0;
  s->root_window->abs_x = 0;
  s->root_window->abs_y = 0;
  s->root_window->width = width;
  s->root_window->height = height;
  s->root_window->viewable = TRUE;

  return s->root_window;
}

static gint
gdk_mir_screen_get_n_monitors (GdkScreen *screen)
{
  //g_printerr ("gdk_mir_screen_get_n_monitors\n");
  MirDisplayConfiguration *config;
  uint32_t i;
  gint count = 0;

  config = GDK_MIR_SCREEN (screen)->display_config;

  for (i = 0; i < config->num_outputs; i++)
    if (config->outputs[i].used)
      count++;

  return count;
}

static gint
gdk_mir_screen_get_primary_monitor (GdkScreen *screen)
{
  g_printerr ("gdk_mir_screen_get_primary_monitor\n");
  return 0; //?
}

static gint
gdk_mir_screen_get_monitor_width_mm	(GdkScreen *screen,
                                     gint       monitor_num)
{
  g_printerr ("gdk_mir_screen_get_monitor_width_mm (%d)\n", monitor_num);
  return get_output (screen, monitor_num)->physical_width_mm;
}

static gint
gdk_mir_screen_get_monitor_height_mm (GdkScreen *screen,
                                      gint       monitor_num)
{
  g_printerr ("gdk_mir_screen_get_monitor_height_mm (%d)\n", monitor_num);
  return get_output (screen, monitor_num)->physical_height_mm;
}

static gchar *
gdk_mir_screen_get_monitor_plug_name (GdkScreen *screen,
                                      gint       monitor_num)
{
  g_printerr ("gdk_mir_screen_get_monitor_plug_name (%d)\n", monitor_num);
  return NULL; //?
}

static void
gdk_mir_screen_get_monitor_geometry (GdkScreen    *screen,
                                     gint          monitor_num,
                                     GdkRectangle *dest)
{
  //g_printerr ("gdk_mir_screen_get_monitor_geometry (%d)\n", monitor_num);
  MirDisplayOutput *output;
  MirDisplayMode *mode;

  output = get_output (screen, monitor_num);
  mode = &output->modes[output->current_mode];
  dest->x = output->position_x;
  dest->y = output->position_y;
  dest->width = mode->horizontal_resolution;
  dest->height = mode->vertical_resolution;
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

static GList *
gdk_mir_screen_list_visuals (GdkScreen *screen)
{
  g_printerr ("gdk_mir_screen_list_visuals\n");
  return g_list_append (NULL, GDK_MIR_SCREEN (screen)->visual);
}

static GdkVisual *
gdk_mir_screen_get_system_visual (GdkScreen *screen)
{
  //g_printerr ("gdk_mir_screen_get_system_visual\n");
  return GDK_MIR_SCREEN (screen)->visual;
}

static GdkVisual *
gdk_mir_screen_get_rgba_visual (GdkScreen *screen)
{
  //g_printerr ("gdk_mir_screen_get_rgba_visual\n");
  return GDK_MIR_SCREEN (screen)->visual;
}

static gboolean
gdk_mir_screen_is_composited (GdkScreen *screen)
{
  //g_printerr ("gdk_mir_screen_is_composited\n");
  /* We're always composited */
  return TRUE;
}

static gchar *
gdk_mir_screen_make_display_name (GdkScreen *screen)
{
  g_printerr ("gdk_mir_screen_make_display_name\n");
  return NULL; // FIXME
}

static GdkWindow *
gdk_mir_screen_get_active_window (GdkScreen *screen)
{
  g_printerr ("gdk_mir_screen_get_active_window\n");
  return NULL; // FIXME
}

static GList *
gdk_mir_screen_get_window_stack (GdkScreen *screen)
{
  g_printerr ("gdk_mir_screen_get_window_stack\n");
  return NULL; // FIXME
}

static void
gdk_mir_screen_broadcast_client_message (GdkScreen *screen,
                                         GdkEvent  *event)
{
  g_printerr ("gdk_mir_screen_broadcast_client_message\n");
  // FIXME
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

  g_error ("unknown property %s", name);

  return FALSE;
}

static gint
gdk_mir_screen_visual_get_best_depth (GdkScreen *screen)
{
  g_printerr ("gdk_mir_screen_visual_get_best_depth\n");
  return VISUAL_DEPTH;
}

static GdkVisualType
gdk_mir_screen_visual_get_best_type (GdkScreen *screen)
{
  g_printerr ("gdk_mir_screen_visual_get_best_type\n");
  return VISUAL_TYPE;
}

static GdkVisual*
gdk_mir_screen_visual_get_best (GdkScreen *screen)
{
  g_printerr ("gdk_mir_screen_visual_get_best\n");
  return GDK_MIR_SCREEN (screen)->visual;
}

static GdkVisual*
gdk_mir_screen_visual_get_best_with_depth (GdkScreen *screen,
                                           gint       depth)
{
  g_printerr ("gdk_mir_screen_visual_get_best_with_depth (%d)\n", depth);
  return GDK_MIR_SCREEN (screen)->visual;
}

static GdkVisual*
gdk_mir_screen_visual_get_best_with_type (GdkScreen     *screen,
                                          GdkVisualType  visual_type)
{
  g_printerr ("gdk_mir_screen_visual_get_best_with_type (%d)\n", visual_type);
  return GDK_MIR_SCREEN (screen)->visual;
}

static GdkVisual*
gdk_mir_screen_visual_get_best_with_both (GdkScreen     *screen,
                                          gint           depth,
                                          GdkVisualType  visual_type)
{
  g_printerr ("gdk_mir_screen_visual_get_best_with_both\n");
  return GDK_MIR_SCREEN (screen)->visual;
}

static void
gdk_mir_screen_query_depths (GdkScreen  *screen,
                             gint      **depths,
                             gint       *count)
{
  g_printerr ("gdk_mir_screen_query_depths\n");
  static gint supported_depths[] = { VISUAL_DEPTH };
  *depths = supported_depths;
  *count = 1;
}

static void
gdk_mir_screen_query_visual_types (GdkScreen      *screen,
                                   GdkVisualType **visual_types,
                                   gint           *count)
{
  g_printerr ("gdk_mir_screen_query_visual_types\n");
  static GdkVisualType supported_visual_types[] = { VISUAL_TYPE };
  *visual_types = supported_visual_types;
  *count = 1;
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
  screen->visual = g_object_new (GDK_TYPE_VISUAL, NULL);
  screen->visual->screen = GDK_SCREEN (screen);
  screen->visual->type = VISUAL_TYPE;
  screen->visual->depth = VISUAL_DEPTH;
}

static void
gdk_mir_screen_class_init (GdkMirScreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkScreenClass *screen_class = GDK_SCREEN_CLASS (klass);

  object_class->dispose = gdk_mir_screen_dispose;
  object_class->finalize = gdk_mir_screen_finalize;

  screen_class->get_display = gdk_mir_screen_get_display;
  screen_class->get_width = gdk_mir_screen_get_width;
  screen_class->get_height = gdk_mir_screen_get_height;
  screen_class->get_width_mm = gdk_mir_screen_get_width_mm;
  screen_class->get_height_mm = gdk_mir_screen_get_height_mm;
  screen_class->get_number = gdk_mir_screen_get_number;
  screen_class->get_root_window = gdk_mir_screen_get_root_window;
  screen_class->get_n_monitors = gdk_mir_screen_get_n_monitors;
  screen_class->get_primary_monitor = gdk_mir_screen_get_primary_monitor;
  screen_class->get_monitor_width_mm = gdk_mir_screen_get_monitor_width_mm;
  screen_class->get_monitor_height_mm = gdk_mir_screen_get_monitor_height_mm;
  screen_class->get_monitor_plug_name = gdk_mir_screen_get_monitor_plug_name;
  screen_class->get_monitor_geometry = gdk_mir_screen_get_monitor_geometry;
  screen_class->get_monitor_workarea = gdk_mir_screen_get_monitor_workarea;
  screen_class->list_visuals = gdk_mir_screen_list_visuals;
  screen_class->get_system_visual = gdk_mir_screen_get_system_visual;
  screen_class->get_rgba_visual = gdk_mir_screen_get_rgba_visual;
  screen_class->is_composited = gdk_mir_screen_is_composited;
  screen_class->make_display_name = gdk_mir_screen_make_display_name;
  screen_class->get_active_window = gdk_mir_screen_get_active_window;
  screen_class->get_window_stack = gdk_mir_screen_get_window_stack;
  screen_class->broadcast_client_message = gdk_mir_screen_broadcast_client_message;
  screen_class->get_setting = gdk_mir_screen_get_setting;
  screen_class->visual_get_best_depth = gdk_mir_screen_visual_get_best_depth;
  screen_class->visual_get_best_type = gdk_mir_screen_visual_get_best_type;
  screen_class->visual_get_best = gdk_mir_screen_visual_get_best;
  screen_class->visual_get_best_with_depth = gdk_mir_screen_visual_get_best_with_depth;
  screen_class->visual_get_best_with_type = gdk_mir_screen_visual_get_best_with_type;
  screen_class->visual_get_best_with_both = gdk_mir_screen_visual_get_best_with_both;
  screen_class->query_depths = gdk_mir_screen_query_depths;
  screen_class->query_visual_types = gdk_mir_screen_query_visual_types;
  screen_class->get_monitor_scale_factor = gdk_mir_screen_get_monitor_scale_factor;
}
