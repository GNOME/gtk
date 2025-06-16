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

  struct
  {
    int hotspot_x, hotspot_y;
    int width, height, scale;
    cairo_surface_t *cairo_surface;
  } surface;

  struct wl_cursor *wl_cursor;
  int scale;
};

struct _GdkWaylandCursorClass
{
  GdkCursorClass cursor_class;
};

GType _gdk_wayland_cursor_get_type (void);

G_DEFINE_TYPE (GdkWaylandCursor, _gdk_wayland_cursor, GDK_TYPE_CURSOR)

void
_gdk_wayland_display_init_cursors (GdkWaylandDisplay *display)
{
  display->cursor_cache = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);
}

void
_gdk_wayland_display_finalize_cursors (GdkWaylandDisplay *display)
{
  g_hash_table_destroy (display->cursor_cache);
}

static const struct {
  const gchar *css_name, *traditional_name;
} name_map[] = {
  { "default",      "left_ptr" },
  { "help",         "left_ptr" },
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
  { "dnd-move",     "default" },
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
  { "zoom-out",     "left_ptr" },
  { "all-resize",   "move" }, /* not CSS, but we want to guarantee it anyway */
  { NULL, NULL }
};

static const gchar *
name_fallback (const gchar *name)
{
  gint i;

  for (i = 0; name_map[i].css_name; i++)
    {
      if (g_str_equal (name_map[i].css_name, name))
        return name_map[i].traditional_name;
    }

  return NULL;
}

static gboolean
_gdk_wayland_cursor_update (GdkWaylandDisplay *display_wayland,
                            GdkWaylandCursor  *cursor)
{
  struct wl_cursor *c;
  struct wl_cursor_theme *theme;

  /* Do nothing if this is not a wl_cursor cursor. */
  if (cursor->name == NULL)
    return FALSE;

  theme = _gdk_wayland_display_get_scaled_cursor_theme (display_wayland,
                                                        cursor->scale);
  c = wl_cursor_theme_get_cursor (theme, cursor->name);
  if (!c)
    {
      const char *fallback;

      fallback = name_fallback (cursor->name);
      if (fallback)
        {
          c = wl_cursor_theme_get_cursor (theme, name_fallback (cursor->name));
          if (!c)
            c = wl_cursor_theme_get_cursor (theme, "default");
        }
    }

  if (!c)
    {
      g_message ("Unable to load %s from the cursor theme", cursor->name);
      return FALSE;
    }

  cursor->wl_cursor = c;

  return TRUE;
}

void
_gdk_wayland_display_update_cursors (GdkWaylandDisplay *display)
{
  GHashTableIter iter;
  const char *name;
  GdkWaylandCursor *cursor;

  g_hash_table_iter_init (&iter, display->cursor_cache);

  while (g_hash_table_iter_next (&iter, (gpointer *) &name, (gpointer *) &cursor))
    _gdk_wayland_cursor_update (display, cursor);
}

static void
gdk_wayland_cursor_finalize (GObject *object)
{
  GdkWaylandCursor *cursor = GDK_WAYLAND_CURSOR (object);

  g_free (cursor->name);
  if (cursor->surface.cairo_surface)
    cairo_surface_destroy (cursor->surface.cairo_surface);

  G_OBJECT_CLASS (_gdk_wayland_cursor_parent_class)->finalize (object);
}

static cairo_surface_t *
gdk_wayland_cursor_get_surface (GdkCursor *cursor,
				gdouble *x_hot,
				gdouble *y_hot)
{
  return NULL;
}

