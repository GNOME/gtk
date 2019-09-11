/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
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

#include "gtkcenterlayout.h"

#include "gtkcsspositionvalueprivate.h"
#include "gtkintl.h"
#include "gtklayoutchild.h"
#include "gtkprivate.h"
#include "gtksizerequest.h"
#include "gtkstylecontextprivate.h"
#include "gtkwidgetprivate.h"

/**
 * SECTION:gtkcenterlayout
 * @Title: GtkCenterLayout
 * @Short_description: A centering layout
 *
 * A #GtkCenterLayout is a layout manager that manages up to three children.
 * The start widget is allocated at the start of the layout (left in LRT
 * layouts and right in RTL ones), and the end widget at the end.
 *
 * The center widget is centered regarding the full width of the layout's.
 */
struct _GtkCenterLayout
{
  GtkLayoutManager parent_instance;

  GtkBaselinePosition baseline_pos;
  GtkOrientation orientation;

  union {
    struct {
      GtkWidget *start_widget;
      GtkWidget *center_widget;
      GtkWidget *end_widget;
    };
    GtkWidget *children[3];
  };
};

G_DEFINE_TYPE (GtkCenterLayout, gtk_center_layout, GTK_TYPE_LAYOUT_MANAGER)

static int
get_spacing (GtkCenterLayout *self,
             GtkStyleContext *style_context)
{
  GtkCssValue *border_spacing;
  int css_spacing;

  border_spacing = _gtk_style_context_peek_property (style_context, GTK_CSS_PROPERTY_BORDER_SPACING);
  if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
    css_spacing = _gtk_css_position_value_get_x (border_spacing, 100);
  else
    css_spacing = _gtk_css_position_value_get_y (border_spacing, 100);

  return css_spacing;
}

static GtkSizeRequestMode
gtk_center_layout_get_request_mode (GtkLayoutManager *layout_manager,
                                    GtkWidget        *widget)
{
  GtkCenterLayout *self = GTK_CENTER_LAYOUT (layout_manager);
  int count[3] = { 0, 0, 0 };

  if (self->start_widget)
    count[gtk_widget_get_request_mode (self->start_widget)]++;

  if (self->center_widget)
    count[gtk_widget_get_request_mode (self->center_widget)]++;

  if (self->end_widget)
    count[gtk_widget_get_request_mode (self->end_widget)]++;

  if (!count[GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH] &&
      !count[GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT])
    return GTK_SIZE_REQUEST_CONSTANT_SIZE;
  else
    return count[GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT] > count[GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH]
           ? GTK_SIZE_REQUEST_WIDTH_FOR_HEIGHT
           : GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}

static gboolean
get_expand (GtkWidget      *widget,
            GtkOrientation  orientation)
{
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    return gtk_widget_get_hexpand (widget);
  else
    return gtk_widget_get_vexpand (widget);
}

static void
gtk_center_layout_distribute (GtkCenterLayout  *self,
                              int               for_size,
                              int               size,
                              int               spacing,
                              GtkRequestedSize *sizes)
{
  int center_size = 0;
  int start_size = 0;
  int end_size = 0;
  gboolean center_expand = FALSE;
  gboolean start_expand = FALSE;
  gboolean end_expand = FALSE;
  int avail;
  int i;

  /* Usable space is really less... */
  size -= spacing * 2;

  sizes[0].minimum_size = sizes[0].natural_size = 0;
  sizes[1].minimum_size = sizes[1].natural_size = 0;
  sizes[2].minimum_size = sizes[2].natural_size = 0;

  for (i = 0; i < 3; i ++)
    {
      if (self->children[i])
        gtk_widget_measure (self->children[i], self->orientation, for_size,
                            &sizes[i].minimum_size, &sizes[i].natural_size,
                            NULL, NULL);
    }

  if (self->center_widget)
    {
      center_size = CLAMP (size - (sizes[0].minimum_size + sizes[2].minimum_size), sizes[1].minimum_size, sizes[1].natural_size);
      center_expand = get_expand (self->center_widget, self->orientation);
    }

  if (self->start_widget)
    {
      avail = MIN ((size - center_size) / 2, size - (center_size + sizes[2].minimum_size));
      start_size = CLAMP (avail, sizes[0].minimum_size, sizes[0].natural_size);
      start_expand = get_expand (self->start_widget, self->orientation);
    }

   if (self->end_widget)
    {
      avail = MIN ((size - center_size) / 2, size - (center_size + sizes[0].minimum_size));
      end_size = CLAMP (avail, sizes[2].minimum_size, sizes[2].natural_size);
      end_expand = get_expand (self->end_widget, self->orientation);
    }

  if (self->center_widget)
    {
      int center_pos;

      center_pos = (size / 2) - (center_size / 2);

      /* Push in from start/end */
      if (start_size > center_pos)
        center_pos = start_size;
      else if (size - end_size < center_pos + center_size)
        center_pos = size - center_size - end_size;
      else if (center_expand)
        {
          center_size = size - 2 * MAX (start_size, end_size);
          center_pos = (size / 2) - (center_size / 2);
        }

      if (start_expand)
        start_size = center_pos;

      if (end_expand)
        end_size = size - (center_pos + center_size);
    }
  else
    {
      avail = size - (start_size + end_size);
      if (start_expand && end_expand)
        {
          start_size += avail / 2;
          end_size += avail / 2;
        }
      else if (start_expand)
        {
          start_size += avail;
        }
      else if (end_expand)
        {
          end_size += avail;
        }
    }

  sizes[0].minimum_size = start_size;
  sizes[1].minimum_size = center_size;
  sizes[2].minimum_size = end_size;
}

