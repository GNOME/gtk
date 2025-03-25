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
  GdkMemoryLayout layout;
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
gdk_memory_texture_download (GdkTexture            *texture,
                             guchar                *data,
                             const GdkMemoryLayout *layout,
                             GdkColorState         *color_state)
{
  GdkMemoryTexture *self = GDK_MEMORY_TEXTURE (texture);

  gdk_memory_convert (data,
                      layout,
                      color_state,
                      (guchar *) g_bytes_get_data (self->bytes, NULL),
                      &self->layout,
                      texture->color_state);
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
gdk_memory_sanitize (GBytes                *bytes,
                     const GdkMemoryLayout *layout,
                     GdkMemoryLayout       *out_layout)
{
  gsize align;
  const guchar *data;
  guchar *copy;

  data = g_bytes_get_data (bytes, NULL);
  align = gdk_memory_format_alignment (layout->format);

  if (GPOINTER_TO_SIZE (data) % align == 0 &&
      gdk_memory_layout_is_aligned (layout, align))
    {
      *out_layout = *layout;
      return bytes;
    }

  gdk_memory_layout_init (out_layout,
                          layout->format,
                          layout->width,
                          layout->height,
                          align);
  
  copy = g_malloc (out_layout->size);
  gdk_memory_copy (copy, out_layout, data, layout);

  g_bytes_unref (bytes);

  return g_bytes_new_take (copy, out_layout->size);
}

GdkTexture *
gdk_memory_texture_new_from_layout (GBytes                *bytes,
                                    const GdkMemoryLayout *layout,
                                    GdkColorState         *color_state,
                                    GdkTexture            *update_texture,
                                    const cairo_region_t  *update_region)
{
  GdkMemoryTexture *self;
  GdkTexture *texture;

  self = g_object_new (GDK_TYPE_MEMORY_TEXTURE,
                       "width", layout->width,
                       "height", layout->height,
                       "color-state", color_state,
                       NULL);
  texture = GDK_TEXTURE (self);

  texture->format = layout->format,
  self->bytes = gdk_memory_sanitize (g_bytes_ref (bytes),
                                     layout,
                                     &self->layout);
  
  if (update_texture)
    {
      if (update_region)
        {
          cairo_region_t *copy_region = cairo_region_copy (update_region);
          cairo_region_intersect_rectangle (copy_region,
                                            &(cairo_rectangle_int_t) {
                                              0, 0,
                                              update_texture->width, update_texture->height
                                            });
          gdk_texture_set_diff (GDK_TEXTURE (self), update_texture, copy_region);
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
  GdkMemoryLayout layout = GDK_MEMORY_LAYOUT_SIMPLE (format, width, height, stride);

  g_return_val_if_fail (bytes != NULL, NULL);
  gdk_memory_layout_return_val_if_invalid (&layout, NULL);
  g_return_val_if_fail (g_bytes_get_size (bytes) >= layout.size, NULL);

  self = g_object_new (GDK_TYPE_MEMORY_TEXTURE,
                       "width", width,
                       "height", height,
                       "color-state", GDK_COLOR_STATE_SRGB,
                       NULL);

  GDK_TEXTURE (self)->format = format;
  self->bytes = gdk_memory_sanitize (g_bytes_ref (bytes), &layout, &self->layout);

  return GDK_TEXTURE (self);
}

GdkTexture *
gdk_memory_texture_new_subtexture (GdkMemoryTexture  *source,
                                   int                x,
                                   int                y,
                                   int                width,
                                   int                height)
{
  GdkMemoryLayout sublayout;
  GdkTexture *texture = GDK_TEXTURE (source);

  g_return_val_if_fail (GDK_IS_MEMORY_TEXTURE (source), NULL);
  g_return_val_if_fail (x >= 0 && x < texture->width, NULL);
  g_return_val_if_fail (y >= 0 && y < texture->height, NULL);
  g_return_val_if_fail (width > 0 && x + width <= texture->width, NULL);
  g_return_val_if_fail (height > 0 && y + height <= texture->height, NULL);

  gdk_memory_layout_init_sublayout (&sublayout,
                                    &source->layout,
                                    &(cairo_rectangle_int_t) { x, y, width, height});

  return gdk_memory_texture_new_from_layout (source->bytes,
                                             &sublayout,
                                             gdk_texture_get_color_state (texture),
                                             NULL,
                                             NULL);
}

GdkMemoryTexture *
gdk_memory_texture_from_texture (GdkTexture *texture)
{
  GdkTexture *result;
  GBytes *bytes;
  guchar *data;
  GdkMemoryLayout layout;

  g_return_val_if_fail (GDK_IS_TEXTURE (texture), NULL);

  if (GDK_IS_MEMORY_TEXTURE (texture))
    return g_object_ref (GDK_MEMORY_TEXTURE (texture));

  gdk_memory_layout_init (&layout,
                          texture->format,
                          texture->width,
                          texture->height,
                          1);
  data = g_malloc (layout.size);

  gdk_texture_do_download (texture, data, &layout, texture->color_state);
  bytes = g_bytes_new_take (data, layout.size);
  result = gdk_memory_texture_new_from_layout (bytes,
                                               &layout,
                                               texture->color_state,
                                               NULL, NULL);
  g_bytes_unref (bytes);

  return GDK_MEMORY_TEXTURE (result);
}

GBytes *
gdk_memory_texture_get_bytes (GdkMemoryTexture *self)
{
  return self->bytes;
}

const GdkMemoryLayout *
gdk_memory_texture_get_layout (GdkMemoryTexture *self)
{
  return &self->layout;
}

