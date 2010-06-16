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


#define DEBUG_EXTENDED_CELL 0

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
 * gtk_extended_cell_is_height_for_width:
 * @cell: a #GtkExtendedCell instance
 *
 * Gets whether the cell renderer prefers a height-for-width layout
 * or a width-for-height layout.
 *
 * Returns: %TRUE if the cell prefers height-for-width, %FALSE if
 * the cell should be treated with a width-for-height preference.
 *
 * Since: 3.0
 */
gboolean
gtk_extended_cell_is_height_for_width (GtkExtendedCell *cell)
{
  GtkExtendedCellIface *iface;

  g_return_val_if_fail (GTK_IS_EXTENDED_CELL (cell), FALSE);

  iface = GTK_EXTENDED_CELL_GET_IFACE (cell);
  if (iface->is_height_for_width)
    return iface->is_height_for_width (cell);

  /* By default cell renderers are height-for-width. */
  return TRUE;
}

/**
 * gtk_extended_cell_get_desired_width:
 * @cell: a #GtkExtendedCell instance
 * @widget: the #GtkWidget this cell will be rendering to
 * @minimum_size: location to store the minimum size, or %NULL
 * @natural_size: location to store the natural size, or %NULL
 *
 * Retreives a renderer's desired size when rendered to @widget.
 *
 * Since: 3.0
 */
void
gtk_extended_cell_get_desired_width (GtkExtendedCell   *cell,
				     GtkWidget         *widget,
				     gint              *minimum_size,
				     gint              *natural_size)
{
  GtkExtendedCellIface *iface;

  g_return_if_fail (GTK_IS_EXTENDED_CELL (cell));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (NULL != minimum_size || NULL != natural_size);

  iface = GTK_EXTENDED_CELL_GET_IFACE (cell);
  iface->get_desired_width (cell, widget, minimum_size, natural_size);

#if DEBUG_EXTENDED_CELL
  g_message ("%s returning minimum width: %d and natural width: %d",
	     G_OBJECT_TYPE_NAME (cell), 
	     minimum_size ? *minimum_size : 20000, 
	     natural_size ? *natural_size : 20000);
#endif
}


/**
 * gtk_extended_cell_get_desired_height:
 * @cell: a #GtkExtendedCell instance
 * @widget: the #GtkWidget this cell will be rendering to
 * @minimum_size: location to store the minimum size, or %NULL
 * @natural_size: location to store the natural size, or %NULL
 *
 * Retreives a renderer's desired size when rendered to @widget.
 *
 * Since: 3.0
 */
void
gtk_extended_cell_get_desired_height (GtkExtendedCell   *cell,
				      GtkWidget         *widget,
				      gint              *minimum_size,
				      gint              *natural_size)
{
  GtkExtendedCellIface *iface;

  g_return_if_fail (GTK_IS_EXTENDED_CELL (cell));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (NULL != minimum_size || NULL != natural_size);

  iface = GTK_EXTENDED_CELL_GET_IFACE (cell);
  iface->get_desired_height (cell, widget, minimum_size, natural_size);

#if DEBUG_EXTENDED_CELL
  g_message ("%s returning minimum height: %d and natural height: %d",
	     G_OBJECT_TYPE_NAME (cell), 
	     minimum_size ? *minimum_size : 20000, 
	     natural_size ? *natural_size : 20000);
#endif
}


/**
 * gtk_extended_cell_get_width_for_height:
 * @cell: a #GtkExtendedCell instance
 * @height: the size which is available for allocation
 * @minimum_width: location for storing the minimum size, or %NULL
 * @natural_width: location for storing the preferred size, or %NULL
 *
 * Retreives a cell renderers's desired width if it were rendered to 
 * @widget with the specified @height.
 *
 * Since: 3.0
 */
