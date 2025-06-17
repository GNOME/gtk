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

#define GDK_PIXBUF_ENABLE_BACKEND

#include <string.h>

#include "gdkcursor-wayland.h"
#include "gdkcursorprivate.h"
#include "gdkdisplay-wayland.h"
#include "gdkshm-private.h"


typedef struct
{
  const char *name;
  GdkTexture *texture;
  int hotspot_x;
  int hotspot_y;
} CursorTexture;

static CursorTexture textures[] = {
  { "default",       NULL,  5,  5 },
  { "context-menu",  NULL,  5,  5 },
  { "help",          NULL, 16, 27 },
  { "pointer",       NULL, 14,  9 },
  { "progress",      NULL,  5,  4 },
  { "wait",          NULL, 11, 11 },
  { "cell",          NULL, 15, 15 },
  { "crosshair",     NULL, 15, 15 },
  { "text",          NULL, 14, 15 },
  { "vertical-text", NULL, 16, 15 },
  { "alias",         NULL, 12, 11 },
  { "copy",          NULL, 12, 11 },
  { "move",          NULL, 12, 11 },
  { "no-drop",       NULL, 12, 11 },
  { "not-allowed",   NULL, 12, 11 },
  { "grab",          NULL, 10,  6 },
  { "grabbing",      NULL, 15, 14 },
  { "e-resize",      NULL, 25, 17 },
  { "n-resize",      NULL, 17,  7 },
  { "ne-resize",     NULL, 20, 13 },
  { "nw-resize",     NULL, 13, 13 },
  { "s-resize",      NULL, 17, 23 },
  { "se-resize",     NULL, 19, 19 },
  { "sw-resize",     NULL, 13, 19 },
  { "w-resize",      NULL,  8, 17 },
  { "ew-resize",     NULL, 16, 15 },
  { "ns-resize",     NULL, 15, 17 },
  { "nesw-resize",   NULL, 14, 14 },
  { "nwse-resize",   NULL, 14, 14 },
  { "col-resize",    NULL, 16, 15 },
  { "row-resize",    NULL, 15, 17 },
  { "all-scroll",    NULL, 15, 15 },
  { "zoom-in",       NULL, 14, 13 },
  { "zoom-out",      NULL, 14, 13 },
  { "dnd-ask",       NULL,  5,  5 },
  { "all-resize",    NULL, 12, 11 },
};

static GdkTexture *
get_cursor_texture (const char *name,
                    int        *hotspot_x,
                    int        *hotspot_y)
{
  for (gsize i = 0; i < G_N_ELEMENTS (textures); i++)
    {
      if (strcmp (name, textures[i].name) == 0)
        {
          if (textures[i].texture == NULL)
            {
              char path[256];

              g_snprintf (path, sizeof (path), "/org/gtk/libgdk/cursor/%s", name);
              if (g_resources_get_info (path, 0, NULL, NULL, NULL))
                textures[i].texture = gdk_texture_new_from_resource (path);
            }

          *hotspot_x = textures[i].hotspot_x;
          *hotspot_y = textures[i].hotspot_y;

          return textures[i].texture;
        }
    }

  return NULL;
}

struct wl_buffer *
_gdk_wayland_cursor_get_buffer (GdkWaylandDisplay *display,
                                GdkCursor         *cursor,
                                double             desired_scale,
                                int               *hotspot_x,
                                int               *hotspot_y,
                                int               *width,
                                int               *height,
                                double            *scale)
{
  GdkTexture *texture;
  const char *name;
  GdkCursor *fallback;
  struct wl_buffer *buffer;

  *scale = 1;
  *hotspot_x = *hotspot_y = 0;
  *width = *height = 0;

  name = gdk_cursor_get_name (cursor);
  if (name)
    {
      if (g_str_equal (name, "none"))
        return NULL;

      texture = get_cursor_texture (name, hotspot_x, hotspot_y);
      if (texture)
        {
          g_object_ref (texture);
          *scale = (double) display->cursor_theme_size / gdk_texture_get_width (texture);
        }
    }
  else if (gdk_cursor_get_texture (cursor))
    {
      texture = g_object_ref (gdk_cursor_get_texture (cursor));

      *hotspot_x = gdk_cursor_get_hotspot_x (cursor);
      *hotspot_y = gdk_cursor_get_hotspot_y (cursor);
    }
  else
    {
      int w, h;

      texture = gdk_cursor_get_texture_for_size (cursor,
                                                 display->cursor_theme_size,
                                                 desired_scale,
                                                 &w,
                                                 &h,
                                                 hotspot_x,
                                                 hotspot_y);

      if (texture)
        {
          *scale = MAX (gdk_texture_get_width (texture) / (float) w,
                        gdk_texture_get_height (texture) / (float) h);
        }
    }

  if (texture)
    {
      *width = gdk_texture_get_width (texture);
      *height = gdk_texture_get_height (texture);

      buffer = _gdk_wayland_shm_texture_get_wl_buffer (display, texture);

      g_object_unref (texture);

      return buffer;
    }

  if (gdk_cursor_get_fallback (cursor))
    fallback = g_object_ref (gdk_cursor_get_fallback (cursor));
  else
    fallback = gdk_cursor_new_from_name ("default", NULL);

  buffer = _gdk_wayland_cursor_get_buffer (display,
                                           fallback,
                                           desired_scale,
                                           hotspot_x, hotspot_y,
                                           width, height,
                                           scale);

  g_object_unref (fallback);

  return buffer;
}
