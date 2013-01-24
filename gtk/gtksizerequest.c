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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <config.h>

#include "gtksizerequest.h"

#include "gtkdebug.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtksizegroup-private.h"
#include "gtksizerequestcacheprivate.h"
#include "gtkwidgetprivate.h"
#include "deprecated/gtkstyle.h"


#ifndef G_DISABLE_CHECKS
static GQuark recursion_check_quark = 0;
#endif /* G_DISABLE_CHECKS */

static void
push_recursion_check (GtkWidget       *widget,
                      GtkOrientation   orientation,
                      gint             for_size)
{
#ifndef G_DISABLE_CHECKS
  const char *previous_method;
  const char *method;

  if (recursion_check_quark == 0)
    recursion_check_quark = g_quark_from_static_string ("gtk-size-request-in-progress");

  previous_method = g_object_get_qdata (G_OBJECT (widget), recursion_check_quark);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      method = for_size < 0 ? "get_width" : "get_width_for_height";
    }
  else
    {
      method = for_size < 0 ? "get_height" : "get_height_for_width";
    }

  if (previous_method != NULL)
    {
      g_warning ("%s %p: widget tried to gtk_widget_%s inside "
                 " GtkWidget     ::%s implementation. "
                 "Should just invoke GTK_WIDGET_GET_CLASS(widget)->%s "
                 "directly rather than using gtk_widget_%s",
                 G_OBJECT_TYPE_NAME (widget), widget,
                 method, previous_method,
                 method, method);
    }

  g_object_set_qdata (G_OBJECT (widget), recursion_check_quark, (char*) method);
#endif /* G_DISABLE_CHECKS */
}

static void
pop_recursion_check (GtkWidget       *widget,
                     GtkOrientation   orientation)
{
#ifndef G_DISABLE_CHECKS
  g_object_set_qdata (G_OBJECT (widget), recursion_check_quark, NULL);
#endif
}

static const char *
get_vfunc_name (GtkOrientation orientation,
                gint           for_size)
{
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    return for_size < 0 ? "get_preferred_width" : "get_preferred_width_for_height";
  else
    return for_size < 0 ? "get_preferred_height" : "get_preferred_height_for_width";
}

