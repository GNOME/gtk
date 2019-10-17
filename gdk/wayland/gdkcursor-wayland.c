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

#include "gdkprivate-wayland.h"
#include "gdkcursorprivate.h"
#include "gdkdisplay-wayland.h"
#include "gdkwayland.h"
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <wayland-cursor.h>

static void
gdk_wayland_cursor_remove_from_cache (gpointer data, GObject *cursor)
{
  GdkWaylandDisplay *display = data;

  g_hash_table_remove (display->cursor_surface_cache, cursor);
}

void
_gdk_wayland_display_init_cursors (GdkWaylandDisplay *display)
{
  display->cursor_surface_cache = g_hash_table_new_full (gdk_cursor_hash, gdk_cursor_equal, NULL, (GDestroyNotify) cairo_surface_destroy);
}

void
_gdk_wayland_display_finalize_cursors (GdkWaylandDisplay *display)
{
  GHashTableIter iter;
  gpointer cursor;

  g_hash_table_iter_init (&iter, display->cursor_surface_cache);
  while (g_hash_table_iter_next (&iter, &cursor, NULL))
    g_object_weak_unref (G_OBJECT (cursor), gdk_wayland_cursor_remove_from_cache, display);
  g_hash_table_destroy (display->cursor_surface_cache);
}

static const struct {
  const gchar *css_name, *traditional_name;
} name_map[] = {
  { "default",      "left_ptr" },
  { "help",         "question_arrow" },
  { "context-menu", "left_ptr" },
  { "pointer",      "hand" },
  { "progress",     "left_ptr_watch" },
  { "wait",         "watch" },
  { "cell",         "crosshair" },
  { "crosshair",    "cross" },
  { "text",         "xterm" },
  { "vertical-text","xterm" },
  { "alias",        "dnd-link" },
  { "copy",         "dnd-copy" },
  { "move",         "dnd-move" },
  { "no-drop",      "dnd-none" },
  { "dnd-ask",      "dnd-copy" }, /* not CSS, but we want to guarantee it anyway */
  { "not-allowed",  "crossed_circle" },
  { "grab",         "hand2" },
  { "grabbing",     "hand2" },
  { "all-scroll",   "left_ptr" },
  { "col-resize",   "h_double_arrow" },
  { "row-resize",   "v_double_arrow" },
  { "n-resize",     "top_side" },
  { "e-resize",     "right_side" },
  { "s-resize",     "bottom_side" },
  { "w-resize",     "left_side" },
  { "ne-resize",    "top_right_corner" },
  { "nw-resize",    "top_left_corner" },
  { "se-resize",    "bottom_right_corner" },
  { "sw-resize",    "bottom_left_corner" },
  { "ew-resize",    "h_double_arrow" },
  { "ns-resize",    "v_double_arrow" },
  { "nesw-resize",  "fd_double_arrow" },
  { "nwse-resize",  "bd_double_arrow" },
  { "zoom-in",      "left_ptr" },
  { "zoom-out",     "left_ptr" }
};

static const gchar *
name_fallback (const gchar *name)
{
  gint i;

  for (i = 0; i < G_N_ELEMENTS (name_map); i++)
    {
      if (g_str_equal (name_map[i].css_name, name))
        return name_map[i].traditional_name;
    }

  return NULL;
}

static struct wl_cursor *
gdk_wayland_cursor_load_for_name (GdkWaylandDisplay      *display_wayland,
                                  struct wl_cursor_theme *theme,
                                  const char             *name)
{
  struct wl_cursor *c;

  c = wl_cursor_theme_get_cursor (theme, name);
  if (!c)
    {
      const char *fallback;

      fallback = name_fallback (name);
      if (fallback)
        {
          c = wl_cursor_theme_get_cursor (theme, fallback);
        }
    }

  return c;
}

static void
buffer_release_callback (void             *_data,
                         struct wl_buffer *wl_buffer)
{
  cairo_surface_t *cairo_surface = _data;

  cairo_surface_destroy (cairo_surface);
}

static const struct wl_buffer_listener buffer_listener = {
  buffer_release_callback
};