const char *
_gdk_wayland_cursor_get_name (GdkCursor *cursor)
{
  GdkWaylandCursor *wayland_cursor = GDK_WAYLAND_CURSOR (cursor);
  return wayland_cursor->name;
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

  if (wayland_cursor->wl_cursor && wayland_cursor->wl_cursor->image_count > 0)
    {
      struct wl_cursor_image *image;
      int cursor_scale;

      if (image_index >= wayland_cursor->wl_cursor->image_count)
        {
          g_warning (G_STRLOC " out of bounds cursor image [%d / %d]",
                     image_index,
                     wayland_cursor->wl_cursor->image_count - 1);
          image_index = 0;
        }

      image = wayland_cursor->wl_cursor->images[image_index];

      cursor_scale = wayland_cursor->scale;
      while ((image->width % cursor_scale != 0) ||
             (image->height % cursor_scale != 0))
        {
          g_warning_once (G_STRLOC " cursor image size (%dx%d) not an integer"
                     "multiple of scale (%d)", image->width, image->height,
                     cursor_scale);
          cursor_scale--;
        }

      *hotspot_x = image->hotspot_x / cursor_scale;
      *hotspot_y = image->hotspot_y / cursor_scale;

      *w = image->width / cursor_scale;
      *h = image->height / cursor_scale;
      *scale = cursor_scale;

      return wl_cursor_image_get_buffer (image);
    }
  else if (wayland_cursor->name == NULL) /* From surface */
    {
      *hotspot_x =
        wayland_cursor->surface.hotspot_x / wayland_cursor->surface.scale;
      *hotspot_y =
        wayland_cursor->surface.hotspot_y / wayland_cursor->surface.scale;

      *w = wayland_cursor->surface.width / wayland_cursor->surface.scale;
      *h = wayland_cursor->surface.height / wayland_cursor->surface.scale;
      *scale = wayland_cursor->surface.scale;

      cairo_surface_reference (wayland_cursor->surface.cairo_surface);

      if (wayland_cursor->surface.cairo_surface)
        return _gdk_wayland_shm_surface_get_wl_buffer (wayland_cursor->surface.cairo_surface);
    }
  else
    {
      *hotspot_x = 0;
      *hotspot_y = 0;
      *w = 0;
      *h = 0;
      *scale = 1;
    }

  return NULL;
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

void
_gdk_wayland_cursor_set_scale (GdkCursor *cursor,
                               guint      scale)
{
  GdkWaylandDisplay *display_wayland =
    GDK_WAYLAND_DISPLAY (gdk_cursor_get_display (cursor));
  GdkWaylandCursor *wayland_cursor = GDK_WAYLAND_CURSOR (cursor);

  if (scale > GDK_WAYLAND_MAX_THEME_SCALE)
    {
      g_warning (G_STRLOC ": cursor theme size %u too large", scale);
      scale = GDK_WAYLAND_MAX_THEME_SCALE;
    }

  if (wayland_cursor->scale == scale)
    return;

  wayland_cursor->scale = scale;

  /* Blank cursor case */
  if (g_strcmp0 (wayland_cursor->name, "none") == 0)
    return;

  _gdk_wayland_cursor_update (display_wayland, wayland_cursor);
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
_gdk_wayland_display_get_cursor_for_name_with_scale (GdkDisplay  *display,
                                                     const gchar *name,
                                                     guint        scale)
{
  GdkWaylandCursor *wayland_cursor;
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  wayland_cursor = g_hash_table_lookup (display_wayland->cursor_cache, name);
  if (wayland_cursor && wayland_cursor->scale == scale)
    return GDK_CURSOR (g_object_ref (wayland_cursor));

  wayland_cursor = g_object_new (GDK_TYPE_WAYLAND_CURSOR,
                                 "cursor-type", GDK_CURSOR_IS_PIXMAP,
                                 "display", display,
                                 NULL);

  /* Blank cursor case */
  if (!name || g_str_equal (name, "none") || g_str_equal (name, "blank_cursor"))
    {
      wayland_cursor->name = g_strdup ("none");
      wayland_cursor->scale = scale;

      return GDK_CURSOR (wayland_cursor);
    }

  wayland_cursor->name = g_strdup (name);
  wayland_cursor->scale = scale;

  if (!_gdk_wayland_cursor_update (display_wayland, wayland_cursor))
    {
      g_object_unref (wayland_cursor);
      return NULL;
    }

  /* Insert into cache. */
  g_hash_table_replace (display_wayland->cursor_cache,
                        wayland_cursor->name,
                        g_object_ref (wayland_cursor));
  return GDK_CURSOR (wayland_cursor);
}

GdkCursor *
_gdk_wayland_display_get_cursor_for_name (GdkDisplay  *display,
                                          const gchar *name)
{
  return _gdk_wayland_display_get_cursor_for_name_with_scale (display, name, 1);
}

GdkCursor *
_gdk_wayland_display_get_cursor_for_type_with_scale (GdkDisplay    *display,
                                                     GdkCursorType  cursor_type,
                                                     guint          scale)
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

  result = _gdk_wayland_display_get_cursor_for_name_with_scale (display,
                                                                cursor_name,
                                                                scale);

  g_free (cursor_name);

  if (!result)
    {
      const char *name = NULL;

      /* Map cursors back to standard names.
       * Currently, we just list the cursor values
       * that are used in GTK. More can be added.
       */
      switch ((int)cursor_type)
        {
        case GDK_XTERM:
          name = "text";
          break;
        case GDK_FLEUR:
          name = "move";
          break;
        case GDK_CROSSHAIR:
          name = "cross";
          break;
        default:
          name = "default";
          break;
        }

      if (name)
        result = _gdk_wayland_display_get_cursor_for_name_with_scale (display,
                                                                      name,
                                                                      scale);
    }

  return result;
}

GdkCursor *
_gdk_wayland_display_get_cursor_for_type (GdkDisplay    *display,
                                          GdkCursorType  cursor_type)
{
  return _gdk_wayland_display_get_cursor_for_type_with_scale (display,
                                                              cursor_type,
                                                              1);
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

GdkCursor *
_gdk_wayland_display_get_cursor_for_surface (GdkDisplay *display,
					     cairo_surface_t *surface,
					     gdouble     x,
					     gdouble     y)
{
  GdkWaylandCursor *cursor;
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  struct wl_buffer *buffer;
  cairo_t *cr;

  cursor = g_object_new (GDK_TYPE_WAYLAND_CURSOR,
			 "cursor-type", GDK_CURSOR_IS_PIXMAP,
			 "display", display_wayland,
			 NULL);
  cursor->name = NULL;
  cursor->surface.hotspot_x = x;
  cursor->surface.hotspot_y = y;

  if (surface)
    {
      cursor->surface.width = cairo_image_surface_get_width (surface);
      cursor->surface.height = cairo_image_surface_get_height (surface);

      double sx, sy;
      cairo_surface_get_device_scale (surface, &sx, &sy);
      cursor->surface.scale = (int)sx;

      while ((cursor->surface.width % cursor->surface.scale != 0) ||
             (cursor->surface.height % cursor->surface.scale != 0))
        {
          g_warning_once (G_STRLOC " cursor image size (%dx%d) not an integer"
                     "multiple of scale (%d)", cursor->surface.width, cursor->surface.height,
                     cursor->surface.scale);
          cursor->surface.scale--;
        }

      cursor->surface.hotspot_x *= sx;
      cursor->surface.hotspot_y *= sx;
    }
  else
    {
      cursor->surface.width = 1;
      cursor->surface.height = 1;
      cursor->surface.scale = 1;
    }

  cursor->surface.cairo_surface =
    _gdk_wayland_display_create_shm_surface (display_wayland,
                                             cursor->surface.width / cursor->surface.scale,
                                             cursor->surface.height / cursor->surface.scale,
                                             cursor->surface.scale);

  buffer = _gdk_wayland_shm_surface_get_wl_buffer (cursor->surface.cairo_surface);
  wl_buffer_add_listener (buffer, &buffer_listener, cursor->surface.cairo_surface);

  if (surface)
    {
      cr = cairo_create (cursor->surface.cairo_surface);
      cairo_set_source_surface (cr, surface, 0, 0);
      cairo_paint (cr);
      cairo_destroy (cr);
    }

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