static void
gtk_widget_query_size_for_orientation (GtkWidget        *widget,
                                       GtkOrientation    orientation,
                                       gint              for_size,
                                       gint             *minimum_size,
                                       gint             *natural_size)
{
  SizeRequestCache *cache;
  gint min_size = 0;
  gint nat_size = 0;
  gboolean found_in_cache;

  if (gtk_widget_get_request_mode (widget) == GTK_SIZE_REQUEST_CONSTANT_SIZE)
    for_size = -1;

  cache = _gtk_widget_peek_request_cache (widget);
  found_in_cache = _gtk_size_request_cache_lookup (cache,
                                                   orientation,
                                                   for_size,
                                                   &min_size,
                                                   &nat_size);
  
  if (!found_in_cache)
    {
      gint adjusted_min, adjusted_natural, adjusted_for_size = for_size;

      gtk_widget_ensure_style (widget);

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          if (for_size < 0)
            {
	      push_recursion_check (widget, orientation, for_size);
              GTK_WIDGET_GET_CLASS (widget)->get_preferred_width (widget, &min_size, &nat_size);
	      pop_recursion_check (widget, orientation);
            }
          else
            {
              gint ignored_position = 0;
              gint minimum_height;
              gint natural_height;

	      /* Pull the base natural height from the cache as it's needed to adjust
	       * the proposed 'for_size' */
	      gtk_widget_get_preferred_height (widget, &minimum_height, &natural_height);

              /* convert for_size to unadjusted height (for_size is a proposed allocation) */
              GTK_WIDGET_GET_CLASS (widget)->adjust_size_allocation (widget,
                                                                     GTK_ORIENTATION_VERTICAL,
                                                                     &minimum_height,
								     &natural_height,
                                                                     &ignored_position,
                                                                     &adjusted_for_size);

	      push_recursion_check (widget, orientation, for_size);
              GTK_WIDGET_GET_CLASS (widget)->get_preferred_width_for_height (widget, 
									     MAX (adjusted_for_size, minimum_height),
									     &min_size, &nat_size);
	      pop_recursion_check (widget, orientation);
            }
        }
      else
        {
          if (for_size < 0)
            {
	      push_recursion_check (widget, orientation, for_size);
              GTK_WIDGET_GET_CLASS (widget)->get_preferred_height (widget, &min_size, &nat_size);
	      pop_recursion_check (widget, orientation);
            }
          else
            {
              gint ignored_position = 0;
              gint minimum_width;
              gint natural_width;

	      /* Pull the base natural width from the cache as it's needed to adjust
	       * the proposed 'for_size' */
	      gtk_widget_get_preferred_width (widget, &minimum_width, &natural_width);

              /* convert for_size to unadjusted width (for_size is a proposed allocation) */
              GTK_WIDGET_GET_CLASS (widget)->adjust_size_allocation (widget,
                                                                     GTK_ORIENTATION_HORIZONTAL,
								     &minimum_width,
                                                                     &natural_width,
                                                                     &ignored_position,
                                                                     &adjusted_for_size);

	      push_recursion_check (widget, orientation, for_size);
              GTK_WIDGET_GET_CLASS (widget)->get_preferred_height_for_width (widget, 
									     MAX (adjusted_for_size, minimum_width),
									     &min_size, &nat_size);
	      pop_recursion_check (widget, orientation);
            }
        }

      if (min_size > nat_size)
        {
          g_warning ("%s %p reported min size %d and natural size %d in %s(); natural size must be >= min size",
                     G_OBJECT_TYPE_NAME (widget), widget, min_size, nat_size, get_vfunc_name (orientation, for_size));
        }

      adjusted_min     = min_size;
      adjusted_natural = nat_size;
      GTK_WIDGET_GET_CLASS (widget)->adjust_size_request (widget,
                                                          orientation,
                                                          &adjusted_min,
                                                          &adjusted_natural);

      if (adjusted_min < min_size ||
          adjusted_natural < nat_size)
        {
          g_warning ("%s %p adjusted size %s min %d natural %d must not decrease below min %d natural %d",
                     G_OBJECT_TYPE_NAME (widget), widget,
                     orientation == GTK_ORIENTATION_VERTICAL ? "vertical" : "horizontal",
                     adjusted_min, adjusted_natural,
                     min_size, nat_size);
          /* don't use the adjustment */
        }
      else if (adjusted_min > adjusted_natural)
        {
          g_warning ("%s %p adjusted size %s min %d natural %d original min %d natural %d has min greater than natural",
                     G_OBJECT_TYPE_NAME (widget), widget,
                     orientation == GTK_ORIENTATION_VERTICAL ? "vertical" : "horizontal",
                     adjusted_min, adjusted_natural,
                     min_size, nat_size);
          /* don't use the adjustment */
        }
      else
        {
          /* adjustment looks good */
          min_size = adjusted_min;
          nat_size = adjusted_natural;
        }

      _gtk_size_request_cache_commit (cache,
                                      orientation,
                                      for_size,
                                      min_size,
                                      nat_size);
    }

  if (minimum_size)
    *minimum_size = min_size;

  if (natural_size)
    *natural_size = nat_size;

  g_assert (min_size <= nat_size);

  GTK_NOTE (SIZE_REQUEST,
            g_print ("[%p] %s\t%s: %d is minimum %d and natural: %d (hit cache: %s)\n",
                     widget, G_OBJECT_TYPE_NAME (widget),
                     orientation == GTK_ORIENTATION_HORIZONTAL ?
                     "width for height" : "height for width" ,
                     for_size, min_size, nat_size,
                     found_in_cache ? "yes" : "no"));
}

/* This is the main function that checks for a cached size and
 * possibly queries the widget class to compute the size if it's
 * not cached. If the for_size here is -1, then get_preferred_width()
 * or get_preferred_height() will be used.
 */