struct wl_buffer *
_gdk_wayland_cursor_get_buffer (GdkWaylandDisplay *display,
                                GdkCursor         *cursor,
                                guint              desired_scale,
                                guint              image_index,
                                int               *hotspot_x,
                                int               *hotspot_y,
                                int               *width,
                                int               *height,
                                int               *scale)
{
  desired_scale = MIN (desired_scale, GDK_WAYLAND_MAX_THEME_SCALE);

  if (gdk_cursor_get_name (cursor))
    {
      struct wl_cursor *c;

      if (g_str_equal (gdk_cursor_get_name (cursor), "none"))
        goto none;

      c = gdk_wayland_cursor_load_for_name (display,
                                            _gdk_wayland_display_get_scaled_cursor_theme (display, desired_scale),
                                            gdk_cursor_get_name (cursor));
      if (c)
        {
          struct wl_cursor_image *image;

          if (image_index >= c->image_count)
            {
              g_warning (G_STRLOC " out of bounds cursor image [%d / %d]",
                         image_index,
                         c->image_count - 1);
              image_index = 0;
            }

          image = c->images[image_index];

          *hotspot_x = image->hotspot_x / desired_scale;
          *hotspot_y = image->hotspot_y / desired_scale;

          *width = image->width / desired_scale;
          *height = image->height / desired_scale;
          *scale = desired_scale;

          return wl_cursor_image_get_buffer (image);
        }
    }
  else
    {
      GdkTexture *texture = gdk_cursor_get_texture (cursor);
      cairo_surface_t *surface;
      struct wl_buffer *buffer;

      surface = g_hash_table_lookup (display->cursor_surface_cache, cursor);
      if (surface == NULL)
        {
          surface = _gdk_wayland_display_create_shm_surface (display,
                                                             gdk_texture_get_width (texture),
                                                             gdk_texture_get_height (texture),
                                                             1);
          
          gdk_texture_download (texture,
                                cairo_image_surface_get_data (surface),
                                cairo_image_surface_get_stride (surface));
          cairo_surface_mark_dirty (surface);

          g_object_weak_ref (G_OBJECT (cursor), gdk_wayland_cursor_remove_from_cache, display);
          g_hash_table_insert (display->cursor_surface_cache, cursor, surface);
        }

      *hotspot_x = gdk_cursor_get_hotspot_x (cursor);
      *hotspot_y = gdk_cursor_get_hotspot_y (cursor);
      *width = gdk_texture_get_width (texture);
      *height = gdk_texture_get_height (texture);
      *scale = 1;

      cairo_surface_reference (surface);
      buffer = _gdk_wayland_shm_surface_get_wl_buffer (surface);
      wl_buffer_add_listener (buffer, &buffer_listener, surface);

      return buffer;
    }

  if (gdk_cursor_get_fallback (cursor))
    return _gdk_wayland_cursor_get_buffer (display,
                                           gdk_cursor_get_fallback (cursor), 
                                           desired_scale,
                                           image_index,
                                           hotspot_x, hotspot_y,
                                           width, height,
                                           scale);

none:
  *hotspot_x = 0;
  *hotspot_y = 0;
  *width = 0;
  *height = 0;
  *scale = 1;

  return NULL;
}

guint
_gdk_wayland_cursor_get_next_image_index (GdkWaylandDisplay *display,
                                          GdkCursor         *cursor,
                                          guint              scale,
                                          guint              current_image_index,
                                          guint             *next_image_delay)
{
  struct wl_cursor *c;

  if (!gdk_cursor_get_name (cursor) ||
      g_str_equal (gdk_cursor_get_name (cursor), "none"))
    {
      *next_image_delay = 0;
      return current_image_index;
    }

  c = gdk_wayland_cursor_load_for_name (display,
                                        _gdk_wayland_display_get_scaled_cursor_theme (display, scale),
                                        gdk_cursor_get_name (cursor));

  if (c)
    {
      if (current_image_index >= c->image_count)
        {
          g_warning (G_STRLOC " out of bounds cursor image [%d / %d]",
                     current_image_index, c->image_count - 1);
          current_image_index = 0;
        }

      if (c->image_count == 1)
        {
          *next_image_delay = 0;
          return current_image_index;
        }
      else
        {
          *next_image_delay = c->images[current_image_index]->delay;
          return (current_image_index + 1) % c->image_count;
        }
    }

  if (gdk_cursor_get_fallback (cursor))
    return _gdk_wayland_cursor_get_next_image_index (display,
                                                     gdk_cursor_get_fallback (cursor),
                                                     scale,
                                                     current_image_index,
                                                     next_image_delay);

  *next_image_delay = 0;
  return current_image_index;
}
