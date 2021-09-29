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

#include "gdkcolorprofileprivate.h"
#include "gdkmemoryformatprivate.h"
#include "gsk/ngl/fp16private.h"

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

static void
gdk_memory_texture_dispose (GObject *object)
{
  GdkMemoryTexture *self = GDK_MEMORY_TEXTURE (object);

  g_clear_pointer (&self->bytes, g_bytes_unref);

  G_OBJECT_CLASS (gdk_memory_texture_parent_class)->dispose (object);
}

static GdkTexture *
gdk_memory_texture_download_texture (GdkTexture *texture)
{
  return g_object_ref (texture);
}

static void
gdk_memory_texture_download (GdkTexture *texture,
                             guchar     *data,
                             gsize       stride)
{
  GdkMemoryTexture *self = GDK_MEMORY_TEXTURE (texture);

  gdk_memory_convert (data, stride,
                      GDK_MEMORY_DEFAULT,
                      gdk_color_profile_get_srgb (),
                      (guchar *) g_bytes_get_data (self->bytes, NULL),
                      self->stride,
                      self->format,
                      gdk_texture_get_color_profile (texture),
                      gdk_texture_get_width (texture),
                      gdk_texture_get_height (texture));
}

static void
gdk_memory_texture_download_float (GdkTexture *texture,
                                   float      *data,
                                   gsize       stride)
{
  GdkMemoryTexture *self = GDK_MEMORY_TEXTURE (texture);

  gdk_memory_convert ((guchar *) data,
                      stride * sizeof (float),
                      GDK_MEMORY_R32G32B32A32_FLOAT_PREMULTIPLIED,
                      gdk_color_profile_get_srgb (),
                      (guchar *) g_bytes_get_data (self->bytes, NULL),
                      self->stride,
                      self->format,
                      gdk_texture_get_color_profile (texture),
                      gdk_texture_get_width (texture),
                      gdk_texture_get_height (texture));
}

static void
gdk_memory_texture_class_init (GdkMemoryTextureClass *klass)
{
  GdkTextureClass *texture_class = GDK_TEXTURE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  texture_class->download_texture = gdk_memory_texture_download_texture;
  texture_class->download = gdk_memory_texture_download;
  texture_class->download_float = gdk_memory_texture_download_float;
  gobject_class->dispose = gdk_memory_texture_dispose;
}

static void
gdk_memory_texture_init (GdkMemoryTexture *self)
{
}

static GBytes *
gdk_memory_sanitize (GBytes          *bytes,
                     int              width,
                     int              height,
                     GdkMemoryFormat  format,
                     gsize            stride,
                     gsize           *out_stride)
{
  gsize align, size, copy_stride, bpp;
  const guchar *data;
  guchar *copy;
  int y;

  data = g_bytes_get_data (bytes, &size);
  align = gdk_memory_format_alignment (format);

  if (GPOINTER_TO_SIZE (data) % align == 0 &&
      stride % align == 0)
    {
      *out_stride = stride;
      return g_bytes_ref (bytes);
    }

  bpp = gdk_memory_format_bytes_per_pixel (format);
  copy_stride = bpp * width;
  /* align to multiples of 4, just to be sure */
  copy_stride = (copy_stride + 3) & ~3;
  copy = g_malloc (copy_stride * height);
  for (y = 0; y < height; y++)
    memcpy (copy + y * copy_stride, data + y * stride, bpp * width);

  *out_stride = copy_stride;
  return g_bytes_new_take (copy, copy_stride * height);
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
 * This function calls [ctor@Gdk.MemoryTexture.new_with_color_profile]
 * with the sRGB profile.
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
  g_return_val_if_fail (width > 0, NULL);
  g_return_val_if_fail (height > 0, NULL);
  g_return_val_if_fail (bytes != NULL, NULL);
  g_return_val_if_fail (stride >= width * gdk_memory_format_bytes_per_pixel (format), NULL);

  return gdk_memory_texture_new_with_color_profile (width, height,
                                                    format,
                                                    gdk_color_profile_get_srgb (),
                                                    bytes, stride);
}

