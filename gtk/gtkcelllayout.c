/* gtkcelllayout.c
 * Copyright (C) 2003  Kristian Rietveld  <kris@gtk.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gtkcelllayout.h"

GType
gtk_cell_layout_get_type (void)
{
  static GType cell_layout_type = 0;

  if (! cell_layout_type)
    {
      static const GTypeInfo cell_layout_info =
      {
        sizeof (GtkCellLayoutIface),
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        0,
        0,
        NULL
      };

      cell_layout_type =
        g_type_register_static (G_TYPE_INTERFACE, "GtkCellLayout",
                                &cell_layout_info, 0);

      g_type_interface_add_prerequisite (cell_layout_type, G_TYPE_OBJECT);
    }

  return cell_layout_type;
}


void
gtk_cell_layout_pack_start (GtkCellLayout   *cell_layout,
                            GtkCellRenderer *cell,
                            gboolean         expand)
{
  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  (* GTK_CELL_LAYOUT_GET_IFACE (cell_layout)->pack_start) (cell_layout,
                                                           cell,
                                                           expand);
}

void
gtk_cell_layout_pack_end (GtkCellLayout   *cell_layout,
                          GtkCellRenderer *cell,
                          gboolean         expand)
{
  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  (* GTK_CELL_LAYOUT_GET_IFACE (cell_layout)->pack_end) (cell_layout,
                                                         cell,
                                                         expand);
}

void
gtk_cell_layout_clear (GtkCellLayout *cell_layout)
{
  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));

  (* GTK_CELL_LAYOUT_GET_IFACE (cell_layout)->clear) (cell_layout);
}

static void
gtk_cell_layout_set_attributesv (GtkCellLayout   *cell_layout,
                                 GtkCellRenderer *cell,
                                 va_list          args)
{
  gchar *attribute;
  gint column;
  GtkCellLayoutIface *iface;

  attribute = va_arg (args, gchar *);

  iface = GTK_CELL_LAYOUT_GET_IFACE (cell_layout);

  (* iface->clear_attributes) (cell_layout, cell);

  while (attribute != NULL)
    {
      column = va_arg (args, gint);
      (* iface->add_attribute) (cell_layout, cell, attribute, column);
      attribute = va_arg (args, gchar *);
    }
}

void
gtk_cell_layout_set_attributes (GtkCellLayout   *cell_layout,
                                GtkCellRenderer *cell,
                                ...)
{
  va_list args;

  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  va_start (args, cell);
  gtk_cell_layout_set_attributesv (cell_layout, cell, args);
  va_end (args);
}

void
gtk_cell_layout_add_attribute (GtkCellLayout   *cell_layout,
                               GtkCellRenderer *cell,
                               const gchar     *attribute,
                               gint             column)
{
  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));
  g_return_if_fail (attribute != NULL);
  g_return_if_fail (column >= 0);

  (* GTK_CELL_LAYOUT_GET_IFACE (cell_layout)->add_attribute) (cell_layout,
                                                              cell,
                                                              attribute,
                                                              column);
}

void
gtk_cell_layout_set_cell_data_func (GtkCellLayout         *cell_layout,
                                    GtkCellRenderer       *cell,
                                    GtkCellLayoutDataFunc  func,
                                    gpointer               func_data,
                                    GDestroyNotify         destroy)
{
  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  (* GTK_CELL_LAYOUT_GET_IFACE (cell_layout)->set_cell_data_func) (cell_layout,
                                                                   cell,
                                                                   func,
                                                                   func_data,
                                                                   destroy);
}

void
gtk_cell_layout_clear_attributes (GtkCellLayout   *cell_layout,
                                  GtkCellRenderer *cell)
{
  g_return_if_fail (GTK_IS_CELL_LAYOUT (cell_layout));
  g_return_if_fail (GTK_IS_CELL_RENDERER (cell));

  (* GTK_CELL_LAYOUT_GET_IFACE (cell_layout)->clear_attributes) (cell_layout,
                                                                 cell);
}
