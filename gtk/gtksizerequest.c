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
#include "gtkcssnodeprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkbutton.h"


#ifdef G_ENABLE_CONSISTENCY_CHECKS
static GQuark recursion_check_quark = 0;

static void
push_recursion_check (GtkWidget       *widget,
                      GtkOrientation   orientation,
                      gint             for_size)
{
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
}

static void
pop_recursion_check (GtkWidget       *widget,
                     GtkOrientation   orientation)
{
  g_object_set_qdata (G_OBJECT (widget), recursion_check_quark, NULL);
}
#else
#define push_recursion_check(widget, orientation, for_size)
#define pop_recursion_check(widget, orientation)
#endif /* G_ENABLE_CONSISTENCY_CHECKS */

static gint
get_number (GtkCssStyle *style,
            guint        property)
{
  double d = _gtk_css_number_value_get (gtk_css_style_get_value (style, property), 100);

  if (d < 1)
    return ceil (d);
  else
    return floor (d);
}

static void
get_box_margin (GtkCssStyle *style,
                GtkBorder   *margin)
{
  margin->top = get_number (style, GTK_CSS_PROPERTY_MARGIN_TOP);
  margin->left = get_number (style, GTK_CSS_PROPERTY_MARGIN_LEFT);
  margin->bottom = get_number (style, GTK_CSS_PROPERTY_MARGIN_BOTTOM);
  margin->right = get_number (style, GTK_CSS_PROPERTY_MARGIN_RIGHT);
}

static void
get_box_border (GtkCssStyle *style,
                GtkBorder   *border)
{
  border->top = get_number (style, GTK_CSS_PROPERTY_BORDER_TOP_WIDTH);
  border->left = get_number (style, GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH);
  border->bottom = get_number (style, GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH);
  border->right = get_number (style, GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH);
}

static void
get_box_padding (GtkCssStyle *style,
                 GtkBorder   *border)
{
  border->top = get_number (style, GTK_CSS_PROPERTY_PADDING_TOP);
  border->left = get_number (style, GTK_CSS_PROPERTY_PADDING_LEFT);
  border->bottom = get_number (style, GTK_CSS_PROPERTY_PADDING_BOTTOM);
  border->right = get_number (style, GTK_CSS_PROPERTY_PADDING_RIGHT);
}

