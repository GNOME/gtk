/*
 * Copyright © 2018 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gdkmemorytextureprivate.h"

/**
 * GdkMemoryTexture:
 *
 * A `GdkTexture` representing image data in memory.
 */

struct _GdkMemoryTexture
{
  GdkTexture parent_instance;

  GdkMemoryFormat format;

  GBytes *bytes;
  gsize stride;
};

struct _GdkMemoryTextureClass
{
  GdkTextureClass parent_class;
};

G_DEFINE_TYPE (GdkMemoryTexture, gdk_memory_texture, GDK_TYPE_TEXTURE)

gsize
gdk_memory_format_bytes_per_pixel (GdkMemoryFormat format)
{
  switch (format)
    {
    case GDK_MEMORY_B8G8R8A8_PREMULTIPLIED:
    case GDK_MEMORY_A8R8G8B8_PREMULTIPLIED:
    case GDK_MEMORY_R8G8B8A8_PREMULTIPLIED:
    case GDK_MEMORY_B8G8R8A8:
    case GDK_MEMORY_A8R8G8B8:
    case GDK_MEMORY_R8G8B8A8:
    case GDK_MEMORY_A8B8G8R8:
      return 4;

    case GDK_MEMORY_R8G8B8:
    case GDK_MEMORY_B8G8R8:
      return 3;

    case GDK_MEMORY_N_FORMATS:
    default:
      g_assert_not_reached ();
      return 4;
    }
}

static void
gdk_memory_texture_dispose (GObject *object)
{
  GdkMemoryTexture *self = GDK_MEMORY_TEXTURE (object);

  g_clear_pointer (&self->bytes, g_bytes_unref);

  G_OBJECT_CLASS (gdk_memory_texture_parent_class)->dispose (object);
}

static void
gdk_memory_texture_download (GdkTexture         *texture,
                             const GdkRectangle *area,
                             guchar             *data,
                             gsize               stride)
{
  GdkMemoryTexture *self = GDK_MEMORY_TEXTURE (texture);

  gdk_memory_convert (data, stride,
                      GDK_MEMORY_CAIRO_FORMAT_ARGB32,
                      (guchar *) g_bytes_get_data (self->bytes, NULL)
                        + area->x * gdk_memory_format_bytes_per_pixel (self->format)
                        + area->y * self->stride,
                      self->stride,
                      self->format,
                      area->width, area->height);
}

static void
gdk_memory_texture_class_init (GdkMemoryTextureClass *klass)
{
  GdkTextureClass *texture_class = GDK_TEXTURE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  texture_class->download = gdk_memory_texture_download;
  gobject_class->dispose = gdk_memory_texture_dispose;
}

static void
gdk_memory_texture_init (GdkMemoryTexture *self)
{
}

/**
 * gdk_memory_texture_new:
 * @width: the width of the texture
 * @height: the height of the texture
 * @format: the format of the data
 * @bytes: the `GBytes` containing the pixel data
 * @stride: rowstride for the data
 *
 * Creates a new texture for a blob of image data.
 *
 * The `GBytes` must contain @stride x @height pixels
 * in the given format.
 *
 * Returns: A newly-created `GdkTexture`
 */
GdkTexture *
gdk_memory_texture_new (int              width,
                        int              height,
                        GdkMemoryFormat  format,
                        GBytes          *bytes,
                        gsize            stride)
{
  GdkMemoryTexture *self;

  self = g_object_new (GDK_TYPE_MEMORY_TEXTURE,
                       "width", width,
                       "height", height,
                       NULL);

  self->format = format;
  self->bytes = g_bytes_ref (bytes);
  self->stride = stride;

  return GDK_TEXTURE (self);
}

GdkMemoryFormat 
gdk_memory_texture_get_format (GdkMemoryTexture *self)
{
  return self->format;
}

const guchar *
gdk_memory_texture_get_data (GdkMemoryTexture *self)
{
  return g_bytes_get_data (self->bytes, NULL);
}

gsize
gdk_memory_texture_get_stride (GdkMemoryTexture *self)
{
  return self->stride;
}

static void
convert_memcpy (guchar       *dest_data,
                gsize         dest_stride,
                const guchar *src_data,
                gsize         src_stride,
                gsize         width,
                gsize         height)
{
  gsize y;

  for (y = 0; y < height; y++)
    memcpy (dest_data + y * dest_stride, src_data + y * src_stride, 4 * width);
}

#define SWIZZLE(A,R,G,B) \
static void \
convert_swizzle ## A ## R ## G ## B (guchar       *dest_data, \
                                     gsize         dest_stride, \
                                     const guchar *src_data, \
                                     gsize         src_stride, \
                                     gsize         width, \
                                     gsize         height) \
{ \
  gsize x, y; \
\
  for (y = 0; y < height; y++) \
    { \
      for (x = 0; x < width; x++) \
        { \
          dest_data[4 * x + A] = src_data[4 * x + 0]; \
          dest_data[4 * x + R] = src_data[4 * x + 1]; \
          dest_data[4 * x + G] = src_data[4 * x + 2]; \
          dest_data[4 * x + B] = src_data[4 * x + 3]; \
        } \
\
      dest_data += dest_stride; \
      src_data += src_stride; \
    } \
}

SWIZZLE(3,2,1,0)
SWIZZLE(2,1,0,3)
SWIZZLE(3,0,1,2)
SWIZZLE(1,2,3,0)