void
gtk_extended_cell_get_width_for_height (GtkExtendedCell *cell,
					GtkWidget       *widget,
					gint             height,
					gint            *minimum_width,
					gint            *natural_width)
{
  GtkExtendedCellIface *iface;

  g_return_if_fail (GTK_IS_EXTENDED_CELL (cell));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (NULL != minimum_width || NULL != natural_width);

  iface = GTK_EXTENDED_CELL_GET_IFACE (cell);
  iface->get_width_for_height (cell, widget, height, minimum_width, natural_width);

#if DEBUG_EXTENDED_CELL
  g_message ("%s width for height: %d is minimum %d and natural: %d",
	     G_OBJECT_TYPE_NAME (cell), height,
	     minimum_width ? *minimum_width : 20000, 
	     natural_width ? *natural_width : 20000);
#endif
}

/**
 * gtk_extended_cell_get_height_for_width:
 * @cell: a #GtkExtendedCell instance
 * @width: the size which is available for allocation
 * @minimum_height: location for storing the minimum size, or %NULL
 * @natural_height: location for storing the preferred size, or %NULL
 *
 * Retreives a cell renderers's desired height if it were rendered to 
 * @widget with the specified @width.
 *
 * Since: 3.0
 */
void
gtk_extended_cell_get_height_for_width (GtkExtendedCell *cell,
					GtkWidget       *widget,
					gint             width,
					gint            *minimum_height,
					gint            *natural_height)
{
  GtkExtendedCellIface *iface;

  g_return_if_fail (GTK_IS_EXTENDED_CELL (cell));
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (NULL != minimum_height || NULL != natural_height);

  iface = GTK_EXTENDED_CELL_GET_IFACE (cell);
  iface->get_height_for_width (cell, widget, width, minimum_height, natural_height);

#if DEBUG_EXTENDED_CELL
  g_message ("%s height for width: %d is minimum %d and natural: %d",
	     G_OBJECT_TYPE_NAME (cell), width,
	     minimum_height ? *minimum_height : 20000, 
	     natural_height ? *natural_height : 20000);
#endif
}

/**
 * gtk_extended_cell_get_desired_size:
 * @cell: a #GtkExtendedCell instance
 * @request_natural: Whether to base the contextual request off of the
 *     base natural or the base minimum
 * @minimum_size: (out) (allow-none): location for storing the minimum size, or %NULL
 * @natural_size: (out) (allow-none): location for storing the natural size, or %NULL
 *
 * Retrieves the minimum and natural size of a cell taking
 * into account the widget's preference for height-for-width management.
 *
 * If request_natural is specified, the non-contextual natural value will
 * be used to make the contextual request; otherwise the minimum will be used.
 *
 * Since: 3.0
 */
void
gtk_extended_cell_get_desired_size (GtkExtendedCell *cell,
				    GtkWidget       *widget,
				    gboolean         request_natural,
				    GtkRequisition  *minimum_size,
				    GtkRequisition  *natural_size)
{
  gint min_width, nat_width;
  gint min_height, nat_height;

  g_return_if_fail (GTK_IS_EXTENDED_CELL (cell));

  if (gtk_extended_cell_is_height_for_width (cell))
    {
      gtk_extended_cell_get_desired_width (cell, widget, &min_width, &nat_width);
      gtk_extended_cell_get_height_for_width (cell, widget, 
					      request_natural ? nat_width : min_width,
					      &min_height, &nat_height);
    }
  else
    {
      gtk_extended_cell_get_desired_height (cell, widget, &min_height, &nat_height);
      gtk_extended_cell_get_width_for_height (cell, widget, 
					      request_natural ? nat_height : min_height,
					      &min_width, &nat_width);
    }

  if (minimum_size)
    {
      minimum_size->width  = min_width;
      minimum_size->height = min_height;
    }

  if (natural_size)
    {
      natural_size->width  = nat_width;
      natural_size->height = nat_height;
    }
}


#define __GTK_EXTENDED_CELL_C__
#include "gtkaliasdef.c"