static void
gtk_widget_query_size_for_orientation (GtkWidget        *widget,
                                       GtkOrientation    orientation,
                                       gint              for_size,
                                       gint             *minimum_size,
                                       gint             *natural_size,
                                       gint             *minimum_baseline,
                                       gint             *natural_baseline)
{
  SizeRequestCache *cache;
  GtkWidgetClass *widget_class;
  gint min_size = 0;
  gint nat_size = 0;
  gint min_baseline = -1;
  gint nat_baseline = -1;
  gboolean found_in_cache;

  gtk_widget_ensure_resize (widget);

  if (gtk_widget_get_request_mode (widget) == GTK_SIZE_REQUEST_CONSTANT_SIZE)
    for_size = -1;

  cache = _gtk_widget_peek_request_cache (widget);
  found_in_cache = _gtk_size_request_cache_lookup (cache,
                                                   orientation,
                                                   for_size,
                                                   &min_size,
                                                   &nat_size,
                                                   &min_baseline,
                                                   &nat_baseline);

  widget_class = GTK_WIDGET_GET_CLASS (widget);
  
  if (!found_in_cache)
    {
      gint adjusted_min, adjusted_natural, adjusted_for_size = for_size;

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          if (for_size < 0)
            {
	      push_recursion_check (widget, orientation, for_size);
              widget_class->measure (widget, GTK_ORIENTATION_HORIZONTAL, -1,
                                     &min_size, &nat_size, NULL, NULL);
	      pop_recursion_check (widget, orientation);
            }
          else
            {
              gint minimum_height;
              gint natural_height;
              int dummy;

              /* Pull the base natural height from the cache as it's needed to adjust
               * the proposed 'for_size' */
              widget_class->measure (widget, GTK_ORIENTATION_VERTICAL, -1,
                                     &minimum_height, &natural_height, &dummy, &dummy);

              /* convert for_size to unadjusted height (for_size is a proposed allocation) */
              gtk_widget_adjust_size_allocation (widget,
                                                 GTK_ORIENTATION_VERTICAL,
                                                 &minimum_height,
                                                 &natural_height,
                                                 &dummy,
                                                 &adjusted_for_size);

	      push_recursion_check (widget, orientation, for_size);
              widget_class->measure (widget,
                                     GTK_ORIENTATION_HORIZONTAL,
                                     MAX (adjusted_for_size, minimum_height),
                                     &min_size, &nat_size,
                                     NULL, NULL);
	      pop_recursion_check (widget, orientation);
            }
        }
      else
        {
          if (for_size < 0)
            {
	      push_recursion_check (widget, orientation, for_size);
              widget_class->measure (widget,
                                     GTK_ORIENTATION_VERTICAL,
                                     -1,
                                     &min_size, &nat_size,
                                     &min_baseline, &nat_baseline);
	      pop_recursion_check (widget, orientation);
            }
          else
            {
              gint minimum_width;
              gint natural_width;
              int dummy;

              /* Pull the base natural width from the cache as it's needed to adjust
               * the proposed 'for_size' */
              widget_class->measure (widget, GTK_ORIENTATION_HORIZONTAL, -1,
                                     &minimum_width, &natural_width, &dummy, &dummy);

              /* convert for_size to unadjusted width (for_size is a proposed allocation) */
              gtk_widget_adjust_size_allocation (widget,
                                                 GTK_ORIENTATION_HORIZONTAL,
                                                 &minimum_width,
                                                 &natural_width,
                                                 &dummy,
                                                 &adjusted_for_size);

	      push_recursion_check (widget, orientation, for_size);
              widget_class->measure (widget,
                                     GTK_ORIENTATION_VERTICAL,
                                     MAX (adjusted_for_size, minimum_width),
                                     &min_size, &nat_size,
                                     &min_baseline, &nat_baseline);
	      pop_recursion_check (widget, orientation);
            }
        }

      if (G_UNLIKELY (min_size > nat_size))
        {
          if (orientation == GTK_ORIENTATION_HORIZONTAL)
            {
              g_warning ("%s %p reported min width %d and natural width %d in measure() with for_size=%d; natural size must be >= min size",
                         G_OBJECT_TYPE_NAME (widget), widget, min_size, nat_size, for_size);
            }
          else
            {
              g_warning ("%s %p reported min height %d and natural height %d in measure() with for_size=%d; natural size must be >= min size",
                         G_OBJECT_TYPE_NAME (widget), widget, min_size, nat_size, for_size);

            }
        }

      adjusted_min     = min_size;
      adjusted_natural = nat_size;
      gtk_widget_adjust_size_request (widget, orientation,
                                      &adjusted_min, &adjusted_natural);

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

      if (min_baseline != -1 || nat_baseline != -1)
	{
	  if (orientation == GTK_ORIENTATION_HORIZONTAL)
	    {
	      g_warning ("%s %p reported a horizontal baseline",
			 G_OBJECT_TYPE_NAME (widget), widget);
	      min_baseline = -1;
	      nat_baseline = -1;
	    }
	  else if (min_baseline == -1 || nat_baseline == -1)
	    {
	      g_warning ("%s %p reported baseline for only one of min/natural (min: %d, natural: %d)",
			 G_OBJECT_TYPE_NAME (widget), widget,
			 min_baseline, nat_baseline);
	      min_baseline = -1;
	      nat_baseline = -1;
	    }
	  else if (gtk_widget_get_valign (widget) != GTK_ALIGN_BASELINE)
	    {
	      /* Ignore requested baseline for non-aligned widgets */
	      min_baseline = -1;
	      nat_baseline = -1;
	    }
	  else
            gtk_widget_adjust_baseline_request (widget, &min_baseline, &nat_baseline);
	}

      _gtk_size_request_cache_commit (cache,
                                      orientation,
                                      for_size,
                                      min_size,
                                      nat_size,
				      min_baseline,
				      nat_baseline);
    }

  if (minimum_size)
    *minimum_size = min_size;

  if (natural_size)
    *natural_size = nat_size;

  if (minimum_baseline)
    *minimum_baseline = min_baseline;

  if (natural_baseline)
    *natural_baseline = nat_baseline;

  g_assert (min_size <= nat_size);

  GTK_NOTE (SIZE_REQUEST, {
            GString *s;

            s = g_string_new ("");
            g_string_append_printf (s, "[%p] %s\t%s: %d is minimum %d and natural: %d",
                                    widget, G_OBJECT_TYPE_NAME (widget),
                                    orientation == GTK_ORIENTATION_HORIZONTAL
                                    ? "width for height"
                                    : "height for width",
                                    for_size, min_size, nat_size);
	    if (min_baseline != -1 || nat_baseline != -1)
              {
                g_string_append_printf (s, ", baseline %d/%d",
                                        min_baseline, nat_baseline);
              }
	    g_string_append_printf (s, " (hit cache: %s)\n",
		                    found_in_cache ? "yes" : "no");
            g_message ("%s", s->str);
            g_string_free (s, TRUE);
	    });
}

