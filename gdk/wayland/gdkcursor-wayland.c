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

#define GDK_TYPE_WAYLAND_CURSOR              (_gdk_wayland_cursor_get_type ())
#define GDK_WAYLAND_CURSOR(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WAYLAND_CURSOR, GdkWaylandCursor))
#define GDK_WAYLAND_CURSOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WAYLAND_CURSOR, GdkWaylandCursorClass))
#define GDK_IS_WAYLAND_CURSOR(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WAYLAND_CURSOR))
#define GDK_IS_WAYLAND_CURSOR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WAYLAND_CURSOR))
#define GDK_WAYLAND_CURSOR_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WAYLAND_CURSOR, GdkWaylandCursorClass))

typedef struct _GdkWaylandCursor GdkWaylandCursor;
typedef struct _GdkWaylandCursorClass GdkWaylandCursorClass;

struct _GdkWaylandCursor
{
  GdkCursor cursor;
  gchar *name;
  guint serial;

  struct
  {
    int hotspot_x, hotspot_y;
    int width, height, scale;
    struct wl_buffer *buffer;
  } surface;

  struct wl_cursor *wl_cursor;
};

struct _GdkWaylandCursorClass
{
  GdkCursorClass cursor_class;
};

G_DEFINE_TYPE (GdkWaylandCursor, _gdk_wayland_cursor, GDK_TYPE_CURSOR)

static guint theme_serial = 0;

struct cursor_cache_key
{
  GdkCursorType type;
  const char *name;
};

static void
add_to_cache (GdkWaylandDisplay *display, GdkWaylandCursor *cursor)
{
  display->cursor_cache = g_slist_prepend (display->cursor_cache, cursor);

  g_object_ref (cursor);
}

static gint
cache_compare_func (gconstpointer listelem,
                    gconstpointer target)
{
  GdkWaylandCursor *cursor = (GdkWaylandCursor *) listelem;
  struct cursor_cache_key* key = (struct cursor_cache_key *) target;

  if (cursor->cursor.type != key->type)
    return 1; /* No match */

  /* Elements marked as pixmap must be named cursors
   * (since we don't store normal pixmap cursors
   */
  if (key->type == GDK_CURSOR_IS_PIXMAP)
    return strcmp (key->name, cursor->name);

  return 0; /* Match */
}

static GdkWaylandCursor*
find_in_cache (GdkWaylandDisplay *display,
               GdkCursorType      type,
               const char        *name)
{
  GSList* res;
  struct cursor_cache_key key;

  key.type = type;
  key.name = name;

  res = g_slist_find_custom (display->cursor_cache, &key, cache_compare_func);

  if (res)
    return (GdkWaylandCursor *) res->data;

  return NULL;
}

/* Called by gdk_wayland_display_finalize to flush any cached cursors
 * for a dead display.
 */
void
_gdk_wayland_display_finalize_cursors (GdkWaylandDisplay *display)
{
  g_slist_foreach (display->cursor_cache, (GFunc) g_object_unref, NULL);
  g_slist_free (display->cursor_cache);
}

static gboolean
set_cursor_from_theme (GdkWaylandCursor *cursor, struct wl_cursor_theme *theme)
{
  struct wl_cursor *c;

  c = wl_cursor_theme_get_cursor (theme, cursor->name);
  if (!c)
    {
      g_warning (G_STRLOC ": Unable to load %s from the cursor theme", cursor->name);

      /* return the left_ptr cursor as a fallback */
      c = wl_cursor_theme_get_cursor (theme, "left_ptr");

      if (!c)
        return FALSE;
    }

  cursor->wl_cursor = c;

  return TRUE;
}

void
_gdk_wayland_display_update_cursors (GdkWaylandDisplay      *display,
                                     struct wl_cursor_theme *theme)
{
  g_slist_foreach (display->cursor_cache, (GFunc) set_cursor_from_theme, theme);
}

static void
gdk_wayland_cursor_finalize (GObject *object)
{
  GdkWaylandCursor *cursor = GDK_WAYLAND_CURSOR (object);

  g_free (cursor->name);
  if (cursor->surface.buffer)
    wl_buffer_destroy (cursor->surface.buffer);

  G_OBJECT_CLASS (_gdk_wayland_cursor_parent_class)->finalize (object);
}

