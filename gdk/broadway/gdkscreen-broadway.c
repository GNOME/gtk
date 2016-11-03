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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkscreen-broadway.h"

#include "gdkscreen.h"
#include "gdkdisplay.h"
#include "gdkdisplay-broadway.h"

#include <glib.h>

#include <stdlib.h>
#include <string.h>

static void   gdk_broadway_screen_dispose     (GObject *object);
static void   gdk_broadway_screen_finalize    (GObject *object);

G_DEFINE_TYPE (GdkBroadwayScreen, gdk_broadway_screen, GDK_TYPE_SCREEN)

static void
gdk_broadway_screen_init (GdkBroadwayScreen *screen)
{
}

static GdkDisplay *
gdk_broadway_screen_get_display (GdkScreen *screen)
{
  return GDK_BROADWAY_SCREEN (screen)->display;
}

static GdkWindow *
gdk_broadway_screen_get_root_window (GdkScreen *screen)
{
  return GDK_BROADWAY_SCREEN (screen)->root_window;
}

void
_gdk_broadway_screen_size_changed (GdkScreen                       *screen,
                                   BroadwayInputScreenResizeNotify *msg)
{
  GdkBroadwayScreen *broadway_screen = GDK_BROADWAY_SCREEN (screen);
  GdkMonitor *monitor;
  GdkRectangle size;
  GList *toplevels, *l;

  monitor = GDK_BROADWAY_DISPLAY (broadway_screen->display)->monitor;
  gdk_monitor_get_geometry (monitor, &size);

  if (msg->width == size.width &&
      msg->height == size.height)
    return;

  gdk_monitor_set_size (monitor, msg->width, msg->height);
  gdk_monitor_set_physical_size (monitor, msg->width * 25.4 / 96, msg->height * 25.4 / 96);

  toplevels = gdk_screen_get_toplevel_windows (screen);
  for (l = toplevels; l != NULL; l = l->next)
    {
      GdkWindow *toplevel = l->data;
      GdkWindowImplBroadway *toplevel_impl = GDK_WINDOW_IMPL_BROADWAY (toplevel->impl);

      if (toplevel_impl->maximized)
	gdk_window_move_resize (toplevel, 0, 0, msg->width, msg->height);
    }
}

static void
gdk_broadway_screen_dispose (GObject *object)
{
  GdkBroadwayScreen *broadway_screen = GDK_BROADWAY_SCREEN (object);

  if (broadway_screen->root_window)
    _gdk_window_destroy (broadway_screen->root_window, TRUE);

  G_OBJECT_CLASS (gdk_broadway_screen_parent_class)->dispose (object);
}

static void
gdk_broadway_screen_finalize (GObject *object)
{
  GdkBroadwayScreen *broadway_screen = GDK_BROADWAY_SCREEN (object);

  if (broadway_screen->root_window)
    g_object_unref (broadway_screen->root_window);

  G_OBJECT_CLASS (gdk_broadway_screen_parent_class)->finalize (object);
}

GdkScreen *
_gdk_broadway_screen_new (GdkDisplay *display,
			  gint	 screen_number)
{
  GdkScreen *screen;
  GdkBroadwayScreen *broadway_screen;

  screen = g_object_new (GDK_TYPE_BROADWAY_SCREEN, NULL);

  broadway_screen = GDK_BROADWAY_SCREEN (screen);
  broadway_screen->display = display;
  _gdk_broadway_screen_init_root_window (screen);

  return screen;
}

static gboolean
gdk_broadway_screen_get_setting (GdkScreen   *screen,
				 const gchar *name,
				 GValue      *value)
{
  return FALSE;
}

void
_gdk_broadway_screen_events_init (GdkScreen *screen)
{
}

static void
gdk_broadway_screen_class_init (GdkBroadwayScreenClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkScreenClass *screen_class = GDK_SCREEN_CLASS (klass);

  object_class->dispose = gdk_broadway_screen_dispose;
  object_class->finalize = gdk_broadway_screen_finalize;

  screen_class->get_display = gdk_broadway_screen_get_display;
  screen_class->get_root_window = gdk_broadway_screen_get_root_window;
  screen_class->get_setting = gdk_broadway_screen_get_setting;
}