void
_gtk_widget_compute_size_for_orientation (GtkWidget        *widget,
                                          GtkOrientation    orientation,
                                          gint              for_size,
                                          gint             *minimum,
                                          gint             *natural)
{
  GHashTable *widgets;
  GHashTableIter iter;
  gpointer key;
  gint    min_result = 0, nat_result = 0;

  if (!gtk_widget_get_visible (widget) && !gtk_widget_is_toplevel (widget))
    {
      if (minimum)
        *minimum = 0;
      if (natural)
        *natural = 0;
      return;
    }

  if (G_LIKELY (!_gtk_widget_get_sizegroups (widget)))
    {
      gtk_widget_query_size_for_orientation (widget, orientation, for_size, minimum, natural);
      return;
    }

  widgets = _gtk_size_group_get_widget_peers (widget, orientation);

  g_hash_table_foreach (widgets, (GHFunc) g_object_ref, NULL);
  
  g_hash_table_iter_init (&iter, widgets);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      GtkWidget *tmp_widget = key;
      gint min_dimension, nat_dimension;

      gtk_widget_query_size_for_orientation (tmp_widget, orientation, for_size, &min_dimension, &nat_dimension);

      min_result = MAX (min_result, min_dimension);
      nat_result = MAX (nat_result, nat_dimension);
    }

  g_hash_table_foreach (widgets, (GHFunc) g_object_unref, NULL);

  g_hash_table_destroy (widgets);

  if (minimum)
    *minimum = min_result;

  if (natural)
    *natural = nat_result;
}

/**
 * gtk_widget_get_request_mode:
 * @widget: a #GtkWidget instance
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
gtk_widget_get_request_mode (GtkWidget *widget)
{
  SizeRequestCache *cache;

  g_return_val_if_fail (GTK_IS_WIDGET (widget), GTK_SIZE_REQUEST_CONSTANT_SIZE);

  cache = _gtk_widget_peek_request_cache (widget);

  if (!cache->request_mode_valid)
    {
      cache->request_mode = GTK_WIDGET_GET_CLASS (widget)->get_request_mode (widget);
      cache->request_mode_valid = TRUE;
    }

  return cache->request_mode;
}

/**
 * gtk_widget_get_preferred_width:
 * @widget: a #GtkWidget instance
 * @minimum_width: (out) (allow-none): location to store the minimum width, or %NULL
 * @natural_width: (out) (allow-none): location to store the natural width, or %NULL
 *
 * Retrieves a widget's initial minimum and natural width.
 *
 * <note><para>This call is specific to height-for-width
 * requests.</para></note>
 *
 * The returned request will be modified by the
 * GtkWidgetClass::adjust_size_request virtual method and by any
 * #GtkSizeGroup<!-- -->s that have been applied. That is, the returned request
 * is the one that should be used for layout, not necessarily the one
 * returned by the widget itself.
 *
 * Since: 3.0
 */
void
gtk_widget_get_preferred_width (GtkWidget *widget,
                                gint      *minimum_width,
                                gint      *natural_width)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (minimum_width != NULL || natural_width != NULL);

  _gtk_widget_compute_size_for_orientation (widget,
                                            GTK_ORIENTATION_HORIZONTAL,
                                            -1,
                                            minimum_width,
                                            natural_width);
}


/**
 * gtk_widget_get_preferred_height:
 * @widget: a #GtkWidget instance
 * @minimum_height: (out) (allow-none): location to store the minimum height, or %NULL
 * @natural_height: (out) (allow-none): location to store the natural height, or %NULL
 *
 * Retrieves a widget's initial minimum and natural height.
 *
 * <note><para>This call is specific to width-for-height requests.</para></note>
 *
 * The returned request will be modified by the
 * GtkWidgetClass::adjust_size_request virtual method and by any
 * #GtkSizeGroup<!-- -->s that have been applied. That is, the returned request
 * is the one that should be used for layout, not necessarily the one
 * returned by the widget itself.
 *
 * Since: 3.0
 */
void
gtk_widget_get_preferred_height (GtkWidget *widget,
                                 gint      *minimum_height,
                                 gint      *natural_height)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (minimum_height != NULL || natural_height != NULL);

  _gtk_widget_compute_size_for_orientation (widget,
                                            GTK_ORIENTATION_VERTICAL,
                                            -1,
                                            minimum_height,
                                            natural_height);
}



/**
 * gtk_widget_get_preferred_width_for_height:
 * @widget: a #GtkWidget instance
 * @height: the height which is available for allocation
 * @minimum_width: (out) (allow-none): location for storing the minimum width, or %NULL
 * @natural_width: (out) (allow-none): location for storing the natural width, or %NULL
 *
 * Retrieves a widget's minimum and natural width if it would be given
 * the specified @height.
 *
 * The returned request will be modified by the
 * GtkWidgetClass::adjust_size_request virtual method and by any
 * #GtkSizeGroup<!-- -->s that have been applied. That is, the returned request
 * is the one that should be used for layout, not necessarily the one
 * returned by the widget itself.
 *
 * Since: 3.0
 */
