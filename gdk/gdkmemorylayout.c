/*
 * Copyright © 2025 Benjamin Otte
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

#include "gdkmemorylayoutprivate.h"

#include "gdkmemoryformatprivate.h"

/*<private>
 * gdk_memory_layout_init:
 * @self: layout to initialize
 * @format: a format
 * @width: width to compute layout for
 * @height: height to compute layout for
 * @align: alignment to guarantee for stride. Must be power of 2.
 *   Use 1 if you don't care.
 *
 * Initializes a layout for the given arguments. The layout can
 * then be used to allocate and then copy data into that memory.
 **/
void
gdk_memory_layout_init (GdkMemoryLayout *self,
                        GdkMemoryFormat  format,
                        gsize            width,
                        gsize            height,
                        gsize            align)
{
  if (gdk_memory_layout_try_init (self, format, width, height, align))
    return;

  g_assert_not_reached ();
}

/*<private>
 * gdk_memory_layout_try_init:
 * @self: layout to initialize
 * @format: a format
 * @width: width to compute layout for
 * @height: height to compute layout for
 * @align: alignment to guarantee for stride. Must be power of 2.
 *   Use 1 if you don't care.
 *
 * Initializes a layout for the given arguments. The layout can
 * then be used to allocate and then copy data into that memory.
 *
 * It might not be possible to initialize a layout, for example
 * when the size is too large or when it is not a multiple of the
 * given format's block size. In that case %FALSE will be returned
 * and the layout won't be initialized.
 *
 * Returns: TRUE if the layout could be initialized
 **/
gboolean
gdk_memory_layout_try_init (GdkMemoryLayout *self,
                            GdkMemoryFormat  format,
                            gsize            width,
                            gsize            height,
                            gsize            align)
{
  gsize plane, n_planes, size;

  g_assert (align > 0);

  if (!gdk_memory_format_is_block_boundary (format, width, height))
    return FALSE;

  n_planes = gdk_memory_format_get_n_planes (format);

  self->format = format;
  self->width = width;
  self->height = height;

  size = 0;
  for (plane = 0; plane < n_planes; plane++)
    {
      gsize block_width, block_height, block_bytes, plane_width, plane_height;
      gsize plane_size;

      block_width = gdk_memory_format_get_plane_block_width (format, plane);
      block_height = gdk_memory_format_get_plane_block_height (format, plane);
      block_bytes = gdk_memory_format_get_plane_block_bytes (format, plane);

      g_assert (width % block_width == 0);
      g_assert (height % block_height == 0);
      plane_width = width / block_width;
      plane_height = height / block_height;

      if (!g_size_checked_mul (&plane_size, plane_width, block_bytes))
        return FALSE;
      /* round up to alignment using fancy checked math */
      if (!g_size_checked_add (&plane_size, plane_size, (align - plane_size % align) % align))
        return FALSE;

      self->planes[plane].stride = plane_size;
      self->planes[plane].offset = size;
      if (!g_size_checked_mul (&plane_size, plane_size, plane_height))
        return FALSE;

      if (!g_size_checked_add (&size, size, plane_size))
        return FALSE;
    }
  self->size = size;

  return TRUE;
}

/**
 * gdk_memory_layout_init_sublayout:
 * @self: layout to initialize
 * @other: layout to initialize from
 * @area: rectangle inside `other`
 *
 * Initializes a new memory layout for the given subarea of an existing layout.
 * 
 * The area bounds must be aligned to the block size.
 *
 * Keep in mind that this only adjusts the offsets, it doesn't shrink the
 * size from the original layout.
 **/
void
gdk_memory_layout_init_sublayout (GdkMemoryLayout             *self,
                                  const GdkMemoryLayout       *other,
                                  const cairo_rectangle_int_t *area)
{
  gsize plane, n_planes;

  g_assert (area->x + area->width <= other->width);
  g_assert (area->y + area->height <= other->height);
  g_assert (gdk_memory_format_is_block_boundary (other->format, area->x, area->y));
  g_assert (gdk_memory_format_is_block_boundary (other->format, area->width, area->height));

  n_planes = gdk_memory_format_get_n_planes (other->format);

  self->format = other->format;
  self->width = area->width;
  self->height = area->height;

  for (plane = 0; plane < n_planes; plane++)
    {
      gsize block_width, block_height, block_bytes;

      block_width = gdk_memory_format_get_plane_block_width (other->format, plane);
      block_height = gdk_memory_format_get_plane_block_height (other->format, plane);
      block_bytes = gdk_memory_format_get_plane_block_bytes (other->format, plane);

      self->planes[plane].offset = other->planes[plane].offset +
                                   area->y / block_height * other->planes[plane].stride +
                                   area->x / block_width * block_bytes;
      self->planes[plane].stride = other->planes[plane].stride;
    }

  self->size = other->size;
}

