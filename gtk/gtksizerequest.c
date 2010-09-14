/* gtksizerequest.c
 * Copyright (C) 2007-2010 Openismus GmbH
 *
 * Authors:
 *      Mathias Hasselmann <mathias@openismus.com>
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


/**
 * SECTION:gtksizerequest
 * @Short_Description: Height-for-width geometry management
 * @Title: GtkSizeRequest
 *
 * The GtkSizeRequest interface is GTK+'s height-for-width (and width-for-height)
 * geometry management system. Height-for-width means that a widget can
 * change how much vertical space it needs, depending on the amount
 * of horizontal space that it is given (and similar for width-for-height).
 * The most common example is a label that reflows to fill up the available
 * width, wraps to fewer lines, and therefore needs less height.
 *
 * GTK+'s traditional two-pass <link linkend="size-allocation">size-allocation</link>
 * algorithm does not allow this flexibility. #GtkWidget provides a default
 * implementation of the #GtkSizeRequest interface for existing widgets,
 * which always requests the same height, regardless of the available width.
 *
 * <refsect2>
 * <title>Implementing GtkSizeRequest</title>
 * <para>
 * Some important things to keep in mind when implementing
 * the GtkSizeRequest interface and when using it in container
 * implementations.
 *
 * The geometry management system will query a logical hierarchy in
 * only one orientation at a time. When widgets are initially queried
 * for their minimum sizes it is generally done in a dual pass
 * in the direction chosen by the toplevel.
 *
 * For instance when queried in the normal height-for-width mode:
 * First the default minimum and natural width for each widget
 * in the interface will computed and collectively returned to
 * the toplevel by way of gtk_size_request_get_width().
 * Next, the toplevel will use the minimum width to query for the
 * minimum height contextual to that width using
 * gtk_size_request_get_height_for_width(), which will also be a
 * highly recursive operation. This minimum-for-minimum size can be
 * used to set the minimum size constraint on the toplevel.
 *
 * When allocating, each container can use the minimum and natural
 * sizes reported by their children to allocate natural sizes and
 * expose as much content as possible with the given allocation.
 *
 * That means that the request operation at allocation time will
 * usually fire again in contexts of different allocated sizes than
 * the ones originally queried for. #GtkSizeRequest caches a
 * small number of results to avoid re-querying for the same
 * allocated size in one allocation cycle.
 *
 * A widget that does not actually do height-for-width
 * or width-for-height size negotiations only has to implement
 * get_width() and get_height().
 *
 * If a widget does move content around to smartly use up the
 * allocated size, then it must support the request properly in
 * both orientations; even if the request only makes sense in
 * one orientation.
 *
 * For instance, a GtkLabel that does height-for-width word wrapping
 * will not expect to have get_height() called because that
 * call is specific to a width-for-height request, in this case the
 * label must return the heights contextual to its minimum possible
 * width. By following this rule any widget that handles height-for-width
 * or width-for-height requests will always be allocated at least
 * enough space to fit its own content.
 * </para>
 * </refsect2>
 */


#include <config.h>
#include "gtksizerequest.h"
#include "gtksizegroup.h"
#include "gtkprivate.h"
#include "gtkintl.h"

typedef GtkSizeRequestIface GtkSizeRequestInterface;
G_DEFINE_INTERFACE (GtkSizeRequest,
		    gtk_size_request,
		    GTK_TYPE_WIDGET);


static void
gtk_size_request_default_init (GtkSizeRequestInterface *iface)
{
}


/* looks for a cached size request for this for_size. If not
 * found, returns the oldest entry so it can be overwritten
 *
 * Note that this caching code was directly derived from
 * the Clutter toolkit.
 */
