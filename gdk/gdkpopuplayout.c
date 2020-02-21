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

#include "gdkpopuplayout.h"

#include "gdksurface.h"

struct _GdkPopupLayout
{
  /* < private >*/
  grefcount ref_count;

  GdkRectangle anchor_rect;
  GdkGravity rect_anchor;
  GdkGravity surface_anchor;
  GdkAnchorHints anchor_hints;
  int dx;
  int dy;
};

G_DEFINE_BOXED_TYPE (GdkPopupLayout, gdk_popup_layout,
                     gdk_popup_layout_ref,
                     gdk_popup_layout_unref)

/**
 * gdk_popup_layout_new: (constructor)
 * @anchor_rect:  (not nullable): the anchor #GdkRectangle to align @surface with
 * @rect_anchor: the point on @anchor_rect to align with @surface's anchor point
 * @surface_anchor: the point on @surface to align with @rect's anchor point
 *
 * Create a popup layout description. Used together with
 * gdk_surface_present_popup() to describe how a popup surface should be placed
 * and behave on-screen.
 *
 * @anchor_rect is relative to the top-left corner of the surface's parent.
 * @rect_anchor and @surface_anchor determine anchor points on @anchor_rect and
 * surface to pin together.
 *
 * The position of @anchor_rect's anchor point can optionally be offset using
 * gdk_popup_layout_set_offset(), which is equivalent to offsetting the
 * position of surface.
 *
 * Returns: (transfer full): newly created instance of #GdkPopupLayout
 */
GdkPopupLayout *
gdk_popup_layout_new (const GdkRectangle *anchor_rect,
                      GdkGravity          rect_anchor,
                      GdkGravity          surface_anchor)
{
  GdkPopupLayout *layout;

  layout = g_new0 (GdkPopupLayout, 1);
  g_ref_count_init (&layout->ref_count);
  layout->anchor_rect = *anchor_rect;
  layout->rect_anchor = rect_anchor;
  layout->surface_anchor = surface_anchor;

  return layout;
}

/**
 * gdk_popup_layout_ref:
 * @layout: a #GdkPopupLayout
 *
 * Increases the reference count of @value.
 *
 * Returns: the same @layout
 */
GdkPopupLayout *
gdk_popup_layout_ref (GdkPopupLayout *layout)
{
  g_ref_count_inc (&layout->ref_count);
  return layout;
}

/**
 * gdk_popup_layout_unref:
 * @layout: a #GdkPopupLayout
 *
 * Decreases the reference count of @value.
 */
void
gdk_popup_layout_unref (GdkPopupLayout *layout)
{
  if (g_ref_count_dec (&layout->ref_count))
    g_free (layout);
}

/**
 * gdk_popup_layout_copy:
 * @layout: a #GdkPopupLayout
 *
 * Create a new #GdkPopupLayout and copy the contents of @layout into it.
 *
 * Returns: (transfer full): a copy of @layout.
 */
GdkPopupLayout *
gdk_popup_layout_copy (GdkPopupLayout *layout)
{
  GdkPopupLayout *new_layout;

  new_layout = g_new0 (GdkPopupLayout, 1);
  g_ref_count_init (&new_layout->ref_count);

  new_layout->anchor_rect = layout->anchor_rect;
  new_layout->rect_anchor = layout->rect_anchor;
  new_layout->surface_anchor = layout->surface_anchor;
  new_layout->anchor_hints = layout->anchor_hints;
  new_layout->dx = layout->dx;
  new_layout->dy = layout->dy;

  return new_layout;
}

/**
 * gdk_popup_layout_set_anchor_rect:
 * @layout: a #GdkPopupLayout
 * @anchor_rect: the new anchor rectangle
 *
 * Set the anchor rectangle.
 */
void
gdk_popup_layout_set_anchor_rect (GdkPopupLayout     *layout,
                                  const GdkRectangle *anchor_rect)
{
  layout->anchor_rect = *anchor_rect;
}