/**
 * gtk_widget_measure:
 * @widget: A #GtkWidget instance
 * @orientation: the orientation to measure
 * @for_size: Size for the opposite of @orientation, i.e.
 *   if @orientation is %GTK_ORIENTATION_HORIZONTAL, this is
 *   the height the widget should be measured with. The %GTK_ORIENTATION_VERTICAL
 *   case is analogous. This way, both height-for-width and width-for-height
 *   requests can be implemented. If no size is known, -1 can be passed.
 * @minimum: (out) (optional): location to store the minimum size, or %NULL
 * @natural: (out) (optional): location to store the natural size, or %NULL
 * @minimum_baseline: (out) (optional): location to store the baseline
 *   position for the minimum size, or %NULL
 * @natural_baseline: (out) (optional): location to store the baseline
 *   position for the natural size, or %NULL
 *
 * Measures @widget in the orientation @orientation and for the given @for_size.
 * As an example, if @orientation is GTK_ORIENTATION_HORIZONTAL and @for_size is 300,
 * this functions will compute the minimum and natural width of @widget if
 * it is allocated at a height of 300 pixels.
 *
 * Since: 3.90
 */
void
gtk_widget_measure (GtkWidget        *widget,
                    GtkOrientation    orientation,
                    int               for_size,
                    int              *minimum,
                    int              *natural,
                    int              *minimum_baseline,
                    int              *natural_baseline)
{
  GHashTable *widgets;
  GHashTableIter iter;
  gpointer key;
  gint    min_result = 0, nat_result = 0;
  GtkCssStyle *style;
  GtkBorder margin, border, padding;
  int css_min_size;
  int css_min_for_size;
  int css_extra_for_size;
  int css_extra_size;

  g_return_if_fail (GTK_IS_WIDGET (widget));
  g_return_if_fail (for_size >= -1);
  g_return_if_fail (orientation == GTK_ORIENTATION_HORIZONTAL ||
                    orientation == GTK_ORIENTATION_VERTICAL);

  /* This is the main function that checks for a cached size and
   * possibly queries the widget class to compute the size if it's
   * not cached.
   */
  if (!_gtk_widget_get_visible (widget) && !_gtk_widget_is_toplevel (widget))
    {
      if (minimum)
        *minimum = 0;
      if (natural)
        *natural = 0;
      if (minimum_baseline)
        *minimum_baseline = -1;
      if (natural_baseline)
        *natural_baseline = -1;
      return;
    }

  /* The passed for_size is for the widget allocation, but we want to pass down the for_size
   * of the content allocation, so remove margin, border and padding from the for_size,
   * pass that down to gtk_widget_query_size_for_orientation and then take the
   * retrieved values and add margin, border and padding again as well as MAX it with the
   * CSS min-width/min-height properties. */
  style = gtk_css_node_get_style (gtk_widget_get_css_node (widget));
  get_box_margin (style, &margin);
  get_box_border (style, &border);
  get_box_padding (style, &padding);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      css_extra_size = margin.left + margin.right + border.left + border.right + padding.left + padding.right;
      css_extra_for_size = margin.top + margin.bottom + border.top + border.bottom + padding.top + padding.bottom;
      css_min_size = get_number (style, GTK_CSS_PROPERTY_MIN_WIDTH);
      css_min_for_size = get_number (style, GTK_CSS_PROPERTY_MIN_HEIGHT);
    }
  else
    {
      css_extra_size = margin.top + margin.bottom + border.top + border.bottom + padding.top + padding.bottom;
      css_extra_for_size = margin.left + margin.right + border.left + border.right + padding.left + padding.right;
      css_min_size = get_number (style, GTK_CSS_PROPERTY_MIN_HEIGHT);
      css_min_for_size = get_number (style, GTK_CSS_PROPERTY_MIN_WIDTH);
    }

  /* TODO: Baselines */
  /* TODO: The GtkCssGadget code has a warning for for_size < min_for_size
   *       where min_for_size depends on the css values */
  if (for_size > -1)
    for_size = MAX (for_size - css_extra_for_size, css_min_for_size);


  if (G_LIKELY (!_gtk_widget_get_sizegroups (widget)))
    {
      gtk_widget_query_size_for_orientation (widget, orientation, for_size, minimum, natural,
                                             minimum_baseline, natural_baseline);

      if (minimum)
        *minimum = MAX (0, MAX (*minimum, css_min_size) + css_extra_size);

      if (natural)
        *natural = MAX (0, MAX (*natural, css_min_size) + css_extra_size);
      /* TODO: Baselines! */

      return;
    }

  widgets = _gtk_size_group_get_widget_peers (widget, orientation);

  g_hash_table_iter_init (&iter, widgets);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      GtkWidget *tmp_widget = key;
      gint min_dimension, nat_dimension;

      style = gtk_css_node_get_style (gtk_widget_get_css_node (tmp_widget));
      get_box_margin (style, &margin);
      get_box_border (style, &border);
      get_box_padding (style, &padding);

      if (orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          css_extra_size = margin.left + margin.right + border.left + border.right + padding.left + padding.right;
          css_min_size = get_number (style, GTK_CSS_PROPERTY_MIN_WIDTH);
        }
      else
        {
          css_extra_size = margin.top + margin.bottom + border.top + border.bottom + padding.top + padding.bottom;
          css_min_size = get_number (style, GTK_CSS_PROPERTY_MIN_HEIGHT);
        }

      gtk_widget_query_size_for_orientation (tmp_widget, orientation, for_size, &min_dimension, &nat_dimension, NULL, NULL);

      min_result = MAX (0, MAX (min_result, MAX (min_dimension, css_min_size) + css_extra_size));
      nat_result = MAX (0, MAX (nat_result, MAX (nat_dimension, css_min_size) + css_extra_size));
    }

  g_hash_table_destroy (widgets);

  /* Baselines make no sense with sizegroups really */
  if (minimum_baseline)
    *minimum_baseline = -1;

  if (natural_baseline)
    *natural_baseline = -1;

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
 * #GtkBin widgets generally propagate the preference of
 * their child, container widgets need to request something either in
 * context of their children or in context of their allocation
 * capabilities.
 *
 * Returns: The #GtkSizeRequestMode preferred by @widget.
 *
 * Since: 3.0
 */