static gboolean
get_cached_size (gint           for_size,
		 SizeRequest   *cached_sizes,
		 SizeRequest  **result)
{
  guint i;

  *result = &cached_sizes[0];

  for (i = 0; i < GTK_SIZE_REQUEST_CACHED_SIZES; i++)
    {
      SizeRequest *cs;

      cs = &cached_sizes[i];

      if (cs->age > 0 && cs->for_size == for_size)
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
do_size_request (GtkWidget      *widget,
		 GtkRequisition *requisition)
{
  /* Now we dont bother caching the deprecated "size-request" returns,
   * just unconditionally invoke here just in case we run into legacy stuff */
  gtk_widget_ensure_style (widget);
  g_signal_emit_by_name (widget, "size-request", requisition);
}

#ifndef G_DISABLE_CHECKS
static GQuark recursion_check_quark = 0;
#endif /* G_DISABLE_CHECKS */

static void
push_recursion_check (GtkSizeRequest  *request,
                      GtkSizeGroupMode orientation,
                      gint             for_size)
{
#ifndef G_DISABLE_CHECKS
  const char *previous_method;
  const char *method;

  if (recursion_check_quark == 0)
    recursion_check_quark = g_quark_from_static_string ("gtk-size-request-in-progress");

  previous_method = g_object_get_qdata (G_OBJECT (request), recursion_check_quark);

  if (orientation == GTK_SIZE_GROUP_HORIZONTAL)
    {
      method = for_size < 0 ? "get_width" : "get_width_for_height";
    }
  else
    {
      method = for_size < 0 ? "get_height" : "get_height_for_width";
    }

  if (previous_method != NULL)
    {
      g_warning ("%s %p: widget tried to gtk_size_request_%s inside "
                 " GtkSizeRequest::%s implementation. "
                 "Should just invoke GTK_SIZE_REQUEST_GET_IFACE(widget)->%s "
                 "directly rather than using gtk_size_request_%s",
                 G_OBJECT_TYPE_NAME (request), request,
                 method, previous_method,
                 method, method);
    }

  g_object_set_qdata (G_OBJECT (request), recursion_check_quark, (char*) method);
#endif /* G_DISABLE_CHECKS */
}

static void
pop_recursion_check (GtkSizeRequest  *request,
                     GtkSizeGroupMode orientation)
{
#ifndef G_DISABLE_CHECKS
  g_object_set_qdata (G_OBJECT (request), recursion_check_quark, NULL);
#endif
}

static void
compute_size_for_orientation (GtkSizeRequest    *request,
                              GtkSizeGroupMode   orientation,
                              gint               for_size,
                              gint              *minimum_size,
                              gint              *natural_size)
{
  SizeRequestCache *cache;
  SizeRequest      *cached_size;
  GtkWidget        *widget;
  gboolean          found_in_cache = FALSE;
  int adjusted_min, adjusted_natural;

  g_return_if_fail (GTK_IS_SIZE_REQUEST (request));
  g_return_if_fail (minimum_size != NULL || natural_size != NULL);

  widget = GTK_WIDGET (request);
  cache  = _gtk_widget_peek_request_cache (widget);

  if (orientation == GTK_SIZE_GROUP_HORIZONTAL)
    {
      cached_size = &cache->widths[0];

      if (!GTK_WIDGET_WIDTH_REQUEST_NEEDED (request))
        found_in_cache = get_cached_size (for_size, cache->widths, &cached_size);
      else
        {
          memset (cache->widths, 0, GTK_SIZE_REQUEST_CACHED_SIZES * sizeof (SizeRequest));
          cache->cached_width_age = 1;
        }
    }
  else
    {
      cached_size = &cache->heights[0];

      if (!GTK_WIDGET_HEIGHT_REQUEST_NEEDED (request))
        found_in_cache = get_cached_size (for_size, cache->heights, &cached_size);
      else
        {
          memset (cache->heights, 0, GTK_SIZE_REQUEST_CACHED_SIZES * sizeof (SizeRequest));
          cache->cached_height_age = 1;
        }
    }

  if (!found_in_cache)
    {
      GtkRequisition requisition = { 0, 0 };
      gint min_size = 0, nat_size = 0;
      gint requisition_size;

      /* Unconditional size request runs but is often unhandled. */
      do_size_request (widget, &requisition);

      push_recursion_check (request, orientation, for_size);
      if (orientation == GTK_SIZE_GROUP_HORIZONTAL)
        {
          requisition_size = requisition.width;

          if (for_size < 0)
            GTK_SIZE_REQUEST_GET_IFACE (request)->get_width (request, &min_size, &nat_size);
          else
            GTK_SIZE_REQUEST_GET_IFACE (request)->get_width_for_height (request, for_size, 
									&min_size, &nat_size);
        }
      else
        {
          requisition_size = requisition.height;

          if (for_size < 0)
            GTK_SIZE_REQUEST_GET_IFACE (request)->get_height (request, &min_size, &nat_size);
          else
            GTK_SIZE_REQUEST_GET_IFACE (request)->get_height_for_width (request, for_size, 
									&min_size, &nat_size);
        }
      pop_recursion_check (request, orientation);

      if (min_size > nat_size)
        {
          g_warning ("%s %p reported min size %d and natural size %d; natural size must be >= min size",
                     G_OBJECT_TYPE_NAME (request), request, min_size, nat_size);
        }

      /* Support for dangling "size-request" signal implementations on
       * legacy widgets
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

          GTK_PRIVATE_UNSET_FLAG (request, GTK_WIDTH_REQUEST_NEEDED);
        }
      else
        {
          cached_size->age = cache->cached_height_age;
          cache->cached_height_age++;

          GTK_PRIVATE_UNSET_FLAG (request, GTK_HEIGHT_REQUEST_NEEDED);
        }

      adjusted_min = cached_size->minimum_size;
      adjusted_natural = cached_size->natural_size;
      GTK_WIDGET_GET_CLASS (request)->adjust_size_request (GTK_WIDGET (request),
                                                           orientation == GTK_SIZE_GROUP_HORIZONTAL ?
                                                           GTK_ORIENTATION_HORIZONTAL :
                                                           GTK_ORIENTATION_VERTICAL,
                                                           cached_size->for_size,
                                                           &adjusted_min,
                                                           &adjusted_natural);

      if (adjusted_min < cached_size->minimum_size ||
          adjusted_natural < cached_size->natural_size)
        {
          g_warning ("%s %p adjusted size %s min %d natural %d must not decrease below min %d natural %d",
                     G_OBJECT_TYPE_NAME (request), request,
                     orientation == GTK_SIZE_GROUP_VERTICAL ? "vertical" : "horizontal",
                     adjusted_min, adjusted_natural,
                     cached_size->minimum_size, cached_size->natural_size);
          /* don't use the adjustment */
        }
      else if (adjusted_min > adjusted_natural)
        {
          g_warning ("%s %p adjusted size %s min %d natural %d original min %d natural %d has min greater than natural",
                     G_OBJECT_TYPE_NAME (request), request,
                     orientation == GTK_SIZE_GROUP_VERTICAL ? "vertical" : "horizontal",
                     adjusted_min, adjusted_natural,
                     cached_size->minimum_size, cached_size->natural_size);
          /* don't use the adjustment */
        }
      else
        {
          /* adjustment looks good */
          cached_size->minimum_size = adjusted_min;
          cached_size->natural_size = adjusted_natural;
        }

      /* Update size-groups with our request and update our cached requests 
       * with the size-group values in a single pass.
       */
      _gtk_size_group_bump_requisition (GTK_WIDGET (request),
					orientation, 
					&cached_size->minimum_size,
					&cached_size->natural_size);
    }

  if (minimum_size)
    *minimum_size = cached_size->minimum_size;

  if (natural_size)
    *natural_size = cached_size->natural_size;

  g_assert (cached_size->minimum_size <= cached_size->natural_size);

  GTK_NOTE (SIZE_REQUEST,
            g_print ("[%p] %s\t%s: %d is minimum %d and natural: %d (hit cache: %s)\n",
                     request, G_OBJECT_TYPE_NAME (request),
                     orientation == GTK_SIZE_GROUP_HORIZONTAL ?
                     "width for height" : "height for width" ,
                     for_size,
                     cached_size->minimum_size,
                     cached_size->natural_size,
                     found_in_cache ? "yes" : "no"));

}

/**
 * gtk_size_request_get_request_mode:
 * @widget: a #GtkSizeRequest instance
 *
 * Gets whether the widget prefers a height-for-width layout
 * or a width-for-height layout.
 *
 * <note><para>#GtkBin widgets generally propagate the preference of
 * their child, container widgets need to request something either in
 * context of their children or in context of their allocation
 * capabilities.</para></note>
 *
 * Returns: The #GtkSizeRequestMode preferred by @widget.
 *
 * Since: 3.0
 */
GtkSizeRequestMode
gtk_size_request_get_request_mode (GtkSizeRequest *widget)
{
  GtkSizeRequestIface *iface;

  g_return_val_if_fail (GTK_IS_SIZE_REQUEST (widget), GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH);

  iface = GTK_SIZE_REQUEST_GET_IFACE (widget);
  if (iface->get_request_mode)
    return iface->get_request_mode (widget);

  /* By default widgets are height-for-width. */
  return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

/**
 * gtk_size_request_get_width:
 * @widget: a #GtkSizeRequest instance
 * @minimum_width: (out) (allow-none): location to store the minimum width, or %NULL
 * @natural_width: (out) (allow-none): location to store the natural width, or %NULL
 *
 * Retrieves a widget's initial minimum and natural width.
 *
 * <note><para>This call is specific to height-for-width
 * requests.</para></note>
 *
 * Since: 3.0
 */
void
gtk_size_request_get_width (GtkSizeRequest *widget,
			    gint           *minimum_width,
			    gint           *natural_width)
{
  compute_size_for_orientation (widget, GTK_SIZE_GROUP_HORIZONTAL,
                                -1, minimum_width, natural_width);
}


/**
 * gtk_size_request_get_height:
 * @widget: a #GtkSizeRequest instance
 * @minimum_height: (out) (allow-none): location to store the minimum height, or %NULL
 * @natural_height: (out) (allow-none): location to store the natural height, or %NULL
 *
 * Retrieves a widget's initial minimum and natural height.
 *
 * <note><para>This call is specific to width-for-height requests.</para></note>
 *
 * Since: 3.0
 */
void
gtk_size_request_get_height (GtkSizeRequest *widget,
			     gint           *minimum_height,
			     gint           *natural_height)
{
  compute_size_for_orientation (widget, GTK_SIZE_GROUP_VERTICAL,
                                -1, minimum_height, natural_height);
}



/**
 * gtk_size_request_get_width_for_height:
 * @widget: a #GtkSizeRequest instance
 * @height: the height which is available for allocation
 * @minimum_width: (out) (allow-none): location for storing the minimum width, or %NULL
 * @natural_width: (out) (allow-none): location for storing the natural width, or %NULL
 *
 * Retrieves a widget's minimum and natural width if it would be given
 * the specified @height.
 *
 * Since: 3.0
 */
void
gtk_size_request_get_width_for_height (GtkSizeRequest *widget,
				       gint            height,
				       gint           *minimum_width,
				       gint           *natural_width)
{
  compute_size_for_orientation (widget, GTK_SIZE_GROUP_HORIZONTAL,
                                height, minimum_width, natural_width);
}

/**
 * gtk_size_request_get_height_for_width:
 * @widget: a #GtkSizeRequest instance
 * @width: the width which is available for allocation
 * @minimum_height: (out) (allow-none): location for storing the minimum height, or %NULL
 * @natural_height: (out) (allow-none): location for storing the natural height, or %NULL
 *
 * Retrieves a widget's minimum and natural height if it would be given
 * the specified @width.
 *
 * Since: 3.0
 */
void
gtk_size_request_get_height_for_width (GtkSizeRequest *widget,
				       gint            width,
				       gint           *minimum_height,
				       gint           *natural_height)
{
  compute_size_for_orientation (widget, GTK_SIZE_GROUP_VERTICAL,
                                width, minimum_height, natural_height);
}

/**
 * gtk_size_request_get_size:
 * @widget: a #GtkSizeRequest instance
 * @minimum_size: (out) (allow-none): location for storing the minimum size, or %NULL
 * @natural_size: (out) (allow-none): location for storing the natural size, or %NULL
 *
 * Retrieves the minimum and natural size of a widget taking
 * into account the widget's preference for height-for-width management.
 *
 * This is used to retrieve a suitable size by container widgets which do
 * not impose any restrictions on the child placement.
 *
 * Since: 3.0
 */
void
gtk_size_request_get_size (GtkSizeRequest    *widget,
			   GtkRequisition    *minimum_size,
			   GtkRequisition    *natural_size)
{
  gint min_width, nat_width;
  gint min_height, nat_height;

  g_return_if_fail (GTK_IS_SIZE_REQUEST (widget));

  if (gtk_size_request_get_request_mode (widget) == GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH)
    {
      gtk_size_request_get_width (widget, &min_width, &nat_width);

      if (minimum_size)
	{
	  minimum_size->width = min_width;
	  gtk_size_request_get_height_for_width (widget, min_width,
						 &minimum_size->height, NULL);
	}

      if (natural_size)
	{
	  natural_size->width = nat_width;
	  gtk_size_request_get_height_for_width (widget, nat_width,
						 NULL, &natural_size->height);
	}
    }
  else /* GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT */
    {
      gtk_size_request_get_height (widget, &min_height, &nat_height);

      if (minimum_size)
	{
	  minimum_size->height = min_height;
	  gtk_size_request_get_width_for_height (widget, min_height,
						 &minimum_size->width, NULL);
	}

      if (natural_size)
	{
	  natural_size->height = nat_height;
	  gtk_size_request_get_width_for_height (widget, nat_height,
						 NULL, &natural_size->width);
	}
    }
}


static gint
compare_gap (gconstpointer p1,
	     gconstpointer p2,
	     gpointer      data)
{
  GtkRequestedSize *sizes = data;
  const guint *c1 = p1;
  const guint *c2 = p2;

  const gint d1 = MAX (sizes[*c1].natural_size -
                       sizes[*c1].minimum_size,
                       0);
  const gint d2 = MAX (sizes[*c2].natural_size -
                       sizes[*c2].minimum_size,
                       0);

  gint delta = (d2 - d1);

  if (0 == delta)
    delta = (*c2 - *c1);

  return delta;
}

/**
 * gtk_distribute_natural_allocation: 
 * @extra_space: Extra space to redistribute among children after subtracting
 *               minimum sizes and any child padding from the overall allocation
 * @n_requested_sizes: Number of requests to fit into the allocation
 * @sizes: An array of structs with a client pointer and a minimum/natural size
 *         in the orientation of the allocation.
 *
 * Distributes @extra_space to child @sizes by bringing up smaller
 * children up to natural size first.
 *
 * The remaining space will be added to the @minimum_size member of the
 * GtkRequestedSize struct. If all sizes reach their natural size then
 * the remaining space is returned.
 *
 * Returns: The remainder of @extra_space after redistributing space
 * to @sizes.
 */
gint 
gtk_distribute_natural_allocation (gint              extra_space,
				   guint             n_requested_sizes,
				   GtkRequestedSize *sizes)
{
  guint *spreading = g_newa (guint, n_requested_sizes);
  gint   i;

  g_return_val_if_fail (extra_space >= 0, 0);

  for (i = 0; i < n_requested_sizes; i++)
    spreading[i] = i;

  /* Distribute the container's extra space c_gap. We want to assign
   * this space such that the sum of extra space assigned to children
   * (c^i_gap) is equal to c_cap. The case that there's not enough
   * space for all children to take their natural size needs some
   * attention. The goals we want to achieve are:
   *
   *   a) Maximize number of children taking their natural size.
   *   b) The allocated size of children should be a continuous
   *   function of c_gap.  That is, increasing the container size by
   *   one pixel should never make drastic changes in the distribution.
   *   c) If child i takes its natural size and child j doesn't,
   *   child j should have received at least as much gap as child i.
   *
   * The following code distributes the additional space by following
   * these rules.
   */
  
  /* Sort descending by gap and position. */
  g_qsort_with_data (spreading,
		     n_requested_sizes, sizeof (guint),
		     compare_gap, sizes);
  
  /* Distribute available space.
   * This master piece of a loop was conceived by Behdad Esfahbod.
   */
  for (i = n_requested_sizes - 1; extra_space > 0 && i >= 0; --i)
    {
      /* Divide remaining space by number of remaining children.
       * Sort order and reducing remaining space by assigned space
       * ensures that space is distributed equally.
       */
      gint glue = (extra_space + i) / (i + 1);
      gint gap = sizes[(spreading[i])].natural_size
	- sizes[(spreading[i])].minimum_size;
      
      gint extra = MIN (glue, gap);
      
      sizes[spreading[i]].minimum_size += extra;
      
      extra_space -= extra;
    }

  return extra_space;
}

