/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2020 Red Hat
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
 *
 */

#include "config.h"

#include "gdktoplevelsizeprivate.h"

/**
 * SECTION:gdktoplevelsize
 * @Title: GdkToplevelSize
 * @Short_description: Information for computing toplevel size
 *
 * The GdkToplevelSIze struct contains information that may be useful
 * for users of GdkToplevel to compute a surface size. It also carries
 * information back with the computational result.
 */

G_DEFINE_POINTER_TYPE (GdkToplevelSize, gdk_toplevel_size)

#define UNCONFIGURED_WIDTH 400
#define UNCONFIGURED_HEIGHT 300

void
gdk_toplevel_size_init (GdkToplevelSize *size,
                        int              bounds_width,
                        int              bounds_height)
{
  *size = (GdkToplevelSize) { 0 };

  size->bounds_width = bounds_width;
  size->bounds_height = bounds_height;

  size->width = UNCONFIGURED_WIDTH;
  size->height = UNCONFIGURED_HEIGHT;
}

/**
 * gdk_toplevel_size_get_bounds:
 * @size: a #GdkToplevelSize
 * @bounds_width: (out): return location for width
 * @bounds_height: (out): return location for height
 *
 * Retrieves the bounds the toplevel is placed within.
 *
 * The bounds represent the largest size a toplevel may have while still being
 * able to fit within some type of boundery. Depending on the backend, this may
 * be equivalent to the dimensions of the work area or the monitor on which the
 * window is being presented on, or something else that limits the way a
 * toplevel can be presented.
 */
void
gdk_toplevel_size_get_bounds (GdkToplevelSize *size,
                              int             *bounds_width,
                              int             *bounds_height)
{
  g_return_if_fail (bounds_width);
  g_return_if_fail (bounds_height);

  *bounds_width = size->bounds_width;
  *bounds_height = size->bounds_height;
}

/**
 * gdk_toplevel_size_set_size:
 * @size: a #GdkToplevelSize
 * @width: the width
 * @height: the height
 *
 * Sets the size the toplevel prefers to be resized to. The size should be
 * within the bounds (see gdk_toplevel_size_get_bounds()). The set size should
 * be considered as a hint, and should not be assumed to be respected by the
 * windowing system, or backend.
 */
void
gdk_toplevel_size_set_size (GdkToplevelSize *size,
                            int              width,
                            int              height)
{
  size->width = width;
  size->height = height;
}

/**
 * gdk_toplevel_size_set_min_size:
 * @size: a #GdkToplevelSize
 * @min_width: the minimum width
 * @min_height: the minimum height
 *
 * The minimum size corresponds to the limitations the toplevel can be shrunk
 * to, without resulting in incorrect painting. A user of a #GdkToplevel should
 * calculate these given both the existing size, and the bounds retrieved from
 * the #GdkToplevelSize object.
 *
 * The minimum size should be within the bounds (see
 * gdk_toplevel_size_get_bounds()).
 */
void
gdk_toplevel_size_set_min_size (GdkToplevelSize *size,
                                int              min_width,
                                int              min_height)
{
  size->min_width = min_width;
  size->min_height = min_height;
}
