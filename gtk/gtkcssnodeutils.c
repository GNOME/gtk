/* GTK - The GIMP Toolkit
 * Copyright (C) 2014 Benjamin Otte <otte@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkcssnodeutilsprivate.h"

#include <math.h>

#include "gtkcssnumbervalueprivate.h"
#include "gtkcssshadowsvalueprivate.h"
#include "gtkcssstyleprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkrenderbackgroundprivate.h"
#include "gtkrenderborderprivate.h"

void
gtk_css_node_style_changed_for_widget (GtkCssNode  *node,
                                       GtkCssStyle *old_style,
                                       GtkCssStyle *new_style,
                                       GtkWidget    *widget)
{
  static GtkBitmask *affects_size = NULL;
  GtkBitmask *changes;
  
  changes = gtk_css_style_get_difference (old_style, new_style);

  if (G_UNLIKELY (affects_size == NULL))
    affects_size = _gtk_css_style_property_get_mask_affecting (GTK_CSS_AFFECTS_SIZE | GTK_CSS_AFFECTS_CLIP);

  if (_gtk_bitmask_intersects (changes, affects_size))
    gtk_widget_queue_resize (widget);
  else
    gtk_widget_queue_draw (widget);

  _gtk_bitmask_free (changes);
}

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
get_content_size_func_default (GtkCssNode            *cssnode,
                               GtkOrientation         orientation,
                               gint                   for_size,
                               gint                  *minimum,
                               gint                  *natural,
                               gint                  *minimum_baseline,
                               gint                  *natural_baseline,
                               gpointer               unused)
{
  *minimum = 0;
  *natural = 0;

  if (minimum_baseline)
    *minimum_baseline = 0;
  if (natural_baseline)
    *natural_baseline = 0;
}

void
gtk_css_node_get_preferred_size (GtkCssNode         *cssnode,
                                 GtkOrientation      orientation,
                                 gint                for_size,
                                 gint               *minimum,
                                 gint               *natural,
                                 gint               *minimum_baseline,
                                 gint               *natural_baseline,
                                 GtkCssNodeSizeFunc  get_content_size_func,
                                 gpointer            get_content_size_data)
{
  GtkCssStyle *style;
  GtkBorder border, padding;
  int min_size, extra_size, extra_opposite, extra_baseline;

  if (!get_content_size_func)
    get_content_size_func = get_content_size_func_default;
  style = gtk_css_node_get_style (cssnode);
  get_box_border (style, &border);
  get_box_padding (style, &padding);
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      extra_size = border.left + border.right + padding.left + padding.right;
      extra_opposite = border.top + border.bottom + padding.top + padding.bottom;
      extra_baseline = border.left + padding.left;
      min_size = get_number (style, GTK_CSS_PROPERTY_MIN_WIDTH);
    }
  else
    {
      extra_size = border.top + border.bottom + padding.top + padding.bottom;
      extra_opposite = border.left + border.right + padding.left + padding.right;
      extra_baseline = border.top + padding.top;
      min_size = get_number (style, GTK_CSS_PROPERTY_MIN_HEIGHT);
    }

  if (for_size > -1)
    for_size -= extra_opposite;

  if (minimum_baseline)
    *minimum_baseline = -1;
  if (natural_baseline)
    *natural_baseline = -1;

  get_content_size_func (cssnode,
                         orientation,
                         for_size,
                         minimum, natural,
                         minimum_baseline, natural_baseline,
                         get_content_size_data);

  g_warn_if_fail (*minimum <= *natural);

  *minimum = MAX (min_size, *minimum);
  *natural = MAX (min_size, *natural);

  *minimum += extra_size;
  *natural += extra_size;

  if (minimum_baseline && *minimum_baseline > -1)
    *minimum_baseline += extra_baseline;
  if (natural_baseline && *natural_baseline > -1)
    *natural_baseline += extra_baseline;
}
                                                         
static void
allocate_func_default (GtkCssNode          *cssnode,
                       const GtkAllocation *allocation,
                       int                  baseline,
                       GtkAllocation       *out_clip,
                       gpointer             unused)
{
  *out_clip = *allocation;
}

void
gtk_css_node_allocate (GtkCssNode             *cssnode,
                       const GtkAllocation    *allocation,
                       int                     baseline,
                       GtkAllocation          *out_clip,
                       GtkCssNodeAllocateFunc  allocate_func,
                       gpointer                allocate_data)
{
  GtkAllocation content_allocation, clip;
  GtkBorder border, padding, shadow, extents;
  GtkCssStyle *style;

  if (out_clip == NULL)
    out_clip = &clip;
  if (!allocate_func)
    allocate_func = allocate_func_default;
  style = gtk_css_node_get_style (cssnode);
  get_box_border (style, &border);
  get_box_padding (style, &padding);
  extents.top = border.top + padding.top;
  extents.right = border.right + padding.right;
  extents.bottom = border.bottom + padding.bottom;
  extents.left = border.left + padding.left;

  content_allocation.x = allocation->x + extents.left;
  content_allocation.y = allocation->y + extents.top;
  content_allocation.width = allocation->width - extents.left - extents.right;
  content_allocation.height = allocation->height - extents.top - extents.bottom;
  if (baseline >= 0)
    baseline += extents.top;

  g_assert (content_allocation.width >= 0);
  g_assert (content_allocation.height >= 0);

  allocate_func (cssnode, &content_allocation, baseline,  out_clip, allocate_data);

  _gtk_css_shadows_value_get_extents (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BOX_SHADOW), &shadow);
  out_clip->x -= extents.left + shadow.left;
  out_clip->y -= extents.top + shadow.top;
  out_clip->width += extents.left + extents.right + shadow.left + shadow.right;
  out_clip->height += extents.top + extents.bottom + shadow.top + shadow.bottom;
}

static gboolean
draw_contents_func_default (GtkCssNode *cssnode,
                            cairo_t    *cr,
                            int         width,
                            int         height,
                            gpointer    unused)
{
  return FALSE;
}

void
gtk_css_node_draw (GtkCssNode         *cssnode,
                   cairo_t            *cr,
                   int                 width,
                   int                 height,
                   GtkCssNodeDrawFunc  draw_contents_func,
                   gpointer            draw_contents_data)
{
  GtkBorder border, padding;
  gboolean draw_focus;
  GtkCssStyle *style;
  int contents_width, contents_height;

  if (draw_contents_func == NULL)
    draw_contents_func = draw_contents_func_default;
  style = gtk_css_node_get_style (cssnode);
  get_box_border (style, &border);
  get_box_padding (style, &padding);

  gtk_css_style_render_background (style, cr, 0, 0, width, height, gtk_css_node_get_junction_sides (cssnode));
  gtk_css_style_render_border (style, cr, 0, 0, width, height, 0, gtk_css_node_get_junction_sides (cssnode));

  cairo_translate (cr,
                   border.left + padding.left,
                   border.top + padding.top);
  contents_width = width - border.left - border.right - padding.left - padding.right;
  contents_height = height - border.top - border.bottom - padding.top - padding.bottom;

  draw_focus = draw_contents_func (cssnode, cr, contents_width, contents_height, draw_contents_data);

  cairo_translate (cr,
                   - (border.left + padding.left),
                   - (border.top + padding.top));

  if (draw_focus)
    gtk_css_style_render_outline (style, cr, 0, 0, width, height);
}


