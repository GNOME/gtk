/* gdkcursor-quartz.c
 *
 * Copyright (C) 2005-2007 Imendio AB
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

#include "config.h"

#include "gdkdisplay.h"
#include "gdkcursor.h"
#include "gdkcursorprivate.h"
#include "gdkquartzcursor.h"
#include "gdkprivate-quartz.h"

#include "xcursors.h"

struct _GdkQuartzCursor
{
  GdkCursor cursor;

  NSCursor *nscursor;
};

struct _GdkQuartzCursorClass
{
  GdkCursorClass cursor_class;
};


static GdkCursor *cached_xcursors[G_N_ELEMENTS (xcursors)];

static GdkCursor *
gdk_quartz_cursor_new_from_nscursor (NSCursor      *nscursor,
                                     GdkCursorType  cursor_type)
{
  GdkQuartzCursor *private;

  private = g_object_new (GDK_TYPE_QUARTZ_CURSOR,
                          "cursor-type", cursor_type,
                          "display", _gdk_display,
                          NULL);
  private->nscursor = nscursor;

  return GDK_CURSOR (private);
}

static GdkCursor *
create_blank_cursor (void)
{
  NSCursor *nscursor;
  NSImage *nsimage;
  NSSize size = { 1.0, 1.0 };

  nsimage = [[NSImage alloc] initWithSize:size];
  nscursor = [[NSCursor alloc] initWithImage:nsimage
                               hotSpot:NSMakePoint(0.0, 0.0)];
  [nsimage release];

  return gdk_quartz_cursor_new_from_nscursor (nscursor, GDK_BLANK_CURSOR);
}

static gboolean
get_bit (const guchar *data,
         gint          width,
         gint          height,
         gint          x,
         gint          y)
{
  gint bytes_per_line;
  const guchar *src;

  if (x < 0 || y < 0 || x >= width || y >= height)
    return FALSE;

  bytes_per_line = (width + 7) / 8;

  src = &data[y * bytes_per_line];
  return ((src[x / 8] >> x % 8) & 1);
}

static GdkCursor *
create_builtin_cursor (GdkCursorType cursor_type)
{
  GdkCursor *cursor;
  NSBitmapImageRep *bitmap_rep;
  NSInteger mask_width, mask_height;
  gint src_width, src_height;
  gint dst_stride;
  const guchar *mask_start, *src_start;
  gint dx, dy;
  gint x, y;
  NSPoint hotspot;
  NSImage *image;
  NSCursor *nscursor;

  if (cursor_type >= G_N_ELEMENTS (xcursors) || cursor_type < 0)
    return NULL;

  cursor = cached_xcursors[cursor_type];
  if (cursor)
    return cursor;

  GDK_QUARTZ_ALLOC_POOL;

  src_width = xcursors[cursor_type].width;
  src_height = xcursors[cursor_type].height;
  mask_width = xcursors[cursor_type+1].width;
  mask_height = xcursors[cursor_type+1].height;

  bitmap_rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:NULL
		pixelsWide:mask_width pixelsHigh:mask_height
		bitsPerSample:8 samplesPerPixel:4
		hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace
		bytesPerRow:0 bitsPerPixel:0];

  dst_stride = [bitmap_rep bytesPerRow];

  src_start = xcursors[cursor_type].bits;
  mask_start = xcursors[cursor_type+1].bits;

  dx = xcursors[cursor_type+1].hotx - xcursors[cursor_type].hotx;
  dy = xcursors[cursor_type+1].hoty - xcursors[cursor_type].hoty;

  for (y = 0; y < mask_height; y++)
    {
      guchar *dst = [bitmap_rep bitmapData] + y * dst_stride;

      for (x = 0; x < mask_width; x++)
	{
	  if (get_bit (mask_start, mask_width, mask_height, x, y))
            {
              if (get_bit (src_start, src_width, src_height, x - dx, y - dy))
                {
                  *dst++ = 0;
                  *dst++ = 0;
                  *dst++ = 0;
                }
              else
                {
                  *dst++ = 0xff;
                  *dst++ = 0xff;
                  *dst++ = 0xff;
                }

              *dst++ = 0xff;
            }
	  else
            {
              *dst++ = 0;
              *dst++ = 0;
              *dst++ = 0;
              *dst++ = 0;
            }
        }
    }

  image = [[NSImage alloc] init];
  [image addRepresentation:bitmap_rep];
  [bitmap_rep release];

  hotspot = NSMakePoint (xcursors[cursor_type+1].hotx,
                         xcursors[cursor_type+1].hoty);

  nscursor = [[NSCursor alloc] initWithImage:image hotSpot:hotspot];
  [image release];

  cursor = gdk_quartz_cursor_new_from_nscursor (nscursor, GDK_CURSOR_IS_PIXMAP);

  cached_xcursors[cursor_type] = g_object_ref (cursor);

  GDK_QUARTZ_RELEASE_POOL;

  return cursor;
}

GdkCursor*
_gdk_quartz_display_get_cursor_for_type (GdkDisplay    *display,
                                         GdkCursorType  cursor_type)
{
  NSCursor *nscursor;

  g_return_val_if_fail (display == gdk_display_get_default (), NULL);

  switch (cursor_type)
    {
    case GDK_XTERM:
      nscursor = [NSCursor IBeamCursor];
      break;
    case GDK_SB_H_DOUBLE_ARROW:
      nscursor = [NSCursor resizeLeftRightCursor];
      break;
    case GDK_SB_V_DOUBLE_ARROW:
      nscursor = [NSCursor resizeUpDownCursor];
      break;
    case GDK_SB_UP_ARROW:
    case GDK_BASED_ARROW_UP:
    case GDK_BOTTOM_TEE:
    case GDK_TOP_SIDE:
      nscursor = [NSCursor resizeUpCursor];
      break;
    case GDK_SB_DOWN_ARROW:
    case GDK_BASED_ARROW_DOWN:
    case GDK_TOP_TEE:
    case GDK_BOTTOM_SIDE:
      nscursor = [NSCursor resizeDownCursor];
      break;
    case GDK_SB_LEFT_ARROW:
    case GDK_RIGHT_TEE:
    case GDK_LEFT_SIDE:
      nscursor = [NSCursor resizeLeftCursor];
      break;
    case GDK_SB_RIGHT_ARROW:
    case GDK_LEFT_TEE:
    case GDK_RIGHT_SIDE:
      nscursor = [NSCursor resizeRightCursor];
      break;
    case GDK_TCROSS:
    case GDK_CROSS:
    case GDK_CROSSHAIR:
    case GDK_DIAMOND_CROSS:
      nscursor = [NSCursor crosshairCursor];
      break;
    case GDK_HAND1:
    case GDK_HAND2:
      nscursor = [NSCursor pointingHandCursor];
      break;
    case GDK_CURSOR_IS_PIXMAP:
      return NULL;
    case GDK_BLANK_CURSOR:
      return create_blank_cursor ();
    default:
      return g_object_ref (create_builtin_cursor (cursor_type));
    }

  [nscursor retain];
  return gdk_quartz_cursor_new_from_nscursor (nscursor, cursor_type);
}


GdkCursor *
_gdk_quartz_display_get_cursor_for_surface (GdkDisplay      *display,
					    cairo_surface_t *surface,
					    gdouble          x,
					    gdouble          y)
{
  NSImage *image;
  NSCursor *nscursor;
  GdkCursor *cursor;
  GdkPixbuf *pixbuf;

  GDK_QUARTZ_ALLOC_POOL;

  pixbuf = gdk_pixbuf_get_from_surface (surface, 0, 0,
					cairo_image_surface_get_width (surface),
					cairo_image_surface_get_height (surface));
  image = gdk_quartz_pixbuf_to_ns_image_libgtk_only (pixbuf);
  nscursor = [[NSCursor alloc] initWithImage:image hotSpot:NSMakePoint(x, y)];

  cursor = gdk_quartz_cursor_new_from_nscursor (nscursor, GDK_CURSOR_IS_PIXMAP);

  g_object_unref (pixbuf);

  GDK_QUARTZ_RELEASE_POOL;

  return cursor;
}

GdkCursor*
_gdk_quartz_display_get_cursor_for_name (GdkDisplay  *display,
                                         const gchar *name)
{
  /* FIXME: Implement */
  return NULL;
}

