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
#include "gtksizegroup.h"
#include "gtkprivate.h"
#include "gtkintl.h"
#include "gtkalias.h"


#define DEBUG_EXTENDED_LAYOUT 0


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




/* looks for a cached size request for this for_size. If not
 * found, returns the oldest entry so it can be overwritten */
static gboolean
_gtk_extended_layout_get_cached_desired_size (gfloat            for_size,
					      GtkDesiredSize   *cached_sizes,
					      GtkDesiredSize  **result)
{
  guint i;

  *result = &cached_sizes[0];

  for (i = 0; i < GTK_N_CACHED_SIZES; i++)
    {
      GtkDesiredSize *cs;

      cs = &cached_sizes[i];

      if (cs->age > 0 &&
          cs->for_size == for_size)
        {
          *result = cs;
          return TRUE;
        }
      else if (cs->age < (*result)->age)
        {
          *result = cs;
        }
    }

  return FALSE;
}

/**
 * gtk_extended_layout_is_height_for_width:
 * @layout: a #GtkExtendedLayout instance
 *
 * Gets whether the widget prefers a height-for-width layout
 * or a width-for-height layout
 *
 * Returns: %TRUE if the widget prefers height-for-width, %FALSE if
 * the widget should be treated with a width-for-height preference.
 *
 * Since: 3.0
 */
gboolean
gtk_extended_layout_is_height_for_width (GtkExtendedLayout *layout)
{
  GtkExtendedLayoutIface *iface;

  g_return_val_if_fail (GTK_IS_EXTENDED_LAYOUT (layout), FALSE);

  iface = GTK_EXTENDED_LAYOUT_GET_IFACE (layout);
  if (iface->is_height_for_width)
    return iface->is_height_for_width (layout);

  /* By default widgets are height-for-width. */
  return TRUE;
}

/**
 * gtk_extended_layout_get_desired_width:
 * @layout: a #GtkExtendedLayout instance
 * @minimum_width: location to store the minimum size, or %NULL
 * @natural_width: location to store the natural size, or %NULL
 *
 * Retreives a widget's minimum and natural size in a single dimension.
 *
 * Since: 3.0
 */
void
gtk_extended_layout_get_desired_width (GtkExtendedLayout *layout,
				       gint              *minimum_width,
				       gint              *natural_width)
{
  GtkWidgetAuxInfo       *aux_info;
  gboolean                found_in_cache = FALSE;
  GtkDesiredSize         *cached_size;

  g_return_if_fail (GTK_IS_EXTENDED_LAYOUT (layout));
  g_return_if_fail (minimum_width != NULL || natural_width != NULL);

  aux_info = _gtk_widget_get_aux_info (GTK_WIDGET (layout), TRUE);

  cached_size = &aux_info->desired_widths[0];

  if (GTK_WIDGET_WIDTH_REQUEST_NEEDED (layout) == FALSE)
    found_in_cache = _gtk_extended_layout_get_cached_desired_size (-1, aux_info->desired_widths, 
								   &cached_size);

  if (!found_in_cache)
    {
      GtkRequisition requisition;
      gint minimum_width = 0, natural_width = 0;

      /* Unconditionally envoke size-request and use those return values as 
       * the base end of our values */
      _gtk_size_group_compute_requisition (GTK_WIDGET (layout), &requisition);

      /* Envoke this after, default GtkWidgetClass will simply copy over widget->requisition
       */
      GTK_EXTENDED_LAYOUT_GET_IFACE (layout)->get_desired_width (layout, 
								 &minimum_width, 
								 &natural_width);

      minimum_width = MAX (minimum_width, requisition.width);
      natural_width = MAX (natural_width, requisition.width);

      /* XXX Possibly we should update this with minimum values instead */
      _gtk_size_group_bump_requisition (GTK_WIDGET (layout), GTK_SIZE_GROUP_HORIZONTAL, natural_width);

      cached_size->minimum_size = minimum_width;
      cached_size->natural_size = natural_width;
      cached_size->for_size     = -1;
      cached_size->age          = aux_info->cached_width_age;

      aux_info->cached_width_age ++;

      GTK_PRIVATE_UNSET_FLAG (layout, GTK_WIDTH_REQUEST_NEEDED);
    }

  if (minimum_width)
    *minimum_width = cached_size->minimum_size;

  if (natural_width)
    *natural_width = cached_size->natural_size;

  g_assert (!minimum_width || !natural_width || *minimum_width <= *natural_width);

#if DEBUG_EXTENDED_LAYOUT
  g_message ("%s returning minimum width: %d and natural width: %d",
	     G_OBJECT_TYPE_NAME (layout), 
	     cached_size->minimum_size,
	     cached_size->natural_size);
#endif
}


/**
 * gtk_extended_layout_get_desired_height:
 * @layout: a #GtkExtendedLayout instance
 * @minimum_width: location to store the minimum size, or %NULL
 * @natural_width: location to store the natural size, or %NULL
 *
 * Retreives a widget's minimum and natural size in a single dimension.
 *
 * Since: 3.0
 */