void
gtk_widget_get_preferred_width_for_height (GtkWidget *widget,
                                           gint       height,
                                           gint      *minimum_width,
                                           gint      *natural_width)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (minimum_width != NULL || natural_width != NULL);
  g_return_if_fail (height >= 0);

  _gtk_widget_compute_size_for_orientation (widget,
                                            GTK_ORIENTATION_HORIZONTAL,
                                            height,
                                            minimum_width,
                                            natural_width);
}

/**
 * gtk_widget_get_preferred_height_for_width:
 * @widget: a #GtkWidget instance
 * @width: the width which is available for allocation
 * @minimum_height: (out) (allow-none): location for storing the minimum height, or %NULL
 * @natural_height: (out) (allow-none): location for storing the natural height, or %NULL
 *
 * Retrieves a widget's minimum and natural height if it would be given
 * the specified @width.
 *
 * The returned request will be modified by the
 * GtkWidgetClass::adjust_size_request virtual method and by any
 * #GtkSizeGroup<!-- -->s that have been applied. That is, the returned request
 * is the one that should be used for layout, not necessarily the one
 * returned by the widget itself.
 *
 * Since: 3.0
 */
void
gtk_widget_get_preferred_height_for_width (GtkWidget *widget,
                                           gint       width,
                                           gint      *minimum_height,
                                           gint      *natural_height)
{
  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (minimum_height != NULL || natural_height != NULL);
  g_return_if_fail (width >= 0);

  _gtk_widget_compute_size_for_orientation (widget,
                                            GTK_ORIENTATION_VERTICAL,
                                            width,
                                            minimum_height,
                                            natural_height);
}

/**
 * gtk_widget_get_preferred_size:
 * @widget: a #GtkWidget instance
 * @minimum_size: (out) (allow-none): location for storing the minimum size, or %NULL
 * @natural_size: (out) (allow-none): location for storing the natural size, or %NULL
 *
 * Retrieves the minimum and natural size of a widget, taking
 * into account the widget's preference for height-for-width management.
 *
 * This is used to retrieve a suitable size by container widgets which do
 * not impose any restrictions on the child placement. It can be used
 * to deduce toplevel window and menu sizes as well as child widgets in
 * free-form containers such as GtkLayout.
 *
 * <note><para>Handle with care. Note that the natural height of a height-for-width
 * widget will generally be a smaller size than the minimum height, since the required
 * height for the natural width is generally smaller than the required height for
 * the minimum width.</para></note>
 *
 * Since: 3.0
 */
void
gtk_widget_get_preferred_size (GtkWidget      *widget,
                               GtkRequisition *minimum_size,
                               GtkRequisition *natural_size)
{
  gint min_width, nat_width;
  gint min_height, nat_height;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (gtk_widget_get_request_mode (widget) == GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH)
    {
      gtk_widget_get_preferred_width (widget, &min_width, &nat_width);

      if (minimum_size)
	{
	  minimum_size->width = min_width;
	  gtk_widget_get_preferred_height_for_width (widget, min_width,
                                                     &minimum_size->height, NULL);
	}

      if (natural_size)
	{
	  natural_size->width = nat_width;
	  gtk_widget_get_preferred_height_for_width (widget, nat_width,
                                                     NULL, &natural_size->height);
	}
    }
  else /* GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT or CONSTANT_SIZE */
    {
      gtk_widget_get_preferred_height (widget, &min_height, &nat_height);

      if (minimum_size)
	{
	  minimum_size->height = min_height;
	  gtk_widget_get_preferred_width_for_height (widget, min_height,
                                                     &minimum_size->width, NULL);
	}

      if (natural_size)
	{
	  natural_size->height = nat_height;
	  gtk_widget_get_preferred_width_for_height (widget, nat_height,
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
 * Distributes @extra_space to child @sizes by bringing smaller
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
  guint *spreading;
  gint   i;

  g_return_val_if_fail (extra_space >= 0, 0);

  spreading = g_newa (guint, n_requested_sizes);

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
