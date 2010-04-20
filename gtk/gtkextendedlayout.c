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

/* With extended layout, a widget may be requested
 * its width for 2 or maximum 3 heights in one resize
 */
#define N_CACHED_SIZES 3

typedef struct 
{
  guint  age;
  gint   for_size;
  gint   minimum_size;
  gint   natural_size;
} DesiredSize;

typedef struct {
  DesiredSize desired_widths[N_CACHED_SIZES];
  DesiredSize desired_heights[N_CACHED_SIZES];
  guint8 cached_width_age;
  guint8 cached_height_age;
} ExtendedLayoutCache;

static GQuark quark_cache = 0;


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

      quark_cache = g_quark_from_static_string ("gtk-extended-layout-cache");
    }

  return extended_layout_type;
}

/* looks for a cached size request for this for_size. If not
 * found, returns the oldest entry so it can be overwritten */
static gboolean
get_cached_desired_size (gint           for_size,
			 DesiredSize   *cached_sizes,
			 DesiredSize  **result)
{
  guint i;

  *result = &cached_sizes[0];

  for (i = 0; i < N_CACHED_SIZES; i++)
    {
      DesiredSize *cs;

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

static void 
destroy_cache (ExtendedLayoutCache *cache)
{
  g_slice_free (ExtendedLayoutCache, cache);
}

ExtendedLayoutCache *
get_cache (GtkExtendedLayout *layout, gboolean create)
{
  ExtendedLayoutCache *cache;
  
  cache = g_object_get_qdata (G_OBJECT (layout), quark_cache);
  if (!cache && create)
    {
      cache = g_slice_new0 (ExtendedLayoutCache);

      cache->cached_width_age  = 1;
      cache->cached_height_age = 1;

      g_object_set_qdata_full (G_OBJECT (layout), quark_cache, cache, 
			       (GDestroyNotify)destroy_cache);
    }
  
  return cache;
}


static void
do_size_request (GtkWidget *widget)
{
  if (GTK_WIDGET_REQUEST_NEEDED (widget))
    {
      gtk_widget_ensure_style (widget);      
      GTK_PRIVATE_UNSET_FLAG (widget, GTK_REQUEST_NEEDED);
      g_signal_emit_by_name (widget,
			     "size-request",
			     &widget->requisition);
    }
}

static void
compute_size_for_orientation (GtkExtendedLayout *layout,
			      GtkSizeGroupMode   orientation,
			      gint               for_size,
			      gint              *minimum_size,
			      gint              *natural_size)
{
  ExtendedLayoutCache *cache;
  DesiredSize         *cached_size;
  GtkWidget           *widget;
  gboolean             found_in_cache = FALSE;

  g_return_if_fail (GTK_IS_EXTENDED_LAYOUT (layout));
  g_return_if_fail (minimum_size != NULL || natural_size != NULL);

  widget = GTK_WIDGET (layout);
  cache  = get_cache (layout, TRUE);

  if (orientation == GTK_SIZE_GROUP_HORIZONTAL)
    {
      cached_size = &cache->desired_widths[0];
      
      if (GTK_WIDGET_WIDTH_REQUEST_NEEDED (layout) == FALSE)
	found_in_cache = get_cached_desired_size (for_size, cache->desired_widths, &cached_size);
      else
	{
	  memset (cache->desired_widths, 0x0, N_CACHED_SIZES * sizeof (DesiredSize));
	  cache->cached_width_age = 1;
	}
    }
  else
    {
      cached_size = &cache->desired_heights[0];
      
      if (GTK_WIDGET_HEIGHT_REQUEST_NEEDED (layout) == FALSE)
	found_in_cache = get_cached_desired_size (for_size, cache->desired_heights, &cached_size);
      else
	{
	  memset (cache->desired_heights, 0x0, N_CACHED_SIZES * sizeof (DesiredSize));
	  cache->cached_height_age = 1;
	}
    }
    
  if (!found_in_cache)
    {
      gint min_size = 0, nat_size = 0;
      gint group_size, requisition_size;

      /* Unconditional size request runs but is often unhandled. */
      do_size_request (widget);

      if (orientation == GTK_SIZE_GROUP_HORIZONTAL)
	{
	  requisition_size = widget->requisition.width;

	  if (for_size < 0)
	    GTK_EXTENDED_LAYOUT_GET_IFACE (layout)->get_desired_width (layout, &min_size, &nat_size);
	  else
	    GTK_EXTENDED_LAYOUT_GET_IFACE (layout)->get_width_for_height (layout, for_size, &min_size, &nat_size);
	}
      else
	{
	  requisition_size = widget->requisition.height;

	  if (for_size < 0)
	    GTK_EXTENDED_LAYOUT_GET_IFACE (layout)->get_desired_height (layout, &min_size, &nat_size);
	  else
	    GTK_EXTENDED_LAYOUT_GET_IFACE (layout)->get_height_for_width (layout, for_size, &min_size, &nat_size);
	}

      /* Support for dangling "size-request" signals and forward derived
       * classes that will not default to a ->get_desired_width() that
       * returns the values in the ->requisition cache.
       */
      min_size = MAX (min_size, requisition_size);
      nat_size = MAX (nat_size, requisition_size);

      cached_size->minimum_size = min_size;
      cached_size->natural_size = nat_size;
      cached_size->for_size     = for_size;

      if (orientation == GTK_SIZE_GROUP_HORIZONTAL)
	{
	  cached_size->age = cache->cached_width_age;
	  cache->cached_width_age++;

	  GTK_PRIVATE_UNSET_FLAG (layout, GTK_WIDTH_REQUEST_NEEDED);
	}
      else
	{
	  cached_size->age = cache->cached_height_age;
	  cache->cached_height_age++;

	  GTK_PRIVATE_UNSET_FLAG (layout, GTK_HEIGHT_REQUEST_NEEDED);
	}

      /* Get size groups to compute the base requisition once one of the values have been cached,
       * then go ahead and update the cache with the sizegroup computed value.
       *
       * Note this is also where values from gtk_widget_set_size_request() are considered.
       */
      group_size = 
	_gtk_size_group_bump_requisition (GTK_WIDGET (layout), 
					  orientation, cached_size->minimum_size);

      cached_size->minimum_size = MAX (cached_size->minimum_size, group_size);
      cached_size->natural_size = MAX (cached_size->natural_size, group_size);
    }

  /* Output the MAX()s of the cached size and the size computed by GtkSizeGroup. */
  if (minimum_size)
    *minimum_size = cached_size->minimum_size;
  
  if (natural_size)
    *natural_size = cached_size->natural_size;

  g_assert (cached_size->minimum_size <= cached_size->natural_size);

  GTK_NOTE (EXTENDED_LAYOUT, 
	    g_print ("[%p] %s\t%s: %d is minimum %d and natural: %d (hit cache: %s)\n",
		     layout, G_OBJECT_TYPE_NAME (layout), 
		     orientation == GTK_SIZE_GROUP_HORIZONTAL ? 
		     "width for height" : "height for width" ,
		     for_size,
		     cached_size->minimum_size,
		     cached_size->natural_size,
		     found_in_cache ? "yes" : "no"));

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
  compute_size_for_orientation (layout, GTK_SIZE_GROUP_HORIZONTAL, -1, minimum_width, natural_width);
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
  compute_size_for_orientation (layout, GTK_SIZE_GROUP_VERTICAL, -1, minimum_height, natural_height);
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
  compute_size_for_orientation (layout, GTK_SIZE_GROUP_HORIZONTAL, height, minimum_width, natural_width);
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
  compute_size_for_orientation (layout, GTK_SIZE_GROUP_VERTICAL, width, minimum_height, natural_height);
}

/**
 * gtk_extended_layout_get_desired_size:
 * @layout: a #GtkExtendedLayout instance
 * @width: the size which is available for allocation
 * @request_natural: Whether to base the contextual request off of the 
 *  base natural or the base minimum
 * @minimum_size: location for storing the minimum size, or %NULL
 * @natural_size: location for storing the natural size, or %NULL
 *
 * Retreives the minimum and natural size of a widget taking
 * into account the widget's preference for height-for-width management.
 *
 * If request_natural is specified, the non-contextual natural value will
 * be used to make the contextual request; otherwise the minimum will be used.
 *
 * This is used to retreive a suitable size by container widgets whom dont 
 * impose any restrictions on the child placement, examples of these are 
 * #GtkWindow and #GtkScrolledWindow. 
 *
 * Since: 3.0
 */
void
gtk_extended_layout_get_desired_size (GtkExtendedLayout *layout,
				      gboolean           request_natural,
                                      GtkRequisition    *minimum_size,
                                      GtkRequisition    *natural_size)
{
  gint min_width, nat_width;
  gint min_height, nat_height;

  g_return_if_fail (GTK_IS_EXTENDED_LAYOUT (layout));

  if (gtk_extended_layout_is_height_for_width (layout))
    {
      gtk_extended_layout_get_desired_width (layout, &min_width, &nat_width);
      gtk_extended_layout_get_height_for_width (layout, 
						request_natural ? nat_width : min_width, 
						&min_height, &nat_height);
    }
  else
    {
      gtk_extended_layout_get_desired_height (layout, &min_height, &nat_height);
      gtk_extended_layout_get_width_for_height (layout, 
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



#define __GTK_EXTENDED_LAYOUT_C__
#include "gtkaliasdef.c"
