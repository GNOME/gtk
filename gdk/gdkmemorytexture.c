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

#include "gdkcolorstateprivate.h"
#include "gdkmemoryformatprivate.h"

/**
 * GdkMemoryTexture:
 *
 * A `GdkTexture` representing image data in memory.
 */

struct _GdkMemoryTexture
{
  GdkTexture parent_instance;

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

static void
gdk_memory_texture_download (GdkTexture      *texture,
                             GdkMemoryFormat  format,
                             GdkColorState   *color_state,
                             guchar          *data,
                             gsize            stride)
{
  GdkMemoryTexture *self = GDK_MEMORY_TEXTURE (texture);

  gdk_memory_convert (data, stride,
                      format,
                      color_state,
                      (guchar *) g_bytes_get_data (self->bytes, NULL),
                      self->stride,
                      texture->format,
                      texture->color_state,
                      gdk_texture_get_width (texture),
                      gdk_texture_get_height (texture));
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
      return bytes;
    }

  bpp = gdk_memory_format_bytes_per_pixel (format);
  copy_stride = bpp * width;
  /* align to multiples of 4, just to be sure */
  copy_stride = (copy_stride + 3) & ~3;
  copy = g_malloc (copy_stride * height);
  for (y = 0; y < height; y++)
    memcpy (copy + y * copy_stride, data + y * stride, bpp * width);

  g_bytes_unref (bytes);

  *out_stride = copy_stride;
  return g_bytes_new_take (copy, copy_stride * height);
}

GdkTexture *
gdk_memory_texture_new_from_builder (GdkMemoryTextureBuilder *builder)
{
  GdkMemoryTexture *self;
  GdkTexture *texture, *update_texture;

  self = g_object_new (GDK_TYPE_MEMORY_TEXTURE,
                       "width", gdk_memory_texture_builder_get_width (builder),
                       "height", gdk_memory_texture_builder_get_height (builder),
                       "color-state", gdk_memory_texture_builder_get_color_state (builder),
                       NULL);
  texture = GDK_TEXTURE (self);

  texture->format = gdk_memory_texture_builder_get_format (builder);
  self->bytes = gdk_memory_sanitize (g_bytes_ref (gdk_memory_texture_builder_get_bytes (builder)),
                                     texture->width,
                                     texture->height,
                                     texture->format,
                                     gdk_memory_texture_builder_get_stride (builder),
                                     &self->stride);

  update_texture = gdk_memory_texture_builder_get_update_texture (builder);
  if (update_texture)
    {
      cairo_region_t *update_region = gdk_memory_texture_builder_get_update_region (builder);
      if (update_region)
        {
          update_region = cairo_region_copy (update_region);
          cairo_region_intersect_rectangle (update_region,
                                            &(cairo_rectangle_int_t) {
                                              0, 0,
                                              update_texture->width, update_texture->height
                                            });
          gdk_texture_set_diff (GDK_TEXTURE (self), update_texture, update_region);
        }
    }

  return GDK_TEXTURE (self);
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
 * The `GBytes` must contain @stride × @height pixels
 * in the given format.
 *
 * Returns: (type GdkMemoryTexture): A newly-created `GdkTexture`
 */
GdkTexture *
gdk_memory_texture_new (int              width,
                        int              height,
                        GdkMemoryFormat  format,
                        GBytes          *bytes,
                        gsize            stride)
{
  GdkMemoryTexture *self;

  g_return_val_if_fail (width > 0, NULL);
  g_return_val_if_fail (height > 0, NULL);
  g_return_val_if_fail (bytes != NULL, NULL);
  g_return_val_if_fail (stride >= width * gdk_memory_format_bytes_per_pixel (format), NULL);
  /* needs to be this complex to support subtexture of the bottom right part */
  g_return_val_if_fail (g_bytes_get_size (bytes) >= gdk_memory_format_min_buffer_size (format, stride, width, height), NULL);

  bytes = gdk_memory_sanitize (g_bytes_ref (bytes), width, height, format, stride, &stride);

  self = g_object_new (GDK_TYPE_MEMORY_TEXTURE,
                       "width", width,
                       "height", height,
                       "color-state", GDK_COLOR_STATE_SRGB,
                       NULL);

  GDK_TEXTURE (self)->format = format;
  self->bytes = bytes;
  self->stride = stride;

  return GDK_TEXTURE (self);
}

GdkTexture *
gdk_memory_texture_new_subtexture (GdkMemoryTexture  *source,
                                   int                x,
                                   int                y,
                                   int                width,
                                   int                height)
{
  GdkTexture *texture, *result;
  gsize offset, size;
  GBytes *bytes;

  g_return_val_if_fail (GDK_IS_MEMORY_TEXTURE (source), NULL);
  g_return_val_if_fail (x >= 0 && x < GDK_TEXTURE (source)->width, NULL);
  g_return_val_if_fail (y >= 0 && y < GDK_TEXTURE (source)->height, NULL);
  g_return_val_if_fail (width > 0 && x + width <= GDK_TEXTURE (source)->width, NULL);
  g_return_val_if_fail (height > 0 && y + height <= GDK_TEXTURE (source)->height, NULL);

  texture = GDK_TEXTURE (source);
  offset = y * source->stride + x * gdk_memory_format_bytes_per_pixel (texture->format);
  size = gdk_memory_format_min_buffer_size (texture->format, source->stride, width, height);
  bytes = g_bytes_new_from_bytes (source->bytes, offset, size);

  result = gdk_memory_texture_new (width,
                                   height,
                                   texture->format,
                                   bytes,
                                   source->stride);
  g_bytes_unref (bytes);

  return result;
}

GdkMemoryTexture *
gdk_memory_texture_from_texture (GdkTexture *texture)
{
  GdkTexture *result;
  GBytes *bytes;
  guchar *data;
  gsize stride;

  g_return_val_if_fail (GDK_IS_TEXTURE (texture), NULL);

  if (GDK_IS_MEMORY_TEXTURE (texture))
    return g_object_ref (GDK_MEMORY_TEXTURE (texture));

  stride = texture->width * gdk_memory_format_bytes_per_pixel (texture->format);
  data = g_malloc_n (stride, texture->height);

  gdk_texture_do_download (texture, texture->format, texture->color_state, data, stride);
  bytes = g_bytes_new_take (data, stride * texture->height);
  result = gdk_memory_texture_new (texture->width,
                                   texture->height,
                                   texture->format,
                                   bytes,
                                   stride);
  g_bytes_unref (bytes);

  return GDK_MEMORY_TEXTURE (result);
}

GBytes *
gdk_memory_texture_get_bytes (GdkMemoryTexture *self,
                              gsize            *out_stride)
{
  *out_stride = self->stride;
  return self->bytes;
}