gboolean
gdk_memory_layout_is_valid (const GdkMemoryLayout  *self,
                            GError                **error)
{
  gsize p, needed_size;

  if (self->format >= GDK_MEMORY_N_FORMATS)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "invalid format given");
      return FALSE;
    }

  if (self->width <= 0 || self->height <= 0)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "image size %zux%zu is invalid", self->width, self->height);
      return FALSE;
    }

  if (!gdk_memory_format_is_block_boundary (self->format, self->width, self->height))
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "image size %zux%zu is not a multiple of the block size %zux%zu",
                   self->width, self->height,
                   gdk_memory_format_get_block_width (self->format),
                   gdk_memory_format_get_block_height (self->format));
      return FALSE;
    }

  needed_size = 0;
  for (p = 0; p < gdk_memory_format_get_n_planes (self->format); p++)
    {
      gsize block_width, block_height, block_bytes, plane_size;

      block_width = gdk_memory_format_get_plane_block_width (self->format, p);
      block_height = gdk_memory_format_get_plane_block_height (self->format, p);
      block_bytes = gdk_memory_format_get_plane_block_bytes (self->format, p);

      if (self->planes[p].offset < needed_size)
        {
          g_set_error (error,
                       G_IO_ERROR, G_IO_ERROR_FAILED,
                       "offset for plane %zu is %zu which overlaps previous plane going up to offset %zu",
                       p, self->planes[p].offset, needed_size);
          return FALSE;
        }

      if (!g_size_checked_mul (&plane_size, self->width / block_width, block_bytes))
        {
          g_set_error (error,
                       G_IO_ERROR, G_IO_ERROR_FAILED,
                       "stride for plane %zu is %zu bytes, but image width %zu would overflow the stride requirement",
                       p, self->planes[p].stride, self->width);
          return FALSE;
        }
      if (plane_size > self->planes[p].stride)
        {
          g_set_error (error,
                       G_IO_ERROR, G_IO_ERROR_FAILED,
                       "stride for plane %zu is %zu bytes, but image width %zu requires a stride of %zu bytes",
                       p, self->planes[p].stride, self->width, plane_size);
          return FALSE;
        }

      if (!g_size_checked_mul (&plane_size, self->planes[p].stride, (self->height - 1) / block_height) ||
          !g_size_checked_add (&plane_size, plane_size, self->width / block_width * block_bytes))
        {
          g_set_error (error,
                       G_IO_ERROR, G_IO_ERROR_FAILED,
                       "size for plane %zu would overflow, image size %zux%zu with stride of %zu bytes is too large",
                       p, self->width, self->height, self->planes[p].stride);
          return FALSE;
        }
      if (!g_size_checked_add (&needed_size, self->planes[p].offset, plane_size))
        {
          g_set_error (error,
                       G_IO_ERROR, G_IO_ERROR_FAILED,
                       "size for plane %zu of %zu bytes at offset %zu does overflow",
                       p, plane_size, self->planes[p].offset);
          return FALSE;
        }
    }

  if (needed_size > self->size)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   "image size of %zu bytes is too small, at least %zu bytes are needed",
                   self->size, needed_size);
      return FALSE;
    }

  return TRUE;
}

gboolean
gdk_memory_layout_is_aligned (const GdkMemoryLayout *self,
                              gsize                  align)
{
  gsize i;

  align = MAX (align, gdk_memory_format_alignment (self->format));

  if (self->size % align)
    return FALSE;

  for (i = 0; i < gdk_memory_format_get_n_planes (self->format); i++)
    {
      if (self->planes[i].offset % align ||
          self->planes[i].stride % align)
        return FALSE;
    }

  return TRUE;
}

/* This is just meant to be a good enough check for assertions, not a guaranteed
 * check you can rely on.
 * It's meant to check accidental overlap during copies between layouts.
 */
