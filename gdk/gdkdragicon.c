/*
 * Copyright © 2020 Red Hat, Inc.
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gdkintl.h"
#include "gdkdragiconprivate.h"
#include "gdk-private.h"

/**
 * SECTION:gdkdragicon
 * @Short_description: Interface for drag icon surfaces
 * @Title: Drag icons
 *
 * A #GdkDragIcon is a surface that is used during a
 * DND operation.
 */


/* FIXME: this can't have GdkSurface as a prerequisite
 * as long as GdkSurface implements this interface itself
 */
G_DEFINE_INTERFACE (GdkDragIcon, gdk_drag_icon, G_TYPE_OBJECT)

static gboolean
gdk_drag_icon_default_present (GdkDragIcon *drag_icon,
                               int          width,
                               int          height)
{
  return FALSE;
}

static void
gdk_drag_icon_default_init (GdkDragIconInterface *iface)
{
  iface->present = gdk_drag_icon_default_present;
}

/**
 * gdk_drag_icon_present:
 * @drag_icon: the #GdkDragIcon to show
 * @width: the unconstrained drag_icon width to layout
 * @height: the unconstrained drag_icon height to layout
 *
 * Present @drag_icon.
 *
 * Returns: %FALSE if it failed to be presented, otherwise %TRUE.
 */
gboolean
gdk_drag_icon_present (GdkDragIcon *drag_icon,
                       int          width,
                       int          height)
{
  g_return_val_if_fail (GDK_IS_DRAG_ICON (drag_icon), FALSE);
  g_return_val_if_fail (width > 0, FALSE);
  g_return_val_if_fail (height > 0, FALSE);

  return GDK_DRAG_ICON_GET_IFACE (drag_icon)->present (drag_icon, width, height);
}
