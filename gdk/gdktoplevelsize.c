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
 * GdkToplevelSize:
 *
 * Contains information that is useful to compute the size of a toplevel.
 */

G_DEFINE_POINTER_TYPE (GdkToplevelSize, gdk_toplevel_size)

#define UNCONFIGURED_WIDTH 400
#define UNCONFIGURED_HEIGHT 300

#define DEFAULT_BOUNDS_WIDTH INT_MAX
#define DEFAULT_BOUNDS_HEIGHT INT_MAX

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
 * @size: a `GdkToplevelSize`
 * @bounds_width: (out): return location for width
 * @bounds_height: (out): return location for height
 *
 * Retrieves the bounds the toplevel is placed within.
 *
 * The bounds represent the largest size a toplevel may have while still being
 * able to fit within some type of boundary. Depending on the backend, this may
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

  if (size->bounds_width > 0)
    *bounds_width = size->bounds_width;
  else
    *bounds_width = DEFAULT_BOUNDS_WIDTH;

  if (size->bounds_height > 0)
    *bounds_height = size->bounds_height;
  else
    *bounds_height = DEFAULT_BOUNDS_HEIGHT;
}

/**
 * gdk_toplevel_size_get_proposed_size:
 * @size: a `GdkToplevelSize`
 * @width: (out) (optional): proposed width, or 0 if the client may choose
 * @height: (out) (optional): proposed height, or 0 if the client may choose
 *
 * Gets a size proposed by the windowing system.
 *
 * A proposal allows the client to choose a smaller size, for example to
 * preserve an aspect ratio or fit a content grid. A zero dimension leaves
 * that dimension up to the client. Each nonzero dimension is an upper
 * limit for the response.
 *
 * Dimensions are in surface coordinates and include the shadows set with
 * [method@Gdk.ToplevelSize.set_shadow_width]. Set the shadow widths before
 * reading the proposal if they have changed.
 *
 * Use [method@Gdk.ToplevelSize.set_size_for_proposal] to respond. Merely
 * reading a proposal does not change how GDK handles it.
 *
 * A proposal is not provided when the backend cannot negotiate the size,
 * or when the toplevel must fill its configured area, such as a maximized,
 * fullscreen or tiled toplevel. Backends that do not support proposals
 * return false. Currently, proposals are supported by the Wayland backend.
 *
 * Returns: true if at least one dimension is proposed
 *
 * Since: 4.26
 */
gboolean
gdk_toplevel_size_get_proposed_size (GdkToplevelSize *size,
                                     int             *width,
                                     int             *height)
{
  g_return_val_if_fail (size != NULL, FALSE);

  if (width)
    *width = size->proposed_width > 0
           ? MIN ((gint64) size->proposed_width + size->shadow.left + size->shadow.right, G_MAXINT)
           : 0;

  if (height)
    *height = size->proposed_height > 0
            ? MIN ((gint64) size->proposed_height + size->shadow.top + size->shadow.bottom, G_MAXINT)
            : 0;

  return size->proposed_width > 0 || size->proposed_height > 0;
}

/**
 * gdk_toplevel_size_get_size:
 * @size: a `GdkToplevelSize`
 * @width: (out) (optional): preferred width
 * @height: (out) (optional): preferred height
 *
 * Gets the preferred size set by a handler of
 * [signal@Gdk.Toplevel::compute-size].
 *
 * This can be used by a later handler to adjust a size computed by the
 * toolkit. The dimensions include shadows and are in surface coordinates.
 *
 * Since: 4.26
 */
void
gdk_toplevel_size_get_size (GdkToplevelSize *size,
                            int             *width,
                            int             *height)
{
  g_return_if_fail (size != NULL);

  if (width)
    *width = size->width;
  if (height)
    *height = size->height;
}

/**
 * gdk_toplevel_size_get_min_size:
 * @size: a `GdkToplevelSize`
 * @min_width: (out) (optional): minimum width
 * @min_height: (out) (optional): minimum height
 *
 * Gets the minimum size set with [method@Gdk.ToplevelSize.set_min_size].
 *
 * The dimensions include shadows and are in surface coordinates. A later
 * [signal@Gdk.Toplevel::compute-size] handler can use them when adjusting
 * the preferred size. A dimension is zero if no minimum has been set.
 *
 * Since: 4.26
 */