gboolean
gdk_memory_layout_has_overlap (const guchar          *data1,
                               const GdkMemoryLayout *layout1,
                               const guchar          *data2,
                               const GdkMemoryLayout *layout2)
{
  /* really, these are pointers, but we wanna do math on them */
  gsize start1, end1, start2, end2, shift;

  /* We only check one plane for now */
  start1 = GPOINTER_TO_SIZE (data1 + gdk_memory_layout_offset (layout1, 0, 0, 0));
  start2 = GPOINTER_TO_SIZE (data2 + gdk_memory_layout_offset (layout2, 0, 0, 0));
  end1 = GPOINTER_TO_SIZE (data1 + gdk_memory_layout_offset (layout1, 0, layout1->width, layout1->height - 1));
  end2 = GPOINTER_TO_SIZE (data2 + gdk_memory_layout_offset (layout2, 0, layout2->width, layout2->height - 1));
  if (end2 <= start1 || end1 <= start2)
    return FALSE;

  /* This can't happen with subimages of the same large image, so something
   * is probably screwed up. */
  if (layout1->planes[0].stride != layout2->planes[0].stride)
    return TRUE;

  end1 = GPOINTER_TO_SIZE (data1 + gdk_memory_layout_offset (layout1, 0, layout1->width, 0));
  end2 = GPOINTER_TO_SIZE (data2 + gdk_memory_layout_offset (layout2, 0, layout2->width, 0));
  shift = start1 % layout1->planes[0].stride;
  if ((end1 - shift) % layout1->planes[0].stride < (start2 - shift) % layout2->planes[0].stride)
    return FALSE;

  return TRUE;
}

/**
 * gdk_memory_layout_offset:
 * @layout: a layout
 * @plane: the plane to get the offset for
 * @x: X coordinate to query the offset for. Must be a multiple of the
 *   block size
 * @y: Y coordinate to query the offset for. Must be a multiple of the
 *   block size
 *
 * Queries the byte offset to a block of data.
 *
 * You can set x == 0 to query the offset of a row.
 * You can also set y == 0 to query the offset of a plane.
 *
 * Returns: the offset in bytes to the location of the block at the
 *   given plane and coordinate.
 **/
gsize
gdk_memory_layout_offset (const GdkMemoryLayout *layout,
                          gsize                  plane,
                          gsize                  x,
                          gsize                  y)
{
  gsize block_width, block_height, block_bytes;

  block_width = gdk_memory_format_get_plane_block_width (layout->format, plane);
  block_height = gdk_memory_format_get_plane_block_height (layout->format, plane);
  block_bytes = gdk_memory_format_get_plane_block_bytes (layout->format, plane);

  g_assert (x % block_width == 0);
  g_assert (y % block_height == 0);

  return layout->planes[plane].offset +
         y / block_height * layout->planes[plane].stride +
         x / block_width * block_bytes;
}

/*<private>
 * gdk_memory_copy:
 * @dest_data: the data to copy into
 * @dest_layout: the layout of `dest_data`
 * @src_data: the data to copy from
 * @src_layout: the layout of `src_data`
 *
 * Copies the source into the destination.
 *
 * The source and the destination must have the same format.
 * 
 * The source and destination must have the same size.
 * You can use gdk_memory_layout_init_sublayout() to adjust sizes
 * before calling this function.
 **/
void
gdk_memory_copy (guchar                      *dest_data,
                 const GdkMemoryLayout       *dest_layout,
                 const guchar                *src_data,
                 const GdkMemoryLayout       *src_layout)
{
  gsize n_planes, plane;
  GdkMemoryFormat format;

  g_assert (dest_layout->format == src_layout->format);
  g_assert (dest_layout->width == src_layout->width);
  g_assert (dest_layout->height == src_layout->height);

  format = src_layout->format;
  n_planes = gdk_memory_format_get_n_planes (format);

  for (plane = 0; plane < n_planes; plane++)
    {
      gsize block_width, block_height, block_bytes, plane_width, plane_height;
      guchar *dest;
      const guchar *src;

      block_width = gdk_memory_format_get_plane_block_width (format, plane);
      block_height = gdk_memory_format_get_plane_block_height (format, plane);
      block_bytes = gdk_memory_format_get_plane_block_bytes (format, plane);

      plane_width = src_layout->width / block_width;
      plane_height = src_layout->height / block_height;

      dest = dest_data + dest_layout->planes[plane].offset;
      src = src_data + src_layout->planes[plane].offset;

      if (dest_layout->planes[plane].stride == src_layout->planes[plane].stride &&
          dest_layout->planes[plane].stride == plane_width * block_bytes)
        {
          memcpy (dest, src, dest_layout->planes[plane].stride * plane_height);
        }
      else
        {
          gsize y;

          for (y = 0; y < plane_height; y++)
            {
              memcpy (dest + y * dest_layout->planes[plane].stride,
                      src + y * src_layout->planes[plane].stride,
                      plane_width * block_bytes);
            }
        }
    }
}