static void
gtk_center_layout_measure_orientation (GtkCenterLayout *self,
                                       GtkWidget       *widget,
                                       GtkOrientation   orientation,
                                       int              for_size,
                                       int             *minimum,
                                       int             *natural,
                                       int             *minimum_baseline,
                                       int             *natural_baseline)
{
  int min[3];
  int nat[3];
  int n_visible_children = 0;
  int spacing;
  int i;

  spacing = get_spacing (self, _gtk_widget_get_style_context (widget));

  for (i = 0; i < 3; i ++)
    {
      GtkWidget *child = self->children[i];

      if (child)
        {
          gtk_widget_measure (child,
                              orientation,
                              for_size,
                              &min[i], &nat[i], NULL, NULL);

          if (_gtk_widget_get_visible (child))
            n_visible_children ++;
        }
      else
        {
          min[i] = 0;
          nat[i] = 0;
        }
    }

  *minimum = min[0] + min[1] + min[2];
  *natural = nat[1] + 2 * MAX (nat[0], nat[2]);

  if (n_visible_children > 0)
    {
      *minimum += (n_visible_children - 1) * spacing;
      *natural += (n_visible_children - 1) * spacing;
    }
}

static void
gtk_center_layout_measure_opposite (GtkCenterLayout *self,
                                    GtkOrientation   orientation,
                                    int              for_size,
                                    int             *minimum,
                                    int             *natural,
                                    int             *minimum_baseline,
                                    int             *natural_baseline)
{
  int child_min, child_nat;
  int child_min_baseline, child_nat_baseline;
  int total_min, above_min, below_min;
  int total_nat, above_nat, below_nat;
  GtkWidget *child[3];
  GtkRequestedSize sizes[3];
  int i;

  child[0] = self->start_widget;
  child[1] = self->center_widget;
  child[2] = self->end_widget;

  if (for_size >= 0)
    gtk_center_layout_distribute (self, -1, for_size, 0, sizes);

  above_min = below_min = above_nat = below_nat = -1;
  total_min = total_nat = 0;

  for (i = 0; i < 3; i++)
    {
      if (child[i] == NULL)
        continue;

      gtk_widget_measure (child[i],
                          orientation,
                          for_size >= 0 ? sizes[i].minimum_size : -1,
                          &child_min, &child_nat,
                          &child_min_baseline, &child_nat_baseline);

      if (child_min_baseline >= 0)
        {
          below_min = MAX (below_min, child_min - child_min_baseline);
          above_min = MAX (above_min, child_min_baseline);
          below_nat = MAX (below_nat, child_nat - child_nat_baseline);
          above_nat = MAX (above_nat, child_nat_baseline);
        }
      else
        {
          total_min = MAX (total_min, child_min);
          total_nat = MAX (total_nat, child_nat);
        }
   }

  if (above_min >= 0)
    {
      int min_baseline = -1;
      int nat_baseline = -1;

      total_min = MAX (total_min, above_min + below_min);
      total_nat = MAX (total_nat, above_nat + below_nat);

      switch (self->baseline_pos)
        {
        case GTK_BASELINE_POSITION_TOP:
          min_baseline = above_min;
          nat_baseline = above_nat;
          break;
        case GTK_BASELINE_POSITION_CENTER:
          min_baseline = above_min + (total_min - (above_min + below_min)) / 2;
          nat_baseline = above_nat + (total_nat - (above_nat + below_nat)) / 2;
          break;
        case GTK_BASELINE_POSITION_BOTTOM:
          min_baseline = total_min - below_min;
          nat_baseline = total_nat - below_nat;
          break;
        default:
          break;
        }

      if (minimum_baseline)
        *minimum_baseline = min_baseline;
      if (natural_baseline)
        *natural_baseline = nat_baseline;
    }

  *minimum = total_min;
  *natural = total_nat;
}