GtkSizeRequestMode
gtk_widget_get_request_mode (GtkWidget *widget)
{
  SizeRequestCache *cache;

  cache = _gtk_widget_peek_request_cache (widget);

  if (G_UNLIKELY (!cache->request_mode_valid))
    {
      cache->request_mode = GTK_WIDGET_GET_CLASS (widget)->get_request_mode (widget);
      cache->request_mode_valid = TRUE;
    }

  return cache->request_mode;
}

/*
 * _gtk_widget_get_preferred_size_and_baseline:
 * @widget: a #GtkWidget instance
 * @minimum_size: (out) (allow-none): location for storing the minimum size, or %NULL
 * @natural_size: (out) (allow-none): location for storing the natural size, or %NULL
 *
 * Retrieves the minimum and natural size  and the corresponding baselines of a widget, taking
 * into account the widget’s preference for height-for-width management. The baselines may
 * be -1 which means that no baseline is requested for this widget.
 *
 * This is used to retrieve a suitable size by container widgets which do
 * not impose any restrictions on the child placement. It can be used
 * to deduce toplevel window and menu sizes as well as child widgets in
 * free-form containers such as GtkLayout.
 *
 * Handle with care. Note that the natural height of a height-for-width
 * widget will generally be a smaller size than the minimum height, since the required
 * height for the natural width is generally smaller than the required height for
 * the minimum width.
 */
