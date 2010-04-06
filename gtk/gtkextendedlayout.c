/* gtkextendedlayout.c
 * Copyright (C) 2007 Openismus GmbH
 *
 * Author:
 *      Mathias Hasselmann <mathias@openismus.com>
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
#include "gtkextendedlayout.h"
#include "gtkintl.h"
#include "gtkalias.h"

GType
gtk_extended_layout_get_type (void)
{
  static GType extended_layout_type = 0;

  if (G_UNLIKELY(!extended_layout_type))
    {
      extended_layout_type =
	g_type_register_static_simple (G_TYPE_INTERFACE, I_("GtkExtendedLayout"),
				       sizeof (GtkExtendedLayoutIface),
				       NULL, 0, NULL, 0);

      g_type_interface_add_prerequisite (extended_layout_type, GTK_TYPE_WIDGET);
    }

  return extended_layout_type;
}

/**
 * gtk_extended_layout_get_desired_size:
 * @layout: a #GtkExtendedLayout instance
 * @minimum_size: location for storing the minimum size, or %NULL
 * @natural_size: location for storing the preferred size, or %NULL
 *
 * Retreives a widget's minimum and natural size and caches the values.
 *
 * <note><para>This api will consider any restrictions imposed by
 * #GtkSizeGroup<!-- -->s or previous calls to gtk_widget_set_size_request().
 * </para></note>
 *
 * Since: 3.0
 */
void
gtk_extended_layout_get_desired_size (GtkExtendedLayout *layout,
                                      GtkRequisition    *minimum_size,
                                      GtkRequisition    *natural_size)
{
  g_return_if_fail (GTK_IS_EXTENDED_LAYOUT (layout));
  g_return_if_fail (NULL != minimum_size || NULL != natural_size);

  _gtk_size_group_compute_desired_size (GTK_WIDGET (layout), minimum_size, natural_size);
}

/**
 * gtk_extended_layout_get_width_for_height:
 * @layout: a #GtkExtendedLayout instance
 * @height: the size which is available for allocation
 * @minimum_size: location for storing the minimum size, or %NULL
 * @natural_size: location for storing the preferred size, or %NULL
 *
 * Retreives a widget's desired width if it would be given
 * the specified @height.
 *
 * Since: 3.0
 */
void
gtk_extended_layout_get_width_for_height (GtkExtendedLayout *layout,
                                          gint               height,
                                          gint              *minimum_width,
                                          gint              *natural_width)
{
  GtkExtendedLayoutIface *iface;

  g_return_if_fail (GTK_IS_EXTENDED_LAYOUT (layout));

  iface = GTK_EXTENDED_LAYOUT_GET_IFACE (layout);
  iface->get_width_for_height (layout, height, minimum_width, natural_width);
}

/**
 * gtk_extended_layout_get_height_for_width:
 * @layout: a #GtkExtendedLayout instance
 * @width: the size which is available for allocation
 * @minimum_size: location for storing the minimum size, or %NULL
 * @natural_size: location for storing the preferred size, or %NULL
 *
 * Retreives a widget's desired height if it would be given
 * the specified @width.
 *
 * Since: 3.0
 */
void
gtk_extended_layout_get_height_for_width (GtkExtendedLayout *layout,
                                          gint               width,
                                          gint              *minimum_height,
                                          gint              *natural_height)
{
  GtkExtendedLayoutIface *iface;

  g_return_if_fail (GTK_IS_EXTENDED_LAYOUT (layout));

  iface = GTK_EXTENDED_LAYOUT_GET_IFACE (layout);
  iface->get_height_for_width (layout, width, minimum_height, natural_height);
}

#define __GTK_EXTENDED_LAYOUT_C__
#include "gtkaliasdef.c"