void
gdk_toplevel_size_get_min_size (GdkToplevelSize *size,
                                int             *min_width,
                                int             *min_height)
{
  g_return_if_fail (size != NULL);

  if (min_width)
    *min_width = size->min_width;
  if (min_height)
    *min_height = size->min_height;
}

/**
 * gdk_toplevel_size_set_size_for_proposal:
 * @size: a `GdkToplevelSize`
 * @width: preferred surface width
 * @height: preferred surface height
 *
 * Sets the preferred size in response to a windowing system proposal.
 *
 * Call this only when [method@Gdk.ToplevelSize.get_proposed_size] returns
 * true. Dimensions must be positive and include shadows. The response
 * should fit within each nonzero proposed dimension and respect the
 * minimum size. If these constraints conflict, the proposed upper limit
 * takes precedence.
 *
 * Unlike [method@Gdk.ToplevelSize.set_size], this explicitly requests that
 * GDK use the preferred size to answer the current proposal. The backend
 * enforces the proposed upper limits. Handlers that do not call this
 * method retain the backend's default configure handling.
 *
 * Subsequent calls to [method@Gdk.ToplevelSize.set_size] may further adjust
 * the response during the same [signal@Gdk.Toplevel::compute-size] emission.
 *
 * Since: 4.26
 */
void
gdk_toplevel_size_set_size_for_proposal (GdkToplevelSize *size,
                                         int              width,
                                         int              height)
{
  g_return_if_fail (size != NULL);
  g_return_if_fail (size->proposed_width > 0 || size->proposed_height > 0);
  g_return_if_fail (width > 0);
  g_return_if_fail (height > 0);

  gdk_toplevel_size_set_size (size, width, height);
  size->reply_to_proposal = TRUE;
}

/**
 * gdk_toplevel_size_set_size:
 * @size: a `GdkToplevelSize`
 * @width: the width
 * @height: the height
 *
 * Sets the size the toplevel prefers to be resized to.
 *
 * The size should be within the bounds (see
 * [method@Gdk.ToplevelSize.get_bounds]). The set size should
 * be considered as a hint, and should not be assumed to be
 * respected by the windowing system, or backend.
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
 * @size: a `GdkToplevelSize`
 * @min_width: the minimum width
 * @min_height: the minimum height
 *
 * Sets the minimum size of the toplevel.
 *
 * The minimum size corresponds to the limitations the toplevel can be shrunk
 * to, without resulting in incorrect painting. A user of a `GdkToplevel` should
 * calculate these given both the existing size, and the bounds retrieved from
 * the `GdkToplevelSize` object.
 *
 * The minimum size should be within the bounds (see
 * [method@Gdk.ToplevelSize.get_bounds]).
 */
void
gdk_toplevel_size_set_min_size (GdkToplevelSize *size,
                                int              min_width,
                                int              min_height)
{
  size->min_width = min_width;
  size->min_height = min_height;
}

/**
 * gdk_toplevel_size_set_shadow_width:
 * @size: a `GdkToplevelSize`
 * @left: width of the left part of the shadow
 * @right: width of the right part of the shadow
 * @top: height of the top part of the shadow
 * @bottom: height of the bottom part of the shadow
 *
 * Sets the shadows size of the toplevel.
 *
 * The shadow width corresponds to the part of the computed surface size
 * that would consist of the shadow margin surrounding the window, would
 * there be any.
 * 
 * Shadow width should only be set if
 * [method@Gtk.Display.supports_shadow_width] is %TRUE.
 */
void
gdk_toplevel_size_set_shadow_width (GdkToplevelSize *size,
                                    int              left,
                                    int              right,
                                    int              top,
                                    int              bottom)
{
  size->shadow.is_valid = TRUE;
  size->shadow.left = left;
  size->shadow.right = right;
  size->shadow.top = top;
  size->shadow.bottom = bottom;
}

void
gdk_toplevel_size_validate (GdkToplevelSize *size)
{
#if 0
  int geometry_width, geometry_height;

  geometry_width = size->width;
  geometry_height = size->height;
  if (size->shadow.is_valid)
    {
      geometry_width -= size->shadow.left + size->shadow.right;
      geometry_height -= size->shadow.top + size->shadow.bottom;
    }
#endif
}
