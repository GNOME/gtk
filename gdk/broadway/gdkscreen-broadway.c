 /*
 * gdkscreen-broadway.c
 * 
 * Copyright 2001 Sun Microsystems Inc. 
 *
 * Erwann Chenede <erwann.chenede@sun.com>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "gdkscreen-broadway.h"

#include "gdkscreen.h"
#include "gdkdisplay.h"
#include "gdkdisplay-broadway.h"

#include <glib.h>

#include <stdlib.h>
#include <string.h>

static void         gdk_screen_broadway_dispose     (GObject		  *object);
static void         gdk_screen_broadway_finalize    (GObject		  *object);

G_DEFINE_TYPE (GdkScreenBroadway, _gdk_screen_broadway, GDK_TYPE_SCREEN)

static void
_gdk_screen_broadway_class_init (GdkScreenBroadwayClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gdk_screen_broadway_dispose;
  object_class->finalize = gdk_screen_broadway_finalize;
}

static void
_gdk_screen_broadway_init (GdkScreenBroadway *screen)
{
  screen->width = 1024;
  screen->height = 768;
}

GdkDisplay *
gdk_screen_get_display (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return GDK_SCREEN_BROADWAY (screen)->display;
}

gint
gdk_screen_get_width (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return GDK_SCREEN_BROADWAY (screen)->width;
}

gint
gdk_screen_get_height (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return GDK_SCREEN_BROADWAY (screen)->height;
}

gint
gdk_screen_get_width_mm (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return gdk_screen_get_width (screen) * 25.4 / 96;
}

gint
gdk_screen_get_height_mm (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return gdk_screen_get_height (screen) * 25.4 / 96;
}

gint
gdk_screen_get_number (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return 0;
}

GdkWindow *
gdk_screen_get_root_window (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return GDK_SCREEN_BROADWAY (screen)->root_window;
}

static void
gdk_screen_broadway_dispose (GObject *object)
{
  GdkScreenBroadway *screen_broadway = GDK_SCREEN_BROADWAY (object);

  if (screen_broadway->root_window)
    _gdk_window_destroy (screen_broadway->root_window, TRUE);

  G_OBJECT_CLASS (_gdk_screen_broadway_parent_class)->dispose (object);
}

static void
gdk_screen_broadway_finalize (GObject *object)
{
  GdkScreenBroadway *screen_broadway = GDK_SCREEN_BROADWAY (object);
  gint          i;

  if (screen_broadway->root_window)
    g_object_unref (screen_broadway->root_window);

  /* Visual Part */
  for (i = 0; i < screen_broadway->nvisuals; i++)
    g_object_unref (screen_broadway->visuals[i]);
  g_free (screen_broadway->visuals);

  G_OBJECT_CLASS (_gdk_screen_broadway_parent_class)->finalize (object);
}

gint
gdk_screen_get_n_monitors (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return 1;
}

gint
gdk_screen_get_primary_monitor (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), 0);

  return 0;
}

gint
gdk_screen_get_monitor_width_mm	(GdkScreen *screen,
				 gint       monitor_num)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), -1);
  g_return_val_if_fail (monitor_num == 0, -1);

  return gdk_screen_get_width_mm (screen);
}

gint
gdk_screen_get_monitor_height_mm (GdkScreen *screen,
				  gint       monitor_num)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), -1);
  g_return_val_if_fail (monitor_num == 0, -1);

  return gdk_screen_get_height_mm (screen);
}

gchar *
gdk_screen_get_monitor_plug_name (GdkScreen *screen,
				  gint       monitor_num)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
  g_return_val_if_fail (monitor_num == 0, NULL);

  return g_strdup ("browser");
}

/**
 * gdk_screen_get_monitor_geometry:
 * @screen: a #GdkScreen
 * @monitor_num: the monitor number, between 0 and gdk_screen_get_n_monitors (screen)
 * @dest: a #GdkRectangle to be filled with the monitor geometry
 *
 * Retrieves the #GdkRectangle representing the size and position of
 * the individual monitor within the entire screen area.
 *
 * Note that the size of the entire screen area can be retrieved via
 * gdk_screen_get_width() and gdk_screen_get_height().
 *
 * Since: 2.2
 */
void
gdk_screen_get_monitor_geometry (GdkScreen    *screen,
				 gint          monitor_num,
				 GdkRectangle *dest)
{
  GdkScreenBroadway *screen_broadway = GDK_SCREEN_BROADWAY (screen);

  g_return_if_fail (GDK_IS_SCREEN (screen));
  g_return_if_fail (monitor_num == 0);

  if (dest)
    {
      dest->x = 0;
      dest->y = 0;
      dest->width = screen_broadway->width;
      dest->height = screen_broadway->height;
    }
}

GdkVisual *
gdk_screen_get_rgba_visual (GdkScreen *screen)
{
  GdkScreenBroadway *screen_broadway;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  screen_broadway = GDK_SCREEN_BROADWAY (screen);

  return screen_broadway->rgba_visual;
}

GdkScreen *
_gdk_broadway_screen_new (GdkDisplay *display,
		     gint	 screen_number)
{
  GdkScreen *screen;
  GdkScreenBroadway *screen_broadway;

  screen = g_object_new (GDK_TYPE_SCREEN_BROADWAY, NULL);

  screen_broadway = GDK_SCREEN_BROADWAY (screen);
  screen_broadway->display = display;
  _gdk_visual_init (screen);
  _gdk_windowing_window_init (screen);

  return screen;
}

/*
 * It is important that we first request the selection
 * notification, and then setup the initial state of
 * is_composited to avoid a race condition here.
 */
void
_gdk_broadway_screen_setup (GdkScreen *screen)
{
}

gboolean
gdk_screen_is_composited (GdkScreen *screen)
{
  GdkScreenBroadway *screen_broadway;

  g_return_val_if_fail (GDK_IS_SCREEN (screen), FALSE);

  screen_broadway = GDK_SCREEN_BROADWAY (screen);

  return FALSE;
}


gchar *
gdk_screen_make_display_name (GdkScreen *screen)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return g_strdup ("browser");
}

GdkWindow *
gdk_screen_get_active_window (GdkScreen *screen)
{
  return NULL;
}

GList *
gdk_screen_get_window_stack (GdkScreen *screen)
{
  return NULL;
}

void
gdk_screen_broadcast_client_message (GdkScreen *screen,
				     GdkEvent  *event)
{
}

gboolean
gdk_screen_get_setting (GdkScreen   *screen,
			const gchar *name,
			GValue      *value)
{
  return FALSE;
}

gboolean
gdk_net_wm_supports (GdkAtom property)
{
  return FALSE;
}

void
_gdk_screen_broadway_events_init (GdkScreen *screen)
{
}

gchar *
_gdk_windowing_substitute_screen_number (const gchar *display_name,
					 gint         screen_number)
{
  return g_strdup ("browser");
}