/**
 * gdk_memory_texture_new_with_color_profile:
 * @width: the width of the texture
 * @height: the height of the texture
 * @format: the format of the data
 * @color_profile: a `GdkColorProfile`
 * @bytes: the `GBytes` containing the pixel data
 * @stride: rowstride for the data
 *
 * Creates a new texture for a blob of image data with a given color profile.
 *
 * The `GBytes` must contain @stride x @height pixels
 * in the given format.
 *
 * Returns: A newly-created `GdkTexture`
 */
GdkTexture *
gdk_memory_texture_new_with_color_profile (int              width,
                                           int              height,
                                           GdkMemoryFormat  format,
                                           GdkColorProfile *color_profile,
                                           GBytes          *bytes,
                                           gsize            stride)
{
  GdkMemoryTexture *self;

  g_return_val_if_fail (width > 0, NULL);
  g_return_val_if_fail (height > 0, NULL);
  g_return_val_if_fail (GDK_IS_COLOR_PROFILE (color_profile), NULL);
  g_return_val_if_fail (bytes != NULL, NULL);
  g_return_val_if_fail (stride >= width * gdk_memory_format_bytes_per_pixel (format), NULL);
  g_return_val_if_fail (gdk_color_profile_supports_memory_format (color_profile, format), NULL);

  bytes = gdk_memory_sanitize (bytes, width, height, format, stride, &stride);

  self = g_object_new (GDK_TYPE_MEMORY_TEXTURE,
                       "width", width,
                       "height", height,
                       "color-profile", color_profile,
                       NULL);

  self->format = format;
  self->bytes = bytes;
  self->stride = stride;

  return GDK_TEXTURE (self);
}

GdkMemoryTexture *
gdk_memory_texture_convert (GdkMemoryTexture   *source,
                            GdkMemoryFormat     to_format,
                            GdkColorProfile    *to_profile,
                            const GdkRectangle *rect_or_null)
{
  GdkTexture *texture = GDK_TEXTURE (source);
  GdkTexture *result;
  GBytes *to_bytes;
  int width, height;
  gsize offset, stride;

  width = texture->width;
  height = texture->height;
  if (rect_or_null)
    {
      g_assert (rect_or_null->x + rect_or_null->width <= width);
      g_assert (rect_or_null->y + rect_or_null->height <= height);

      offset = rect_or_null->y * source->stride +
               rect_or_null->x * gdk_memory_format_bytes_per_pixel (source->format);
      width = rect_or_null->width;
      height = rect_or_null->height;
    }
  else
    offset = 0;

  if (to_format == source->format &&
      gdk_color_profile_equal (texture->color_profile, to_profile))
    {
      if (offset == 0 && width == texture->width && height == texture->height)
        return source;

      to_bytes = g_bytes_new_from_bytes (source->bytes,
                                         offset,
                                         source->stride * height);
      stride = source->stride;
    }
  else
    {
      guchar *data;

      stride = gdk_memory_format_bytes_per_pixel (to_format) * width;
      data = g_malloc_n (stride, height);
      gdk_memory_convert (data,
                          stride,
                          to_format,
                          to_profile,
                          (guchar *) g_bytes_get_data (source->bytes, NULL) + offset,
                          source->stride,
                          source->format,
                          texture->color_profile,
                          width,
                          height);
      to_bytes = g_bytes_new_take (data, stride * height);
    }

  result = gdk_memory_texture_new_with_color_profile (width,
                                                      height,
                                                      to_format,
                                                      to_profile,
                                                      to_bytes,
                                                      stride);

  g_bytes_unref (to_bytes);
  g_object_unref (source);

  return GDK_MEMORY_TEXTURE (result);
}

GdkMemoryFormat 
gdk_memory_texture_get_format (GdkMemoryTexture *self)
{
  return self->format;
}

GBytes *
gdk_memory_texture_get_bytes (GdkMemoryTexture *self)
{
  return self->bytes;
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