void
_gtk_widget_get_preferred_size_and_baseline (GtkWidget      *widget,
                                             GtkRequisition *minimum_size,
                                             GtkRequisition *natural_size,
                                             gint           *minimum_baseline,
                                             gint           *natural_baseline)
{
  gint min_width, nat_width;
  gint min_height, nat_height;

  g_return_if_fail (GTK_IS_WIDGET (widget));

  if (gtk_widget_get_request_mode (widget) == GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH)
    {
      gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, -1,
                          &min_width, &nat_width, NULL, NULL);

      if (minimum_size)
	{
	  minimum_size->width = min_width;
          gtk_widget_measure (widget,
                              GTK_ORIENTATION_VERTICAL, min_width,
                              &minimum_size->height, NULL, minimum_baseline, NULL);
	}

      if (natural_size)
	{
	  natural_size->width = nat_width;
          gtk_widget_measure (widget,
                              GTK_ORIENTATION_VERTICAL, nat_width,
                              NULL, &natural_size->height, NULL, natural_baseline);
	}
    }
  else /* GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT or CONSTANT_SIZE */
    {
      gtk_widget_measure (widget, GTK_ORIENTATION_VERTICAL,
                          -1, &min_height, &nat_height, minimum_baseline, natural_baseline);

      if (minimum_size)
	{
	  minimum_size->height = min_height;
          gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, min_height,
                              &minimum_size->width, NULL, NULL, NULL);
	}

      if (natural_size)
	{
	  natural_size->height = nat_height;
          gtk_widget_measure (widget, GTK_ORIENTATION_HORIZONTAL, nat_height,
                              NULL, &natural_size->width, NULL, NULL);
	}
    }
}

/**
 * gtk_widget_get_preferred_size:
 * @widget: a #GtkWidget instance
 * @minimum_size: (out) (allow-none): location for storing the minimum size, or %NULL
 * @natural_size: (out) (allow-none): location for storing the natural size, or %NULL
 *
 * Retrieves the minimum and natural size of a widget, taking
 * into account the widget’s preference for height-for-width management.
 *
 * This is used to retrieve a suitable size by container widgets which do
 * not impose any restrictions on the child placement. It can be used
 * to deduce toplevel window and menu sizes as well as child widgets in
 * free-form containers such as GtkLayout.
 *
 * Handle with care. Note that the natural height of a height-for-width
 * widget will generally be a smaller size than the minimum height, since the required
 * height for the natural width is generally smaller than the required height for
 * the minimum width.
 *
 * Use gtk_widget_measure() if you want to support
 * baseline alignment.
 *
 * Since: 3.0
 */
void
gtk_widget_get_preferred_size (GtkWidget      *widget,
                               GtkRequisition *minimum_size,
                               GtkRequisition *natural_size)
{
  _gtk_widget_get_preferred_size_and_baseline (widget, minimum_size, natural_size,
                                               NULL, NULL);
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
