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

static inline gsize
round_up (gsize number, gsize divisor)
{
  return (number + divisor - 1) / divisor * divisor;
}

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
  gsize plane, n_planes, size;

  g_assert (align > 0);
  g_assert (gdk_memory_format_is_block_boundary (format, width, height));

  n_planes = gdk_memory_format_get_n_planes (format);

  self->format = format;
  self->width = width;
  self->height = height;

  size = 0;
  for (plane = 0; plane < n_planes; plane++)
    {
      gsize block_width, block_height, block_bytes, plane_width, plane_height;

      block_width = gdk_memory_format_get_plane_block_width (format, plane);
      block_height = gdk_memory_format_get_plane_block_height (format, plane);
      block_bytes = gdk_memory_format_get_plane_block_bytes (format, plane);

      g_assert (width % block_width == 0);
      g_assert (height % block_height == 0);
      plane_width = width / block_width;
      plane_height = height / block_height;

      self->planes[plane].offset = size;
      self->planes[plane].stride = round_up (plane_width * block_bytes, align);
      size += self->planes[plane].stride * plane_height;
    }
  self->size = size;
}

