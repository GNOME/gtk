/* GTK+ - accessibility implementations
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gtk/gtk.h>
#include "gtkcellaccessibleparent.h"

GType
gtk_cell_accessible_parent_get_type (void)
{
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
    {
      GType g_define_type_id =
        g_type_register_static_simple (G_TYPE_INTERFACE,
                                       "GtkCellAccessibleParent",
                                       sizeof (GtkCellAccessibleParentIface),
                                       NULL,
                                       0,
                                       NULL,
                                       0);

      g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
    }

  return g_define_type_id__volatile;
}

void
gtk_cell_accessible_parent_get_cell_extents (GtkCellAccessibleParent *parent,
                                             GtkCellAccessible       *cell,
                                             gint                    *x,
                                             gint                    *y,
                                             gint                    *width,
                                             gint                    *height,
                                             AtkCoordType             coord_type)
{
  GtkCellAccessibleParentIface *iface;

  g_return_if_fail (GTK_IS_CELL_ACCESSIBLE_PARENT (parent));

  iface = GTK_CELL_ACCESSIBLE_PARENT_GET_IFACE (parent);

  if (iface->get_cell_extents)
    (iface->get_cell_extents) (parent, cell, x, y, width, height, coord_type);
}

void
gtk_cell_accessible_parent_get_cell_area (GtkCellAccessibleParent *parent,
                                          GtkCellAccessible       *cell,
                                          GdkRectangle            *cell_rect)
{
  GtkCellAccessibleParentIface *iface;

  g_return_if_fail (GTK_IS_CELL_ACCESSIBLE_PARENT (parent));
  g_return_if_fail (cell_rect);

  iface = GTK_CELL_ACCESSIBLE_PARENT_GET_IFACE (parent);

  if (iface->get_cell_area)
    (iface->get_cell_area) (parent, cell, cell_rect);
}

gboolean
gtk_cell_accessible_parent_grab_focus (GtkCellAccessibleParent *parent,
                                       GtkCellAccessible       *cell)
{
  GtkCellAccessibleParentIface *iface;

  g_return_val_if_fail (GTK_IS_CELL_ACCESSIBLE_PARENT (parent), FALSE);

  iface = GTK_CELL_ACCESSIBLE_PARENT_GET_IFACE (parent);

  if (iface->grab_focus)
    return (iface->grab_focus) (parent, cell);
  else
    return FALSE;
}

int
gtk_cell_accessible_parent_get_child_index (GtkCellAccessibleParent *parent,
                                            GtkCellAccessible       *cell)
{
  GtkCellAccessibleParentIface *iface;

  g_return_val_if_fail (GTK_IS_CELL_ACCESSIBLE_PARENT (parent), FALSE);

  iface = GTK_CELL_ACCESSIBLE_PARENT_GET_IFACE (parent);

  if (iface->get_child_index)
    return (iface->get_child_index) (parent, cell);
  else
    return -1;
}

GtkCellRendererState
gtk_cell_accessible_parent_get_renderer_state (GtkCellAccessibleParent *parent,
                                               GtkCellAccessible       *cell)
{
  GtkCellAccessibleParentIface *iface;

  g_return_val_if_fail (GTK_IS_CELL_ACCESSIBLE_PARENT (parent), 0);
  g_return_val_if_fail (GTK_IS_CELL_ACCESSIBLE (cell), 0);

  iface = GTK_CELL_ACCESSIBLE_PARENT_GET_IFACE (parent);

  if (iface->get_renderer_state)
    return (iface->get_renderer_state) (parent, cell);
  else
    return 0;
}

void
gtk_cell_accessible_parent_expand_collapse (GtkCellAccessibleParent *parent,
                                            GtkCellAccessible       *cell)
{
  GtkCellAccessibleParentIface *iface;

  g_return_if_fail (GTK_IS_CELL_ACCESSIBLE_PARENT (parent));
  g_return_if_fail (GTK_IS_CELL_ACCESSIBLE (cell));

  iface = GTK_CELL_ACCESSIBLE_PARENT_GET_IFACE (parent);

  if (iface->expand_collapse)
    (iface->expand_collapse) (parent, cell);
}

void
gtk_cell_accessible_parent_activate (GtkCellAccessibleParent *parent,
                                     GtkCellAccessible       *cell)
{
  GtkCellAccessibleParentIface *iface;

  g_return_if_fail (GTK_IS_CELL_ACCESSIBLE_PARENT (parent));
  g_return_if_fail (GTK_IS_CELL_ACCESSIBLE (cell));

  iface = GTK_CELL_ACCESSIBLE_PARENT_GET_IFACE (parent);

  if (iface->activate)
    (iface->activate) (parent, cell);
}

void
gtk_cell_accessible_parent_edit (GtkCellAccessibleParent *parent,
                                 GtkCellAccessible       *cell)
{
  GtkCellAccessibleParentIface *iface;

  g_return_if_fail (GTK_IS_CELL_ACCESSIBLE_PARENT (parent));
  g_return_if_fail (GTK_IS_CELL_ACCESSIBLE (cell));

  iface = GTK_CELL_ACCESSIBLE_PARENT_GET_IFACE (parent);

  if (iface->edit)
    (iface->edit) (parent, cell);
}

void
gtk_cell_accessible_parent_update_relationset (GtkCellAccessibleParent *parent,
                                               GtkCellAccessible       *cell,
                                               AtkRelationSet          *relationset)
{
  GtkCellAccessibleParentIface *iface;

  g_return_if_fail (GTK_IS_CELL_ACCESSIBLE_PARENT (parent));
  g_return_if_fail (GTK_IS_CELL_ACCESSIBLE (cell));
  g_return_if_fail (ATK_IS_RELATION_SET (relationset));

  iface = GTK_CELL_ACCESSIBLE_PARENT_GET_IFACE (parent);

  if (iface->update_relationset)
    (iface->update_relationset) (parent, cell, relationset);
}