static void
gtk_center_layout_measure (GtkLayoutManager *layout_manager,
                           GtkWidget        *widget,
                           GtkOrientation    orientation,
                           int               for_size,
                           int              *minimum,
                           int              *natural,
                           int              *minimum_baseline,
                           int              *natural_baseline)
{
  GtkCenterLayout *self = GTK_CENTER_LAYOUT (layout_manager);

  if (self->orientation == orientation)
    gtk_center_layout_measure_orientation (self, widget, orientation, for_size,
                                           minimum, natural, minimum_baseline, natural_baseline);
  else
    gtk_center_layout_measure_opposite (self, orientation, for_size,
                                        minimum, natural, minimum_baseline, natural_baseline);
}

static void
gtk_center_layout_allocate (GtkLayoutManager *layout_manager,
                            GtkWidget        *widget,
                            int               width,
                            int               height,
                            int               baseline)
{
  GtkCenterLayout *self = GTK_CENTER_LAYOUT (layout_manager);
  GtkWidget *child[3];
  int child_size[3];
  int child_pos[3];
  GtkRequestedSize sizes[3];
  int size;
  int for_size;
  int i;
  int spacing;

  spacing = get_spacing (self, _gtk_widget_get_style_context (widget));

  if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      size = width;
      for_size = height;
    }
  else
    {
      size = height;
      for_size = width;
      baseline = -1;
    }

  /* Allocate child sizes */

  gtk_center_layout_distribute (self, for_size, size, spacing, sizes);

  child[1] = self->center_widget;
  child_size[1] = sizes[1].minimum_size;

  if (self->orientation == GTK_ORIENTATION_HORIZONTAL &&
      gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    {
      child[0] = self->end_widget;
      child[2] = self->start_widget;
      child_size[0] = sizes[2].minimum_size;
      child_size[2] = sizes[0].minimum_size;
    }
  else
    {
      child[0] = self->start_widget;
      child[2] = self->end_widget;
      child_size[0] = sizes[0].minimum_size;
      child_size[2] = sizes[2].minimum_size;
    }

  /* Determine baseline */
  if (self->orientation == GTK_ORIENTATION_HORIZONTAL &&
      baseline == -1)
    {
      int min_above, nat_above;
      int min_below, nat_below;
      gboolean have_baseline;

      have_baseline = FALSE;
      min_above = nat_above = 0;
      min_below = nat_below = 0;

      for (i = 0; i < 3; i++)
        {
          if (child[i] && gtk_widget_get_valign (child[i]) == GTK_ALIGN_BASELINE)
            {
              int child_min_height, child_nat_height;
              int child_min_baseline, child_nat_baseline;

              child_min_baseline = child_nat_baseline = -1;

              gtk_widget_measure (child[i], GTK_ORIENTATION_VERTICAL,
                                  child_size[i],
                                  &child_min_height, &child_nat_height,
                                  &child_min_baseline, &child_nat_baseline);

              if (child_min_baseline >= 0)
                {
                  have_baseline = TRUE;
                  min_below = MAX (min_below, child_min_height - child_min_baseline);
                  nat_below = MAX (nat_below, child_nat_height - child_nat_baseline);
                  min_above = MAX (min_above, child_min_baseline);
                  nat_above = MAX (nat_above, child_nat_baseline);
                }
            }
        }

      if (have_baseline)
        {
          /* TODO: This is purely based on the minimum baseline.
           * When things fit we should use the natural one
           */
          switch (self->baseline_pos)
            {
            default:
            case GTK_BASELINE_POSITION_TOP:
              baseline = min_above;
              break;
            case GTK_BASELINE_POSITION_CENTER:
              baseline = min_above + (height - (min_above + min_below)) / 2;
              break;
            case GTK_BASELINE_POSITION_BOTTOM:
              baseline = height - min_below;
              break;
            }
        }
    }

  /* Allocate child positions */

  child_pos[0] = 0;
  child_pos[1] = (size / 2) - (child_size[1] / 2);
  child_pos[2] = size - child_size[2];

  if (child[1])
    {
      /* Push in from start/end */
      if (child_size[0] > child_pos[1])
        child_pos[1] = child_size[0];
      else if (size - child_size[2] < child_pos[1] + child_size[1])
        child_pos[1] = size - child_size[1] - child_size[2];
    }

  for (i = 0; i < 3; i++)
    {
      GtkAllocation child_allocation;

      if (child[i] == NULL)
        continue;

      if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          child_allocation.x = child_pos[i];
          child_allocation.y = 0;
          child_allocation.width = child_size[i];
          child_allocation.height = height;
        }
      else
        {
          child_allocation.x = 0;
          child_allocation.y = child_pos[i];
          child_allocation.width = width;
          child_allocation.height = child_size[i];
        }

      gtk_widget_size_allocate (child[i], &child_allocation, baseline);
    }
}

