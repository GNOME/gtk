/* gtkcenterlayout.c: Center layout manager
 *
 * Copyright 2019  GNOME Foundation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkcenterlayout.h"

#include "gtkcsspositionvalueprivate.h"
#include "gtkintl.h"
#include "gtkorientable.h"
#include "gtkprivate.h"
#include "gtksizerequest.h"
#include "gtkstylecontextprivate.h"
#include "gtktypebuiltins.h"
#include "gtkwidgetprivate.h"

/**
 * SECTION:gtkcenterlayout
 * @Title: GtkCenterLayout
 * @Short_description: Layout manager for placing all children in a single row or column
 *
 * A GtkCenterLayout is a layout manager that arranges the children of any
 * widget using it into a single row or column, depending on the value
 * of its #GtkOrientable:orientation property. Within the other dimension
 * all children all allocated the same size. The GtkCenterLayout will respect
 * the #GtkWidget:halign and #GtkWidget:valign properties of each child
 * widget.
 *
 * If you want all children to be assigned the same size, you can use
 * the #GtkCenterLayout:homogeneous property.
 *
 * If you want to specify the amount of space placed between each child,
 * you can use the #GtkCenterLayout:spacing property.
 */

struct _GtkCenterLayout
{
  GtkLayoutManager parent_instance;

  GtkOrientation orientation;
  GtkBaselinePosition baseline_position;

  GtkWidget *start_widget;
  GtkWidget *center_widget;
  GtkWidget *end_widget;
};

G_DEFINE_TYPE_WITH_CODE (GtkCenterLayout, gtk_center_layout, GTK_TYPE_LAYOUT_MANAGER,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL))

enum {
  PROP_BASELINE_POSITION = 1,

  /* From GtkOrientable */
  PROP_ORIENTATION,

  N_PROPS = PROP_ORIENTATION
};

static GParamSpec *center_layout_props[N_PROPS];