G_DEFINE_TYPE (GdkQuartzCursor, gdk_quartz_cursor, GDK_TYPE_CURSOR)

static cairo_surface_t *gdk_quartz_cursor_get_surface (GdkCursor *cursor,
						       gdouble *x_hot,
						       gdouble *y_hot);

static void
gdk_quartz_cursor_finalize (GObject *object)
{
  GdkQuartzCursor *private = GDK_QUARTZ_CURSOR (object);

  if (private->nscursor)
    [private->nscursor release];
  private->nscursor = NULL;
}

static void
gdk_quartz_cursor_class_init (GdkQuartzCursorClass *quartz_cursor_class)
{
  GdkCursorClass *cursor_class = GDK_CURSOR_CLASS (quartz_cursor_class);
  GObjectClass *object_class = G_OBJECT_CLASS (quartz_cursor_class);

  object_class->finalize = gdk_quartz_cursor_finalize;

  cursor_class->get_surface = gdk_quartz_cursor_get_surface;
}

static void
gdk_quartz_cursor_init (GdkQuartzCursor *cursor)
{
}


gboolean
_gdk_quartz_display_supports_cursor_alpha (GdkDisplay *display)
{
  return TRUE;
}

gboolean
_gdk_quartz_display_supports_cursor_color (GdkDisplay *display)
{
  return TRUE;
}

void
_gdk_quartz_display_get_default_cursor_size (GdkDisplay *display,
                                             guint      *width,
                                             guint      *height)
{
  /* Mac OS X doesn't have the notion of a default size */
  *width = 32;
  *height = 32;
}

void
_gdk_quartz_display_get_maximal_cursor_size (GdkDisplay *display,
                                             guint       *width,
                                             guint       *height)
{
  /* Cursor sizes in Mac OS X can be arbitrarily large */
  *width = 65536;
  *height = 65536;
}

NSCursor *
_gdk_quartz_cursor_get_ns_cursor (GdkCursor *cursor)
{
  GdkQuartzCursor *cursor_private;

  if (!cursor)
    return [NSCursor arrowCursor];

  g_return_val_if_fail (GDK_IS_QUARTZ_CURSOR (cursor), NULL);

  cursor_private = GDK_QUARTZ_CURSOR (cursor);

  return cursor_private->nscursor;
}

static cairo_surface_t *
gdk_quartz_cursor_get_surface (GdkCursor *cursor,
			       gdouble *x_hot,
			       gdouble *y_hot)
{
  /* FIXME: Implement */
  return NULL;
}