static cairo_surface_t *
gdk_wayland_cursor_get_surface (GdkCursor *cursor,
				gdouble *x_hot,
				gdouble *y_hot)
{
  return NULL;
}

struct wl_buffer *
_gdk_wayland_cursor_get_buffer (GdkCursor *cursor,
                                guint      image_index,
                                int       *hotspot_x,
                                int       *hotspot_y,
                                int       *w,
                                int       *h,
				int       *scale)
{
  GdkWaylandCursor *wayland_cursor = GDK_WAYLAND_CURSOR (cursor);

  if (wayland_cursor->wl_cursor)
    {
      struct wl_cursor_image *image;

      if (image_index >= wayland_cursor->wl_cursor->image_count)
        {
          g_warning (G_STRLOC " out of bounds cursor image [%d / %d]",
                     image_index,
                     wayland_cursor->wl_cursor->image_count - 1);
          image_index = 0;
        }

      image = wayland_cursor->wl_cursor->images[image_index];

      *hotspot_x = image->hotspot_x;
      *hotspot_y = image->hotspot_y;

      *w = image->width;
      *h = image->height;
      *scale = 1;

      return wl_cursor_image_get_buffer (image);
    }
  else /* From surface */
    {
      *hotspot_x = wayland_cursor->surface.hotspot_x;
      *hotspot_y = wayland_cursor->surface.hotspot_y;

      *w = wayland_cursor->surface.width / wayland_cursor->surface.scale;
      *h = wayland_cursor->surface.height / wayland_cursor->surface.scale;
      *scale = wayland_cursor->surface.scale;

      return wayland_cursor->surface.buffer;
    }
}

guint
_gdk_wayland_cursor_get_next_image_index (GdkCursor *cursor,
                                          guint      current_image_index,
                                          guint     *next_image_delay)
{
  struct wl_cursor *wl_cursor = GDK_WAYLAND_CURSOR (cursor)->wl_cursor;

  if (wl_cursor && wl_cursor->image_count > 1)
    {
      if (current_image_index >= wl_cursor->image_count)
        {
          g_warning (G_STRLOC " out of bounds cursor image [%d / %d]",
                     current_image_index, wl_cursor->image_count - 1);
          current_image_index = 0;
        }

      /* Return the time to next image */
      if (next_image_delay)
        *next_image_delay = wl_cursor->images[current_image_index]->delay;

      return (current_image_index + 1) % wl_cursor->image_count;
    }
  else
    return current_image_index;
}

static void
_gdk_wayland_cursor_class_init (GdkWaylandCursorClass *wayland_cursor_class)
{
  GdkCursorClass *cursor_class = GDK_CURSOR_CLASS (wayland_cursor_class);
  GObjectClass *object_class = G_OBJECT_CLASS (wayland_cursor_class);

  object_class->finalize = gdk_wayland_cursor_finalize;

  cursor_class->get_surface = gdk_wayland_cursor_get_surface;
}

static void
_gdk_wayland_cursor_init (GdkWaylandCursor *cursor)
{
}

GdkCursor *
_gdk_wayland_display_get_cursor_for_type (GdkDisplay    *display,
					  GdkCursorType  cursor_type)
{
  GEnumClass *enum_class;
  GEnumValue *enum_value;
  gchar *cursor_name;
  GdkCursor *result;

  enum_class = g_type_class_ref (GDK_TYPE_CURSOR_TYPE);
  enum_value = g_enum_get_value (enum_class, cursor_type);
  cursor_name = g_strdup (enum_value->value_nick);
  g_strdelimit (cursor_name, "-", '_');
  g_type_class_unref (enum_class);

  result = _gdk_wayland_display_get_cursor_for_name (display, cursor_name);

  g_free (cursor_name);

  return result;
}