static void
gtk_center_layout_class_init (GtkCenterLayoutClass *klass)
{
  GtkLayoutManagerClass *layout_class = GTK_LAYOUT_MANAGER_CLASS (klass);

  layout_class->get_request_mode = gtk_center_layout_get_request_mode;
  layout_class->measure = gtk_center_layout_measure;
  layout_class->allocate = gtk_center_layout_allocate;
}

static void
gtk_center_layout_init (GtkCenterLayout *self)
{
  self->orientation = GTK_ORIENTATION_HORIZONTAL;
  self->baseline_pos = GTK_BASELINE_POSITION_CENTER;
}

/**
 * gtk_center_layout_new:
 *
 * Creates a new #GtkCenterLayout.
 *
 * Returns: the newly created #GtkCenterLayout
 */
GtkLayoutManager *
gtk_center_layout_new (void)
{
  return g_object_new (GTK_TYPE_CENTER_LAYOUT, NULL);
}

/**
 * gtk_center_layout_set_orientation:
 * @self: a #GtkCenterLayout
 * @orientation: the new orientation
 *
 * Sets the orientation of @self.
 */
void
gtk_center_layout_set_orientation (GtkCenterLayout *self,
                                   GtkOrientation   orientation)
{
  if (orientation != self->orientation)
    {
      self->orientation = orientation;
      gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (self));
    }
}

/**
 * gtk_center_layout_get_orientation:
 * @self: a #GtkCenterLayout
 *
 *
 * Returns: The current orientation of @self
 */
GtkOrientation
gtk_center_layout_get_orientation (GtkCenterLayout *self)
{
  return self->orientation;
}

/**
 * gtk_center_layout_set_baseline_position:
 * @self: a #GtkCenterLayout
 * @baseline_position: the new baseline position
 *
 * Sets the new baseline position of @self
 */
void
gtk_center_layout_set_baseline_position (GtkCenterLayout     *self,
                                         GtkBaselinePosition  baseline_position)
{
  if (baseline_position != self->baseline_pos)
    {
      self->baseline_pos = baseline_position;
      gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (self));
    }
}

/**
 * gtk_center_layout_get_baseline_position:
 * @self: a #GtkCenterLayout
 *
 * Returns: The current baseline position of @self.
 */
GtkBaselinePosition
gtk_center_layout_get_baseline_position (GtkCenterLayout *self)
{
  return self->baseline_pos;
}

/**
 * gtk_center_layout_set_start_widget:
 * @self: a #GtkCenterLayout
 * @widget: the new start widget
 *
 * Sets the new start widget of @self.
 */
void
gtk_center_layout_set_start_widget (GtkCenterLayout *self,
                                    GtkWidget       *widget)
{
  self->start_widget = widget;
}

/**
 * gtk_center_layout_get_start_widget:
 * @self: a #GtkCenterLayout
 *
 * Returns: (transfer none): The current start widget of @self
 */
GtkWidget *
gtk_center_layout_get_start_widget (GtkCenterLayout *self)
{
  return self->start_widget;
}

/**
 * gtk_center_layout_set_center_widget:
 * @self: a #GtkCenterLayout
 * @widget: the new center widget
 *
 * Sets the new center widget of @self
 */
void
gtk_center_layout_set_center_widget (GtkCenterLayout *self,
                                     GtkWidget       *widget)
{
  self->center_widget = widget;
}

/**
 * gtk_center_layout_get_center_widget:
 * @self: a #GtkCenterLayout
 *
 * Returns: (transfer none): the current center widget of @self
 */
GtkWidget *
gtk_center_layout_get_center_widget (GtkCenterLayout *self)
{
  return self->center_widget;
}

/**
 * gtk_center_layout_set_end_widget:
 * @self: a #GtkCenterLayout
 * @widget: (transfer none): the new end widget
 *
 * Sets the new end widget of @self
 */
void
gtk_center_layout_set_end_widget (GtkCenterLayout *self,
                                  GtkWidget       *widget)
{
  self->end_widget = widget;
}

/**
 * gtk_center_layout_get_end_widget:
 * @self: a #GtkCenterLayout
 *
 * Returns: (transfer none): the current end widget of @self
 */
GtkWidget *
gtk_center_layout_get_end_widget (GtkCenterLayout *self)
{
  return self->end_widget;
}
