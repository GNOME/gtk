/* gtkextendedcell.c
 * Copyright (C) 2010 Openismus GmbH
 *
 * Author:
 *      Tristan Van Berkom <tristan.van.berkom@gmail.com>
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


#include <config.h>
#include "gtkcellrenderer.h"
#include "gtkextendedcell.h"
#include "gtkintl.h"
#include "gtkalias.h"

GType
gtk_extended_cell_get_type (void)
{
  static GType extended_cell_type = 0;

  if (G_UNLIKELY(!extended_cell_type))
    {
      extended_cell_type =
	g_type_register_static_simple (G_TYPE_INTERFACE, I_("GtkExtendedCell"),
				       sizeof (GtkExtendedCellIface),
				       NULL, 0, NULL, 0);

      g_type_interface_add_prerequisite (extended_cell_type, GTK_TYPE_CELL_RENDERER);
    }
  return extended_cell_type;
}

/**
 * gtk_extended_cell_get_desired_size:
 * @cell: a #GtkExtendedCell instance
 * @widget: the #GtkWidget this cell will be rendering to
 * @minimum_size: location for storing the minimum size, or %NULL
 * @natural_size: location for storing the preferred size, or %NULL
 *
 * Retreives a renderer's desired size.
 *
 * Since: 3.0
 */
void
gtk_extended_cell_get_desired_size (GtkExtendedCell   *cell,
				    GtkWidget         *widget,
				    GtkRequisition    *minimum_size,
				    GtkRequisition    *natural_size)
{
  GtkExtendedCellIface *iface;

  g_return_if_fail (GTK_IS_EXTENDED_CELL (cell));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (NULL != minimum_size || NULL != natural_size);

  iface = GTK_EXTENDED_CELL_GET_IFACE (cell);
  iface->get_desired_size (cell, widget, minimum_size, natural_size);
}

#define __GTK_EXTENDED_CELL_C__
#include "gtkaliasdef.c"