void
gtk_extended_layout_get_desired_height (GtkExtendedLayout *layout,
					gint              *minimum_height,
					gint              *natural_height)
{
  GtkWidgetAuxInfo       *aux_info;
  gboolean                found_in_cache = FALSE;
  GtkDesiredSize         *cached_size;

  g_return_if_fail (GTK_IS_EXTENDED_LAYOUT (layout));
  g_return_if_fail (minimum_height != NULL || natural_height != NULL);

  aux_info = _gtk_widget_get_aux_info (GTK_WIDGET (layout), TRUE);

  cached_size = &aux_info->desired_heights[0];

  if (GTK_WIDGET_HEIGHT_REQUEST_NEEDED (layout) == FALSE)
    found_in_cache = _gtk_extended_layout_get_cached_desired_size (-1, aux_info->desired_heights, 
								   &cached_size);

  if (!found_in_cache)
    {
      GtkRequisition requisition;
      gint minimum_height = 0, natural_height = 0;

      /* Unconditionally envoke size-request and use those return values as 
       * the base end of our values */
      _gtk_size_group_compute_requisition (GTK_WIDGET (layout), &requisition);

      /* Envoke this after, default GtkWidgetClass will simply copy over widget->requisition
       */
      GTK_EXTENDED_LAYOUT_GET_IFACE (layout)->get_desired_height (layout, 
								  &minimum_height, 
								  &natural_height);

      minimum_height = MAX (minimum_height, requisition.height);
      natural_height = MAX (natural_height, requisition.height);

      /* XXX Possibly we should update this with minimum values instead */
      _gtk_size_group_bump_requisition (GTK_WIDGET (layout), GTK_SIZE_GROUP_VERTICAL, natural_height);

      cached_size->minimum_size = minimum_height;
      cached_size->natural_size = natural_height;
      cached_size->for_size     = -1;
      cached_size->age          = aux_info->cached_height_age;

      aux_info->cached_height_age ++;

      GTK_PRIVATE_UNSET_FLAG (layout, GTK_HEIGHT_REQUEST_NEEDED);
    }

  if (minimum_height)
    *minimum_height = cached_size->minimum_size;

  if (natural_height)
    *natural_height = cached_size->natural_size;

  g_assert (!minimum_height || !natural_height || *minimum_height <= *natural_height);

#if DEBUG_EXTENDED_LAYOUT
  g_message ("%s returning minimum height: %d and natural height: %d",
	     G_OBJECT_TYPE_NAME (layout), 
	     cached_size->minimum_size,
	     cached_size->natural_size);
#endif
}



/**
 * gtk_extended_layout_get_width_for_height:
 * @layout: a #GtkExtendedLayout instance
 * @height: the size which is available for allocation
 * @minimum_size: location for storing the minimum size, or %NULL
 * @natural_size: location for storing the natural size, or %NULL
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
  GtkWidgetAuxInfo       *aux_info;
  gboolean                found_in_cache = FALSE;
  GtkDesiredSize         *cached_size;

  g_return_if_fail (GTK_IS_EXTENDED_LAYOUT (layout));
  g_return_if_fail (minimum_width != NULL || natural_width != NULL);

  aux_info = _gtk_widget_get_aux_info (GTK_WIDGET (layout), TRUE);

  cached_size = &aux_info->desired_widths[0];

  if (GTK_WIDGET_WIDTH_REQUEST_NEEDED (layout) == FALSE)
    found_in_cache = _gtk_extended_layout_get_cached_desired_size (height, aux_info->desired_widths, 
								   &cached_size);

  if (!found_in_cache)
    {
      GtkRequisition requisition;
      gint minimum_width = 0, natural_width = 0;

      /* Unconditionally envoke size-request and use those return values as 
       * the base end of our values */
      _gtk_size_group_compute_requisition (GTK_WIDGET (layout), &requisition);

      GTK_EXTENDED_LAYOUT_GET_IFACE (layout)->get_width_for_height (layout, 
								    height,
								    &minimum_width, 
								    &natural_width);

      minimum_width = MAX (minimum_width, requisition.width);
      natural_width = MAX (natural_width, requisition.width);

      /* XXX Possibly we should update this with minimum values instead */
      _gtk_size_group_bump_requisition (GTK_WIDGET (layout), GTK_SIZE_GROUP_HORIZONTAL, natural_width);
      
      cached_size->minimum_size = minimum_width;
      cached_size->natural_size = natural_width;
      cached_size->for_size     = height;
      cached_size->age          = aux_info->cached_width_age;

      aux_info->cached_width_age++;

      GTK_PRIVATE_UNSET_FLAG (layout, GTK_WIDTH_REQUEST_NEEDED);
    }


  if (minimum_width)
    *minimum_width = cached_size->minimum_size;

  if (natural_width)
    *natural_width = cached_size->natural_size;

  g_assert (!minimum_width || !natural_width || *minimum_width <= *natural_width);

