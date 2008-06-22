/* GAIL - The GNOME Accessibility Implementation Library

 * Copyright 2001 Sun Microsystems Inc.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <gtk/gtk.h>
#include "gailcellparent.h"

GType
gail_cell_parent_get_type (void)
{
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      GType g_define_type_id =
        g_type_register_static_simple (G_TYPE_INTERFACE,
                                       "GailCellParent",
                                       sizeof (GailCellParentIface),
                                       NULL,
                                       0,
                                       NULL,
                                       0);

      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
}

/**
 * gail_cell_parent_get_cell_extents:
 * @parent: a #GObject instance that implements GailCellParentIface
 * @cell: a #GailCell whose extents is required
 * @x: address of #gint to put x coordinate
 * @y: address of #gint to put y coordinate
 * @width: address of #gint to put width
 * @height: address of #gint to put height
 * @coord_type: specifies whether the coordinates are relative to the screen
 * or to the components top level window
 *
 * Gets the rectangle which gives the extent of the @cell.
 *
 **/
void
gail_cell_parent_get_cell_extents (GailCellParent *parent,
                                   GailCell       *cell,
                                   gint           *x,
                                   gint           *y,
                                   gint           *width,
                                   gint           *height,
                                   AtkCoordType   coord_type)
{
  GailCellParentIface *iface;

  g_return_if_fail (GAIL_IS_CELL_PARENT (parent));

  iface = GAIL_CELL_PARENT_GET_IFACE (parent);

  if (iface->get_cell_extents)
    (iface->get_cell_extents) (parent, cell, x, y, width, height, coord_type);
}

/**
 * gail_cell_parent_get_cell_area:
 * @parent: a #GObject instance that implements GailCellParentIface
 * @cell: a #GailCell whose area is required
 * @cell_rect: address of #GdkRectangle to put the cell area
 *
 * Gets the cell area of the @cell.
 *
 **/
void
gail_cell_parent_get_cell_area (GailCellParent *parent,
                                GailCell       *cell,
                                GdkRectangle   *cell_rect)
{
  GailCellParentIface *iface;

  g_return_if_fail (GAIL_IS_CELL_PARENT (parent));
  g_return_if_fail (cell_rect);

  iface = GAIL_CELL_PARENT_GET_IFACE (parent);

  if (iface->get_cell_area)
    (iface->get_cell_area) (parent, cell, cell_rect);
}
/**
 * gail_cell_parent_grab_focus:
 * @parent: a #GObject instance that implements GailCellParentIface
 * @cell: a #GailCell whose area is required
 *
 * Puts focus in the specified cell.
 *
 **/
gboolean
gail_cell_parent_grab_focus (GailCellParent *parent,
                             GailCell       *cell)
{
  GailCellParentIface *iface;

  g_return_val_if_fail (GAIL_IS_CELL_PARENT (parent), FALSE);

  iface = GAIL_CELL_PARENT_GET_IFACE (parent);

  if (iface->grab_focus)
    return (iface->grab_focus) (parent, cell);
  else
    return FALSE;
}