static void
gtk_center_layout_set_property (GObject      *gobject,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GtkCenterLayout *self = GTK_CENTER_LAYOUT (gobject);

  switch (prop_id)
    {
    case PROP_BASELINE_POSITION:
      gtk_center_layout_set_baseline_position (self, g_value_get_enum (value));
      break;

    /* We cannot call gtk_orientable_set_orientation(), because
     * that will just call g_object_set() on this property
     */
    case PROP_ORIENTATION:
      {
        GtkOrientation new_orientation = g_value_get_enum (value);

        if (self->orientation != new_orientation)
          {
            self->orientation = new_orientation;

            gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (self));
            g_object_notify (gobject, "orientation");
          }
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_center_layout_get_property (GObject    *gobject,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GtkCenterLayout *center_layout = GTK_CENTER_LAYOUT (gobject);

  switch (prop_id)
    {
    case PROP_BASELINE_POSITION:
      g_value_set_enum (value, center_layout->baseline_position);
      break;

    case PROP_ORIENTATION:
      g_value_set_enum (value, center_layout->orientation);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static GtkSizeRequestMode
gtk_center_layout_get_request_mode (GtkLayoutManager *layout_manager,
                                 GtkWidget        *widget)
{
  GtkCenterLayout *self = GTK_CENTER_LAYOUT (layout_manager);
  gint count[3] = { 0, 0, 0 };

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
                              gint              for_size,
                              gint              size,
                              GtkRequestedSize *sizes)
{
  int center_size = 0;
  int start_size = 0;
  int end_size = 0;
  gboolean center_expand = FALSE;
  gboolean start_expand = FALSE;
  gboolean end_expand = FALSE;
  int avail;

  sizes[0].minimum_size = sizes[0].natural_size = 0;
  sizes[1].minimum_size = sizes[1].natural_size = 0;
  sizes[2].minimum_size = sizes[2].natural_size = 0;

  if (self->start_widget)
    gtk_widget_measure (self->start_widget,
                        self->orientation,
                        for_size,
                        &sizes[0].minimum_size, &sizes[0].natural_size,
                        NULL, NULL);

  if (self->center_widget)
    gtk_widget_measure (self->center_widget,
                        self->orientation,
                        for_size,
                        &sizes[1].minimum_size, &sizes[1].natural_size,
                        NULL, NULL);


  if (self->end_widget)
    gtk_widget_measure (self->end_widget,
                        self->orientation,
                        for_size,
                        &sizes[2].minimum_size, &sizes[2].natural_size,
                        NULL, NULL);

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
gtk_center_layout_compute_size (GtkCenterLayout  *self,
                                GtkWidget        *widget,
                                int               for_size,
                                int              *minimum,
                                int              *natural)
{
  int min_baseline, nat_baseline;
  int start_min = 0;
  int start_nat = 0;
  int center_min = 0;
  int center_nat = 0;
  int end_min = 0;
  int end_nat = 0;

  if (self->start_widget)
    gtk_widget_measure (self->start_widget,
                        self->orientation,
                        for_size,
                        &start_min, &start_nat,
                        &min_baseline, &nat_baseline);

  if (self->center_widget)
    gtk_widget_measure (self->center_widget,
                        self->orientation,
                        for_size,
                        &center_min, &center_nat,
                        &min_baseline, &nat_baseline);

  if (self->end_widget)
    gtk_widget_measure (self->end_widget,
                        self->orientation,
                        for_size,
                        &end_min, &end_nat,
                        &min_baseline, &nat_baseline);

  *minimum = start_min + center_min + end_min;
  *natural = center_nat + 2 * MAX (start_nat, end_nat);
}

static void
gtk_center_layout_compute_opposite_size (GtkCenterLayout  *self,
                                         GtkWidget        *widget,
                                         int               for_size,
                                         int              *minimum,
                                         int              *natural,
                                         int              *minimum_baseline,
                                         int              *natural_baseline)
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
    gtk_center_layout_distribute (self, -1, for_size, sizes);

  above_min = below_min = above_nat = below_nat = -1;
  total_min = total_nat = 0;

  for (i = 0; i < 3; i++)
    {
      if (child[i] == NULL)
        continue;

      gtk_widget_measure (child[i],
                          OPPOSITE_ORIENTATION (self->orientation),
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

      switch (self->baseline_position)
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
                           int              *min_baseline,
                           int              *nat_baseline)
{
  GtkCenterLayout *self = GTK_CENTER_LAYOUT (layout_manager);

  if (self->orientation != orientation)
    {
      gtk_center_layout_compute_opposite_size (self, widget, for_size,
                                               minimum, natural,
                                               min_baseline, nat_baseline);
    }
  else
    {
      gtk_center_layout_compute_size (self, widget, for_size,
                                      minimum, natural);
    }
}

static void
gtk_center_layout_allocate (GtkLayoutManager *layout_manager,
                            GtkWidget        *widget,
                            int               width,
                            int               height,
                            int               baseline)
{
  GtkCenterLayout *self = GTK_CENTER_LAYOUT (layout_manager);
  GtkAllocation child_allocation;
  GtkWidget *child[3];
  int child_size[3];
  int child_pos[3];
  GtkRequestedSize sizes[3];
  int size;
  int for_size;
  int i;

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

  gtk_center_layout_distribute (self, for_size, size, sizes);

  if (self->orientation == GTK_ORIENTATION_HORIZONTAL &&
      gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
    {
      child[0] = self->end_widget;
      child[1] = self->center_widget;
      child[2] = self->start_widget;
      child_size[0] = sizes[2].minimum_size;
      child_size[1] = sizes[1].minimum_size;
      child_size[2] = sizes[0].minimum_size;
    }
  else
    {
      child[0] = self->start_widget;
      child[1] = self->center_widget;
      child[2] = self->end_widget;
      child_size[0] = sizes[0].minimum_size;
      child_size[1] = sizes[1].minimum_size;
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
          switch (self->baseline_position)
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

  child_allocation = (GtkAllocation) { 0, 0, width, height };

  for (i = 0; i < 3; i++)
    {
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
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GtkLayoutManagerClass *layout_manager_class = GTK_LAYOUT_MANAGER_CLASS (klass);

  gobject_class->set_property = gtk_center_layout_set_property;
  gobject_class->get_property = gtk_center_layout_get_property;

  layout_manager_class->get_request_mode = gtk_center_layout_get_request_mode;
  layout_manager_class->measure = gtk_center_layout_measure;
  layout_manager_class->allocate = gtk_center_layout_allocate;

  /**
   * GtkCenterLayout:baseline-position:
   *
   * The position of the allocated baseline within the extra space
   * allocated to each child of the widget using a center layout
   * manager.
   *
   * This property is only relevant for horizontal layouts containing
   * at least one child with a baseline alignment.
   */
  center_layout_props[PROP_BASELINE_POSITION] =
    g_param_spec_enum ("baseline-position",
                       P_("Baseline position"),
                       P_("The position of the baseline aligned widgets if extra space is available"),
                       GTK_TYPE_BASELINE_POSITION,
                       GTK_BASELINE_POSITION_CENTER,
                       GTK_PARAM_READWRITE |
                       G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, center_layout_props);
  g_object_class_override_property (gobject_class, PROP_ORIENTATION, "orientation");
}

static void
gtk_center_layout_init (GtkCenterLayout *self)
{
  self->orientation = GTK_ORIENTATION_HORIZONTAL;
  self->baseline_position = GTK_BASELINE_POSITION_CENTER;
}

GtkLayoutManager *
gtk_center_layout_new (GtkOrientation orientation)
{
  return g_object_new (GTK_TYPE_CENTER_LAYOUT,
                       "orientation", orientation,
                       NULL);
}

/**
 * gtk_center_layout_set_baseline_position:
 * @center_layout: a #GtkCenterLayout
 * @position: a #GtkBaselinePosition
 *
 * Sets the baseline position of a center layout.
 *
 * The baseline position affects only horizontal centeres with at least one
 * baseline aligned child. If there is more vertical space available than
 * requested, and the baseline is not allocated by the parent then the
 * given @position is used to allocate the baseline within the extra
 * space available.
 */
void
gtk_center_layout_set_baseline_position (GtkCenterLayout     *center_layout,
                                         GtkBaselinePosition  position)
{
  g_return_if_fail (GTK_IS_CENTER_LAYOUT (center_layout));

  if (center_layout->baseline_position != position)
    {
      center_layout->baseline_position = position;

      g_object_notify_by_pspec (G_OBJECT (center_layout), center_layout_props[PROP_BASELINE_POSITION]);

      gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (center_layout));
    }
}

/**
 * gtk_center_layout_get_baseline_position:
 * @center_layout: a #GtkCenterLayout
 *
 * Gets the value set by gtk_center_layout_set_baseline_position().
 *
 * Returns: the baseline position
 */
GtkBaselinePosition
gtk_center_layout_get_baseline_position (GtkCenterLayout *center_layout)
{
  g_return_val_if_fail (GTK_IS_CENTER_LAYOUT (center_layout), GTK_BASELINE_POSITION_CENTER);

  return center_layout->baseline_position;
}

void
gtk_center_layout_set_start_widget (GtkCenterLayout *center_layout,
                                    GtkWidget       *child)
{
  center_layout->start_widget = child;
}

void
gtk_center_layout_set_center_widget (GtkCenterLayout *center_layout,
                                     GtkWidget       *child)
{
  center_layout->center_widget = child;
}

void
gtk_center_layout_set_end_widget (GtkCenterLayout *center_layout,
                                    GtkWidget       *child)
{
  center_layout->end_widget = child;
}
