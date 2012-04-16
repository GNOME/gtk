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

#include <sys/mman.h>

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
  int x, y, width, height, size;
  void *map;
  struct wl_buffer *buffer;
};

struct _GdkWaylandCursorClass
{
  GdkCursorClass cursor_class;
};

G_DEFINE_TYPE (GdkWaylandCursor, _gdk_wayland_cursor, GDK_TYPE_CURSOR)

static guint theme_serial = 0;

static void
gdk_wayland_cursor_finalize (GObject *object)
{
  GdkWaylandCursor *cursor = GDK_WAYLAND_CURSOR (object);

  g_free (cursor->name);

  G_OBJECT_CLASS (_gdk_wayland_cursor_parent_class)->finalize (object);
}

static GdkPixbuf*
gdk_wayland_cursor_get_image (GdkCursor *cursor)
{
  return NULL;
}

struct wl_buffer *
_gdk_wayland_cursor_get_buffer (GdkCursor *cursor, int *x, int *y)
{
  GdkWaylandCursor *wayland_cursor = GDK_WAYLAND_CURSOR (cursor);

  *x = wayland_cursor->x;
  *y = wayland_cursor->y;

  return wayland_cursor->buffer;
}

static void
_gdk_wayland_cursor_class_init (GdkWaylandCursorClass *wayland_cursor_class)
{
  GdkCursorClass *cursor_class = GDK_CURSOR_CLASS (wayland_cursor_class);
  GObjectClass *object_class = G_OBJECT_CLASS (wayland_cursor_class);

  object_class->finalize = gdk_wayland_cursor_finalize;

  cursor_class->get_image = gdk_wayland_cursor_get_image;
}

static void
_gdk_wayland_cursor_init (GdkWaylandCursor *cursor)
{
}

static void
set_pixbuf (GdkWaylandCursor *cursor, GdkPixbuf *pixbuf)
{
  int stride, i, n_channels;
  unsigned char *pixels, *end, *argb_pixels, *s, *d;

  stride = gdk_pixbuf_get_rowstride(pixbuf);
  pixels = gdk_pixbuf_get_pixels(pixbuf);
  n_channels = gdk_pixbuf_get_n_channels(pixbuf);
  argb_pixels = cursor->map;

#define MULT(_d,c,a,t) \
	do { t = c * a + 0x7f; _d = ((t >> 8) + t) >> 8; } while (0)

  if (n_channels == 4)
    {
      for (i = 0; i < cursor->height; i++)
	{
	  s = pixels + i * stride;
	  end = s + cursor->width * 4;
	  d = argb_pixels + i * cursor->width * 4;
	  while (s < end)
	    {
	      unsigned int t;

	      MULT(d[0], s[2], s[3], t);
	      MULT(d[1], s[1], s[3], t);
	      MULT(d[2], s[0], s[3], t);
	      d[3] = s[3];
	      s += 4;
	      d += 4;
	    }
	}
    }
  else if (n_channels == 3)
    {
      for (i = 0; i < cursor->height; i++)
	{
	  s = pixels + i * stride;
	  end = s + cursor->width * 3;
	  d = argb_pixels + i * cursor->width * 4;
	  while (s < end)
	    {
	      d[0] = s[2];
	      d[1] = s[1];
	      d[2] = s[0];
	      d[3] = 0xff;
	      s += 3;
	      d += 4;
	    }
	}
    }
}

