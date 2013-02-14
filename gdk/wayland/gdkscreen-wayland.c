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
#include "gdkscreenprivate.h"
#include "gdkvisualprivate.h"
#include "gdkdisplay.h"
#include "gdkdisplay-wayland.h"
#include "gdkwayland.h"
#include "gdkprivate-wayland.h"

typedef struct _GdkWaylandScreen      GdkWaylandScreen;
typedef struct _GdkWaylandScreenClass GdkWaylandScreenClass;

#define GDK_TYPE_WAYLAND_SCREEN              (_gdk_wayland_screen_get_type ())
#define GDK_WAYLAND_SCREEN(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WAYLAND_SCREEN, GdkWaylandScreen))
#define GDK_WAYLAND_SCREEN_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WAYLAND_SCREEN, GdkWaylandScreenClass))
#define GDK_IS_WAYLAND_SCREEN(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WAYLAND_SCREEN))
#define GDK_IS_WAYLAND_SCREEN_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WAYLAND_SCREEN))
#define GDK_WAYLAND_SCREEN_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WAYLAND_SCREEN, GdkWaylandScreenClass))

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
};

struct _GdkWaylandScreenClass
{
  GdkScreenClass parent_class;

  void (* window_manager_changed) (GdkWaylandScreen *screen_wayland);
};

struct _GdkWaylandMonitor
{
  struct wl_output *output;
  GdkRectangle  geometry;
  int		width_mm;
  int		height_mm;
  char *	output_name;
  char *	manufacturer;
};

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
    _gdk_window_destroy (screen_wayland->root_window, TRUE);

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

static gboolean
gdk_wayland_screen_get_setting (GdkScreen   *screen,
				const gchar *name,
				GValue      *value)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);

  if (strcmp ("gtk-theme-name", name) == 0)
    {
      const gchar *s = "Adwaita";
      GDK_NOTE(MISC, g_print("gdk_screen_get_setting(\"%s\") : %s\n", name, s));
      g_value_set_static_string (value, s);
      return TRUE;
    }
  if (strcmp ("gtk-cursor-theme-name", name) == 0)
    {
      const gchar *s = "Adwaita";
      GDK_NOTE(MISC, g_print("gdk_screen_get_setting(\"%s\") : %s\n", name, s));
      g_value_set_static_string (value, s);
      return TRUE;
    }
  else if (strcmp ("gtk-icon-theme-name", name) == 0)
    {
      const gchar *s = "gnome";
      GDK_NOTE(MISC, g_print("gdk_screen_get_setting(\"%s\") : %s\n", name, s));
      g_value_set_static_string (value, s);
      return TRUE;
    }
  else if (strcmp ("gtk-double-click-time", name) == 0)
    {
      gint i = 250;
      GDK_NOTE(MISC, g_print("gdk_screen_get_setting(\"%s\") : %d\n", name, i));
      g_value_set_int (value, i);
      return TRUE;
    }
  else if (strcmp ("gtk-double-click-distance", name) == 0)
    {
      gint i = 5;
      GDK_NOTE(MISC, g_print("gdk_screen_get_setting(\"%s\") : %d\n", name, i));
      g_value_set_int (value, i);
      return TRUE;
    }
  else if (strcmp ("gtk-dnd-drag-threshold", name) == 0)
    {
      gint i = 8;
      GDK_NOTE(MISC, g_print("gdk_screen_get_setting(\"%s\") : %d\n", name, i));
      g_value_set_int (value, i);
      return TRUE;
    }
  else if (strcmp ("gtk-split-cursor", name) == 0)
    {
      GDK_NOTE(MISC, g_print("gdk_screen_get_setting(\"%s\") : FALSE\n", name));
      g_value_set_boolean (value, FALSE);
      return TRUE;
    }
  else if (strcmp ("gtk-alternative-button-order", name) == 0)
    {
      GDK_NOTE(MISC, g_print("gdk_screen_get_setting(\"%s\") : TRUE\n", name));
      g_value_set_boolean (value, TRUE);
      return TRUE;
    }
  else if (strcmp ("gtk-alternative-sort-arrows", name) == 0)
    {
      GDK_NOTE(MISC, g_print("gdk_screen_get_setting(\"%s\") : TRUE\n", name));
      g_value_set_boolean (value, TRUE);
      return TRUE;
    }
  else if (strcmp ("gtk-xft-dpi", name) == 0)
    {
      gint i = 96*1024;
      GDK_NOTE(MISC, g_print("gdk_screen_get_setting(\"%s\") : TRUE\n", name));
      g_value_set_int (value, i);
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
  screen_wayland->width = 8192;
  screen_wayland->height = 8192;

  screen_wayland->visual = gdk_wayland_visual_new (screen);

  screen_wayland->root_window =
    _gdk_wayland_screen_create_root_window (screen,
					    screen_wayland->width,
					    screen_wayland->height);

  init_multihead (screen);

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
}

static const struct wl_output_listener output_listener =
{
  output_handle_geometry,
  output_handle_mode
};

void
_gdk_wayland_screen_add_output (GdkScreen *screen,
                                struct wl_output *output)
{
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (screen);
  GdkWaylandMonitor *monitor = g_new0(GdkWaylandMonitor, 1);

  monitor->output = output;
  g_ptr_array_add(screen_wayland->monitors, monitor);

  wl_output_add_listener(output, &output_listener, monitor);
}

void
_gdk_wayland_screen_remove_output_by_id (GdkScreen *screen,
                                         guint32    id)
{
  GdkWaylandScreen *screen_wayland = GDK_WAYLAND_SCREEN (screen);
  int i;

  for (i = 0; i < screen_wayland->monitors->len; i++)
    {
      GdkWaylandMonitor *monitor = screen_wayland->monitors->pdata[i];

      if (wl_proxy_get_id ((struct wl_proxy *)monitor->output) == id)
        {
          g_ptr_array_remove (screen_wayland->monitors, monitor);
          break;
        }
    }
}