#if DEBUG_EXTENDED_LAYOUT
  g_message ("%s width for height: %d is minimum %d and natural: %d",
	     G_OBJECT_TYPE_NAME (layout), height,
	     cached_size->minimum_size,
	     cached_size->natural_size);
#endif

}

/**
 * gtk_extended_layout_get_height_for_width:
 * @layout: a #GtkExtendedLayout instance
 * @width: the size which is available for allocation
 * @minimum_size: location for storing the minimum size, or %NULL
 * @natural_size: location for storing the natural size, or %NULL
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
  GtkWidgetAuxInfo       *aux_info;
  gboolean                found_in_cache = FALSE;
  GtkDesiredSize         *cached_size;

  g_return_if_fail (GTK_IS_EXTENDED_LAYOUT (layout));
  g_return_if_fail (minimum_height != NULL || natural_height != NULL);

  aux_info = _gtk_widget_get_aux_info (GTK_WIDGET (layout), TRUE);

  cached_size = &aux_info->desired_heights[0];

  if (GTK_WIDGET_HEIGHT_REQUEST_NEEDED (layout) == FALSE)
    found_in_cache = _gtk_extended_layout_get_cached_desired_size (width, aux_info->desired_heights, 
								   &cached_size);

  if (!found_in_cache)
    {
      GtkRequisition requisition;
      gint minimum_height = 0, natural_height = 0;

      /* Unconditionally envoke size-request and use those return values as 
       * the base end of our values */
      _gtk_size_group_compute_requisition (GTK_WIDGET (layout), &requisition);

      GTK_EXTENDED_LAYOUT_GET_IFACE (layout)->get_height_for_width (layout, 
								    width,
								    &minimum_height, 
								    &natural_height);

      minimum_height = MAX (minimum_height, requisition.height);
      natural_height = MAX (natural_height, requisition.height);

      /* XXX Possibly we should update this with minimum values instead */
      _gtk_size_group_bump_requisition (GTK_WIDGET (layout), GTK_SIZE_GROUP_VERTICAL, natural_height);

      cached_size->minimum_size = minimum_height;
      cached_size->natural_size = natural_height;
      cached_size->for_size     = width;
      cached_size->age          = aux_info->cached_height_age;

      aux_info->cached_height_age++;

      GTK_PRIVATE_UNSET_FLAG (layout, GTK_HEIGHT_REQUEST_NEEDED);
    }


  if (minimum_height)
    *minimum_height = cached_size->minimum_size;

  if (natural_height)
    *natural_height = cached_size->natural_size;

  g_assert (!minimum_height || !natural_height || *minimum_height <= *natural_height);

#if DEBUG_EXTENDED_LAYOUT
  g_message ("%s height for width: %d is minimum %d and natural: %d",
	     G_OBJECT_TYPE_NAME (layout), width,
	     cached_size->minimum_size,
	     cached_size->natural_size);
#endif
}




/**
 * gtk_extended_layout_get_desired_size:
 * @layout: a #GtkExtendedLayout instance
 * @width: the size which is available for allocation
 * @minimum_size: location for storing the minimum size, or %NULL
 * @natural_size: location for storing the natural size, or %NULL
 *
 * Retreives the minimum and natural size of a widget taking
 * into account the widget's preference for height-for-width management.
 *
 * This is used to retreive a suitable size by container widgets whom dont 
 * impose any restrictions on the child placement, examples of these are 
 * #GtkWindow and #GtkScrolledWindow. 
 *
 * Since: 3.0
 */
void
gtk_extended_layout_get_desired_size (GtkExtendedLayout *layout,
                                      GtkRequisition    *minimum_size,
                                      GtkRequisition    *natural_size)
{
  gint min_width, nat_width;
  gint min_height, nat_height;

  g_return_if_fail (GTK_IS_EXTENDED_LAYOUT (layout));

  if (gtk_extended_layout_is_height_for_width (layout))
    {
      gtk_extended_layout_get_desired_width (layout, &min_width, &nat_width);
      gtk_extended_layout_get_height_for_width (layout, nat_width, &min_height, &nat_height);

      /* The minimum size here is the minimum height for the natrual width */
      if (minimum_size)
	{
	  minimum_size->width  = nat_width; 
	  minimum_size->height = min_height;
	}
      
    }
  else
    {
      gtk_extended_layout_get_desired_height (layout, &min_height, &nat_height);
      gtk_extended_layout_get_height_for_width (layout, nat_height, &min_width, &nat_width);

      /* The minimum size here is the minimum width for the natrual height */
      if (minimum_size)
	{
	  minimum_size->width  = min_width; 
	  minimum_size->height = nat_height;
	}
    }

  if (natural_size)
    {
      natural_size->width  = nat_width; 
      natural_size->height = nat_height;
    }


#if DEBUG_EXTENDED_LAYOUT
  g_message ("get_desired_size called on a %s; minimum width: %d natural width: %d minimum height %d natural height %d",
	     G_OBJECT_TYPE_NAME (layout), min_width, nat_width, min_height, nat_height);
#endif
}



#define __GTK_EXTENDED_LAYOUT_C__
#include "gtkaliasdef.c"