static GdkCursor *
create_cursor(GdkWaylandDisplay *display, GdkPixbuf *pixbuf, int x, int y)
{
  GdkWaylandCursor *cursor;
  int stride, fd;
  char *filename;
  GError *error = NULL;

  cursor = g_object_new (GDK_TYPE_WAYLAND_CURSOR,
			 "cursor-type", GDK_CURSOR_IS_PIXMAP,
			 "display", display,
			 NULL);
  cursor->name = NULL;
  cursor->serial = theme_serial;
  cursor->x = x;
  cursor->y = y;
  if (pixbuf)
    {
      cursor->width = gdk_pixbuf_get_width (pixbuf);
      cursor->height = gdk_pixbuf_get_height (pixbuf);
    }
  else
    {
      cursor->width = 1;
      cursor->height = 1;
    }

  stride = cursor->width * 4;
  cursor->size = stride * cursor->height;

  fd = g_file_open_tmp("wayland-shm-XXXXXX", &filename, &error);
  if (fd < 0) {
    fprintf(stderr, "g_file_open_tmp failed: %s\n", error->message);
    g_error_free (error);
    return NULL;
  }

  unlink (filename);
  g_free (filename);

  if (ftruncate(fd, cursor->size) < 0) {
    fprintf(stderr, "ftruncate failed: %m\n");
    close(fd);
    return NULL;
  }

  cursor->map = mmap(NULL, cursor->size,
		     PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (cursor->map == MAP_FAILED) {
    fprintf(stderr, "mmap failed: %m\n");
    close(fd);
    return NULL;
  }

  if (pixbuf)
    set_pixbuf (cursor, pixbuf);
  else
    memset (cursor->map, 0, 4);

  cursor->buffer = wl_shm_create_buffer(display->shm,
					fd,
					cursor->width,
					cursor->height,
					stride, WL_SHM_FORMAT_ARGB8888);

  close(fd);

  return GDK_CURSOR (cursor);
}

static const struct {
  GdkCursorType type;
  const char *filename;
  int hotspot_x, hotspot_y;
} cursor_definitions[] = {
  { GDK_BLANK_CURSOR, NULL, 0, 0 },
  { GDK_HAND1, "hand1.png", 18, 11 },
  { GDK_HAND2, "hand2.png", 14,  8 },
  { GDK_LEFT_PTR, "left_ptr.png", 10, 5 },
  { GDK_SB_H_DOUBLE_ARROW, "sb_h_double_arrow.png", 15, 15 },
  { GDK_SB_V_DOUBLE_ARROW, "sb_v_double_arrow.png", 15, 15 },
  { GDK_XTERM, "xterm.png", 15, 15 },
  { GDK_BOTTOM_RIGHT_CORNER, "bottom_right_corner.png", 28, 28 }
};

GdkCursor *
_gdk_wayland_display_get_cursor_for_type (GdkDisplay    *display,
					  GdkCursorType  cursor_type)
{
  GdkWaylandDisplay *wayland_display;
  GdkPixbuf *pixbuf = NULL;
  GError *error = NULL;
  int i;

  for (i = 0; i < G_N_ELEMENTS (cursor_definitions); i++)
    {
      if (cursor_definitions[i].type == cursor_type)
	break;
    }

  if (i == G_N_ELEMENTS (cursor_definitions))
    {
      g_warning ("Unhandled cursor type %d, falling back to blank\n",
                 cursor_type);
      i = 0;
    }

  wayland_display = GDK_WAYLAND_DISPLAY (display);
  if (!wayland_display->cursors)
    wayland_display->cursors =
      g_new0 (GdkCursor *, G_N_ELEMENTS(cursor_definitions));
  if (wayland_display->cursors[i])
    return g_object_ref (wayland_display->cursors[i]);

  GDK_NOTE (CURSOR,
	    g_message ("Creating new cursor for type %d, filename %s",
		       cursor_type, cursor_definitions[i].filename));

  if (cursor_type != GDK_BLANK_CURSOR)
    {
      const gchar * const *directories;
      gint j;

      directories = g_get_system_data_dirs();

      for (j = 0; directories[j] != NULL; j++)
        {
          gchar *filename;
          filename = g_build_filename (directories[j],
                                       "weston",
                                       cursor_definitions[i].filename,
                                       NULL);
          if (g_file_test (filename, G_FILE_TEST_EXISTS))
            {
              pixbuf = gdk_pixbuf_new_from_file (filename, &error);

              if (error != NULL)
                {
                  g_warning ("Failed to load cursor: %s: %s",
                             filename, error->message);
                  g_error_free(error);
                  return NULL;
                }
              break;
            }
        }

      if (!pixbuf)
        {
          g_warning ("Unable to find cursor for: %s",
                     cursor_definitions[i].filename);
          return NULL;
        }
    }

  wayland_display->cursors[i] =
    create_cursor(wayland_display, pixbuf,
		  cursor_definitions[i].hotspot_x,
		  cursor_definitions[i].hotspot_y);
  if (pixbuf)
    g_object_unref (pixbuf);

  return g_object_ref (wayland_display->cursors[i]);
}

GdkCursor*
_gdk_wayland_display_get_cursor_for_name (GdkDisplay  *display,
					  const gchar *name)
{
  GdkWaylandCursor *private;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  private = g_object_new (GDK_TYPE_WAYLAND_CURSOR,
                          "cursor-type", GDK_CURSOR_IS_PIXMAP,
                          "display", display,
                          NULL);
  private->name = g_strdup (name);
  private->serial = theme_serial;

  return GDK_CURSOR (private);
}

GdkCursor *
_gdk_wayland_display_get_cursor_for_pixbuf (GdkDisplay *display,
					    GdkPixbuf  *pixbuf,
					    gint        x,
					    gint        y)
{
  GdkWaylandCursor *private;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);
  g_return_val_if_fail (0 <= x && x < gdk_pixbuf_get_width (pixbuf), NULL);
  g_return_val_if_fail (0 <= y && y < gdk_pixbuf_get_height (pixbuf), NULL);

  private = g_object_new (GDK_TYPE_WAYLAND_CURSOR,
                          "cursor-type", GDK_CURSOR_IS_PIXMAP,
                          "display", display,
                          NULL);

  private->name = NULL;
  private->serial = theme_serial;

  return GDK_CURSOR (private);
}

void
_gdk_wayland_display_get_default_cursor_size (GdkDisplay *display,
					      guint       *width,
					      guint       *height)
{
  /* FIXME: wayland settings? */
  *width = 64;
  *height = 64;
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
