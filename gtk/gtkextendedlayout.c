/* gtkextendedlayout.c
 * Copyright (C) 2007 Mathias Hasselmann <mathias.hasselmann@gmx.de>
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
    extended_layout_type =
      g_type_register_static_simple (G_TYPE_INTERFACE, I_("GtkExtendedLayout"),
				     sizeof (GtkExtendedLayoutIface),
				     NULL, 0, NULL, 0);

  return extended_layout_type;
}

/**
 * gtk_extended_layout_get_features:
 * @layout: a #GtkExtendedLayout
 *
 * Query which extended layout features are supported.
 *
 * Returns: the #GtkExtendedLayoutFeatures supported
 *
 * Since: 2.14
 **/
GtkExtendedLayoutFeatures
gtk_extended_layout_get_features (GtkExtendedLayout *layout)
{
  GtkExtendedLayoutIface *iface;

  g_return_val_if_fail (GTK_IS_EXTENDED_LAYOUT (layout), 0);

  iface = GTK_EXTENDED_LAYOUT_GET_IFACE (layout);

  g_return_val_if_fail (iface->get_features, 0);
  return iface->get_features(layout);
}

/**
 * gtk_extended_layout_get_height_for_width:
 * @layout: a #GtkExtendedLayout
 * @width: the horizontal space available
 *
 * Query which vertical space this extended layout item would require,
 * given the specified amount of horizontal space is made available.
 *
 * Returns: the height required when #width is assigned or -1 on error
 *
 * Since: 2.14
 **/
gint
gtk_extended_layout_get_height_for_width (GtkExtendedLayout *layout,
                                          gint               width)
{
  GtkExtendedLayoutIface *iface;

  g_return_val_if_fail (GTK_IS_EXTENDED_LAYOUT (layout), -1);

  iface = GTK_EXTENDED_LAYOUT_GET_IFACE (layout);

  g_return_val_if_fail (iface->get_height_for_width, -1);
  return iface->get_height_for_width(layout, width);
}

/**
 * gtk_extended_layout_get_width_for_height:
 * @layout: a #GtkExtendedLayout
 * @height: the vertical space available
 *
 * Query which horizontal space this extended layout item would require,
 * given the specified amount of vertical space is made available.
 *
 * Returns: the width required when #height is assigned or -1 on error
 *
 * Since: 2.14
 **/
gint
gtk_extended_layout_get_width_for_height (GtkExtendedLayout *layout,
                                          gint               height)
{
  GtkExtendedLayoutIface *iface;

  g_return_val_if_fail (GTK_IS_EXTENDED_LAYOUT (layout), -1);

  iface = GTK_EXTENDED_LAYOUT_GET_IFACE (layout);

  g_return_val_if_fail (iface->get_width_for_height, -1);
  return iface->get_width_for_height(layout, height);
}

/**
 * gtk_extended_layout_get_natural_size:
 * @layout: a #GtkExtendedLayout
 * @requisition: a #GtkRequisition to be filled in
 *
 * Query the natural (preferred) size of the layout item. 
 * This natural size can be larger than the requested size,
 * e.g. if the item contains optional or ellipized text.
 *
 * Since: 2.14
 **/
void
gtk_extended_layout_get_natural_size (GtkExtendedLayout *layout,
                                      GtkRequisition    *requisition)
{
  GtkExtendedLayoutIface *iface;

  g_return_if_fail (GTK_IS_EXTENDED_LAYOUT (layout));
  g_return_if_fail (NULL != requisition);

  iface = GTK_EXTENDED_LAYOUT_GET_IFACE (layout);

  g_return_if_fail (iface->get_natural_size);
  return iface->get_natural_size(layout, requisition);
}

/**
 * gtk_extended_layout_get_baselines:
 * @layout: a #GtkExtendedLayout
 * @baselines: an array of baselines to be filled in
 *
 * Query the baselines of the layout item. Baselines are imaginary 
 * horizontal lines used to vertically align text and images.
 *
 * Returns: the number of entries in @baselines or -1 on error
 *
 * Since: 2.14
 **/
gint
gtk_extended_layout_get_baselines (GtkExtendedLayout  *layout,
                                   gint              **baselines)
{
  GtkExtendedLayoutIface *iface;

  g_return_val_if_fail (GTK_IS_EXTENDED_LAYOUT (layout), -1);
  g_return_val_if_fail (NULL != baselines, -1);

  iface = GTK_EXTENDED_LAYOUT_GET_IFACE (layout);

  g_return_val_if_fail (iface->get_baselines, -1);
  return iface->get_baselines(layout, baselines);
}

/**
 * gtk_extended_layout_get_single_baseline:
 * @layout: a #GtkExtendedLayout
 * @policy: specifies the policy for choosing a baseline
 *
 * Queries a single baseline of the layout item.
 *
 * Since: 2.14
 **/
gint
gtk_extended_layout_get_single_baseline (GtkExtendedLayout *layout,
                                         GtkBaselinePolicy  policy)
{
  gint *baselines = NULL;
  gint offset = -1;
  gint count, i;

  g_return_val_if_fail (GTK_BASELINE_NONE != policy, -1);

  count = gtk_extended_layout_get_baselines (layout, &baselines);

  if (count > 0)
    {
      switch (policy)
        {
          case GTK_BASELINE_NONE:
            break;

          case GTK_BASELINE_FIRST:
            offset = baselines [0];
            break;

          case GTK_BASELINE_LAST:
            offset = baselines [count - 1];
            break;

          case GTK_BASELINE_AVERAGE:
            for (i = 0, offset = 0; i < count; ++i)
              offset += baselines[i];

            offset = (offset + count - 1) / count;
            break;
        }

      g_free (baselines);
    }

  return offset;
}

/**
 * gtk_extended_layout_get_padding:
 * @layout: a #GtkExtendedLayout
 * @padding: an #GtkBorder to be filled with the padding
 *
 * Query the padding this layout item puts arround its content.
 *
 * Since: 2.14
 **/
void
gtk_extended_layout_get_padding (GtkExtendedLayout *layout,
                                 GtkBorder         *padding)
{
  GtkExtendedLayoutIface *iface;

  g_return_if_fail (GTK_IS_EXTENDED_LAYOUT (layout));
  g_return_if_fail (NULL != padding);

  iface = GTK_EXTENDED_LAYOUT_GET_IFACE (layout);

  g_return_if_fail (iface->get_padding);
  iface->get_padding(layout, padding);
}

#define __GTK_EXTENDED_LAYOUT_C__
#include "gtkaliasdef.c"