GdkCursor *
_gdk_wayland_display_get_cursor_for_name (GdkDisplay  *display,
					  const gchar *name)
{
  GdkWaylandCursor *private;
  GdkWaylandDisplay *wayland_display = GDK_WAYLAND_DISPLAY (display);

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  private = find_in_cache (wayland_display, GDK_CURSOR_IS_PIXMAP, name);
  if (private)
    {
      /* Cache had it, add a ref for this user */
      g_object_ref (private);

      return (GdkCursor*) private;
    }

  private = g_object_new (GDK_TYPE_WAYLAND_CURSOR,
                          "cursor-type", GDK_CURSOR_IS_PIXMAP,
                          "display", display,
                          NULL);
  private->name = g_strdup (name);
  private->serial = theme_serial;
  private->surface.scale = 1;

  /* Blank cursor case */
  if (!name || g_str_equal (name, "blank_cursor"))
    return GDK_CURSOR (private);

  if (!set_cursor_from_theme (private, wayland_display->cursor_theme))
    return GDK_CURSOR (private);

  add_to_cache (wayland_display, private);

  return GDK_CURSOR (private);
}

GdkCursor *
_gdk_wayland_display_get_cursor_for_surface (GdkDisplay *display,
					     cairo_surface_t *surface,
					     gdouble     x,
					     gdouble     y)
{
  GdkWaylandCursor *cursor;
  GdkWaylandDisplay *wayland_display = GDK_WAYLAND_DISPLAY (display);
  int stride;
  size_t size;
  gpointer data;
  struct wl_shm_pool *pool;
  cairo_surface_t *buffer_surface;
  cairo_t *cr;

  cursor = g_object_new (GDK_TYPE_WAYLAND_CURSOR,
			 "cursor-type", GDK_CURSOR_IS_PIXMAP,
			 "display", wayland_display,
			 NULL);
  cursor->name = NULL;
  cursor->serial = theme_serial;
  cursor->surface.hotspot_x = x;
  cursor->surface.hotspot_y = y;

  cursor->surface.scale = 1;

  if (surface)
    {
#ifdef HAVE_CAIRO_SURFACE_SET_DEVICE_SCALE
	{
	  double sx, sy;
	  cairo_surface_get_device_scale (surface, &sx, &sy);
	  cursor->surface.scale = (int)sx;
	}
#endif
      cursor->surface.width = cairo_image_surface_get_width (surface);
      cursor->surface.height = cairo_image_surface_get_height (surface);
    }
  else
    {
      cursor->surface.width = 1;
      cursor->surface.height = 1;
    }

  pool = _create_shm_pool (wayland_display->shm,
                           cursor->surface.width,
                           cursor->surface.height,
                           &size,
                           &data);

  if (surface)
    {
      buffer_surface = cairo_image_surface_create_for_data (data,
							    CAIRO_FORMAT_ARGB32,
							    cursor->surface.width,
							    cursor->surface.height,
							    cursor->surface.width * 4);
      cr = cairo_create (buffer_surface);
      cairo_set_source_surface (cr, surface, 0, 0);
      cairo_paint (cr);
      cairo_destroy (cr);
      cairo_surface_destroy (buffer_surface);
    }
  else
    memset (data, 0, 4);

  stride = cursor->surface.width * 4;
  cursor->surface.buffer = wl_shm_pool_create_buffer (pool, 0,
                                                     cursor->surface.width,
                                                     cursor->surface.height,
                                                     stride,
                                                     WL_SHM_FORMAT_ARGB8888);

  wl_shm_pool_destroy (pool);

  return GDK_CURSOR (cursor);
}

void
_gdk_wayland_display_get_default_cursor_size (GdkDisplay *display,
					      guint       *width,
					      guint       *height)
{
  *width = 32;
  *height = 32;
}

void
_gdk_wayland_display_get_maximal_cursor_size (GdkDisplay *display,
					      guint       *width,
					      guint       *height)
{
  *width = 256;
  *height = 256;
}

gboolean
_gdk_wayland_display_supports_cursor_alpha (GdkDisplay *display)
{
  return TRUE;
}

gboolean
_gdk_wayland_display_supports_cursor_color (GdkDisplay *display)
{
  return TRUE;
}