#define SWIZZLE_OPAQUE(A,R,G,B) \
static void \
convert_swizzle_opaque_## A ## R ## G ## B (guchar       *dest_data, \
                                            gsize         dest_stride, \
                                            const guchar *src_data, \
                                            gsize         src_stride, \
                                            gsize         width, \
                                            gsize         height) \
{ \
  gsize x, y; \
\
  for (y = 0; y < height; y++) \
    { \
      for (x = 0; x < width; x++) \
        { \
          dest_data[4 * x + A] = 0xFF; \
          dest_data[4 * x + R] = src_data[3 * x + 0]; \
          dest_data[4 * x + G] = src_data[3 * x + 1]; \
          dest_data[4 * x + B] = src_data[3 * x + 2]; \
        } \
\
      dest_data += dest_stride; \
      src_data += src_stride; \
    } \
}

SWIZZLE_OPAQUE(3,2,1,0)
SWIZZLE_OPAQUE(3,0,1,2)
SWIZZLE_OPAQUE(0,1,2,3)
SWIZZLE_OPAQUE(0,3,2,1)

#define PREMULTIPLY(d,c,a) G_STMT_START { guint t = c * a + 0x80; d = ((t >> 8) + t) >> 8; } G_STMT_END
#define SWIZZLE_PREMULTIPLY(A,R,G,B, A2,R2,G2,B2) \
static void \
convert_swizzle_premultiply_ ## A ## R ## G ## B ## _ ## A2 ## R2 ## G2 ## B2 \
                                    (guchar       *dest_data, \
                                     gsize         dest_stride, \
                                     const guchar *src_data, \
                                     gsize         src_stride, \
                                     gsize         width, \
                                     gsize         height) \
{ \
  gsize x, y; \
\
  for (y = 0; y < height; y++) \
    { \
      for (x = 0; x < width; x++) \
        { \
          dest_data[4 * x + A] = src_data[4 * x + A2]; \
          PREMULTIPLY(dest_data[4 * x + R], src_data[4 * x + R2], src_data[4 * x + A2]); \
          PREMULTIPLY(dest_data[4 * x + G], src_data[4 * x + G2], src_data[4 * x + A2]); \
          PREMULTIPLY(dest_data[4 * x + B], src_data[4 * x + B2], src_data[4 * x + A2]); \
        } \
\
      dest_data += dest_stride; \
      src_data += src_stride; \
    } \
}

SWIZZLE_PREMULTIPLY (3,2,1,0, 3,2,1,0)
SWIZZLE_PREMULTIPLY (0,1,2,3, 3,2,1,0)
SWIZZLE_PREMULTIPLY (3,2,1,0, 0,1,2,3)
SWIZZLE_PREMULTIPLY (0,1,2,3, 0,1,2,3)
SWIZZLE_PREMULTIPLY (3,2,1,0, 3,0,1,2)
SWIZZLE_PREMULTIPLY (0,1,2,3, 3,0,1,2)
SWIZZLE_PREMULTIPLY (3,2,1,0, 0,3,2,1)
SWIZZLE_PREMULTIPLY (0,1,2,3, 0,3,2,1)
SWIZZLE_PREMULTIPLY (3,0,1,2, 3,2,1,0)
SWIZZLE_PREMULTIPLY (3,0,1,2, 0,1,2,3)
SWIZZLE_PREMULTIPLY (3,0,1,2, 3,0,1,2)
SWIZZLE_PREMULTIPLY (3,0,1,2, 0,3,2,1)

typedef void (* ConversionFunc) (guchar       *dest_data,
                                 gsize         dest_stride,
                                 const guchar *src_data,
                                 gsize         src_stride,
                                 gsize         width,
                                 gsize         height);

static ConversionFunc converters[GDK_MEMORY_N_FORMATS][3] =
{
  { convert_memcpy, convert_swizzle3210, convert_swizzle2103 },
  { convert_swizzle3210, convert_memcpy, convert_swizzle3012 },
  { convert_swizzle2103, convert_swizzle1230, convert_memcpy },
  { convert_swizzle_premultiply_3210_3210, convert_swizzle_premultiply_0123_3210, convert_swizzle_premultiply_3012_3210,  },
  { convert_swizzle_premultiply_3210_0123, convert_swizzle_premultiply_0123_0123, convert_swizzle_premultiply_3012_0123 },
  { convert_swizzle_premultiply_3210_3012, convert_swizzle_premultiply_0123_3012, convert_swizzle_premultiply_3012_3012 },
  { convert_swizzle_premultiply_3210_0321, convert_swizzle_premultiply_0123_0321, convert_swizzle_premultiply_3012_0321 },
  { convert_swizzle_opaque_3210, convert_swizzle_opaque_0123, convert_swizzle_opaque_3012 },
  { convert_swizzle_opaque_3012, convert_swizzle_opaque_0321, convert_swizzle_opaque_3210 }
};

void
gdk_memory_convert (guchar          *dest_data,
                    gsize            dest_stride,
                    GdkMemoryFormat  dest_format,
                    const guchar    *src_data,
                    gsize            src_stride,
                    GdkMemoryFormat  src_format,
                    gsize            width,
                    gsize            height)
{
  g_assert (dest_format < 3);
  g_assert (src_format < GDK_MEMORY_N_FORMATS);

  converters[src_format][dest_format] (dest_data, dest_stride, src_data, src_stride, width, height);
}