/**
 * gdk_popup_layout_get_anchor_rect:
 * @layout: a #GdkPopupLayout
 *
 * Get the anchor rectangle.
 *
 * Returns: The anchor rectangle.
 */
const GdkRectangle *
gdk_popup_layout_get_anchor_rect (GdkPopupLayout *layout)
{
  return &layout->anchor_rect;
}

/**
 * gdk_popup_layout_set_rect_anchor:
 * @layout: a #GdkPopupLayout
 * @anchor: the new rect anchor
 *
 * Set the anchor on the anchor rectangle.
 */
void
gdk_popup_layout_set_rect_anchor (GdkPopupLayout *layout,
                                  GdkGravity      anchor)
{
  layout->rect_anchor = anchor;
}

/**
 * gdk_popup_layout_get_rect_anchor:
 * @layout: a #GdkPopupLayout
 *
 * Returns: the anchor on the anchor rectangle.
 */
GdkGravity
gdk_popup_layout_get_rect_anchor (GdkPopupLayout *layout)
{
  return layout->rect_anchor;
}

/**
 * gdk_popup_layout_set_surface_anchor:
 * @layout: a #GdkPopupLayout
 * @anchor: the new popup surface anchor
 *
 * Set the anchor on the popup surface.
 */
void
gdk_popup_layout_set_surface_anchor (GdkPopupLayout *layout,
                                     GdkGravity      anchor)
{
  layout->surface_anchor = anchor;
}

/**
 * gdk_popup_layout_get_surface_anchor:
 * @layout: a #GdkPopupLayout
 *
 * Returns: the anchor on the popup surface.
 */
GdkGravity
gdk_popup_layout_get_surface_anchor (GdkPopupLayout *layout)
{
  return layout->surface_anchor;
}

/**
 * gdk_popup_layout_set_anchor_hints:
 * @layout: a #GdkPopupLayout
 * @anchor_hints: the new #GdkAnchorHints
 *
 * Set new anchor hints.
 *
 * The set @anchor_hints determines how @surface will be moved if the anchor
 * points cause it to move off-screen. For example, %GDK_ANCHOR_FLIP_X will
 * replace %GDK_GRAVITY_NORTH_WEST with %GDK_GRAVITY_NORTH_EAST and vice versa
 * if @surface extends beyond the left or right edges of the monitor.
 */
void
gdk_popup_layout_set_anchor_hints (GdkPopupLayout *layout,
                                   GdkAnchorHints  anchor_hints)
{
  layout->anchor_hints = anchor_hints;
}

/**
 * gdk_popup_layout_get_anchor_hints:
 * @layout: a #GdkPopupLayout
 *
 * Get the #GdkAnchorHints.
 *
 * Returns: the #GdkAnchorHints.
 */
GdkAnchorHints
gdk_popup_layout_get_anchor_hints (GdkPopupLayout *layout)
{
  return layout->anchor_hints;
}

/**
 * gdk_popup_layout_set_offset:
 * @layout: a #GdkPopupLayout
 * @dx: x delta to offset the anchor rectangle with
 * @dy: y delta to offset the anchor rectangle with
 *
 * Offset the position of the anchor rectangle with the given delta.
 */
void
gdk_popup_layout_set_offset (GdkPopupLayout *layout,
                             int             dx,
                             int             dy)
{
  layout->dx = dx;
  layout->dy = dy;
}

/**
 * gdk_popup_layout_get_offset:
 * @layout: a #GdkPopupLayout
 * @dx: a pointer to where to store the delta x coordinate
 * @dy: a pointer to where to store the delta y coordinate
 *
 * Get the delta the anchor rectangle is offset with
 */
void
gdk_popup_layout_get_offset (GdkPopupLayout *layout,
                             int            *dx,
                             int            *dy)
{
  if (dx)
    *dx = layout->dx;
  if (dy)
    *dy = layout->dy;
}
