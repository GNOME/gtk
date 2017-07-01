/*
 * Copyright (c) 2017 Timm Bäder <mail@baedert.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Timm Bäder <mail@baedert.org>
 */

/**
 * SECTION:gtkcenterbox
 * @Short_description: A centering container
 * @Title: GtkCenterBox
 * @See_also: #GtkBox
 *
 * The GtkCenterBox widget arranges three children in a horizontal
 * or vertical arrangement, keeping the middle child centered as well
 * as possible.
 *
 * To add children to GtkCenterBox, use gtk_center_box_set_start_widget(),
 * gtk_center_box_set_center_widget() and gtk_center_box_set_end_widget().
 *
 * The sizing and positioning of children can be influenced with the
 * align and expand properties of the children.
 *
 * # GtkCenterBox as GtkBuildable
 *
 * The GtkCenterBox implementation of the GtkBuildable interface supports
 * placing children in the 3 positions by specifying “start”, “center” or
 * “end” as the “type” attribute of a <child> element.
 *
 * # CSS nodes
 *
 * GtkCenterBox uses a single CSS node with name box.
 *
 * In horizontal orientation, the nodes of the children are always arranged
 * from left to right. So :first-child will always select the leftmost child,
 * regardless of text direction.
 *
 * In vertical orientation, the are arranged from top to bottom.
 */

#include "config.h"
#include "gtkcenterbox.h"
#include "gtkcssnodeprivate.h"
#include "gtkwidgetprivate.h"
#include "gtkorientable.h"
#include "gtkorientableprivate.h"
#include "gtkbuildable.h"
#include "gtksizerequest.h"
#include "gtktypebuiltins.h"
#include "gtkprivate.h"
#include "gtkintl.h"

struct _GtkCenterBox
{
  GtkWidget parent_instance;

  GtkWidget *start_widget;
  GtkWidget *center_widget;
  GtkWidget *end_widget;

  GtkOrientation orientation;
  GtkBaselinePosition baseline_pos;
};

struct _GtkCenterBoxClass
{
  GtkWidgetClass parent_class;
};


enum {
  PROP_0,
  PROP_BASELINE_POSITION,
  PROP_ORIENTATION
};

static void gtk_center_box_buildable_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkCenterBox, gtk_center_box, GTK_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_ORIENTABLE, NULL)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, gtk_center_box_buildable_init))

static void
gtk_center_box_buildable_add_child (GtkBuildable  *buildable,
                                    GtkBuilder    *builder,
                                    GObject       *child,
                                    const gchar   *type)
{
  if (g_strcmp0 (type, "start") == 0)
    gtk_center_box_set_start_widget (GTK_CENTER_BOX (buildable), GTK_WIDGET (child));
  else if (g_strcmp0 (type, "center") == 0)
    gtk_center_box_set_center_widget (GTK_CENTER_BOX (buildable), GTK_WIDGET (child));
  else if (g_strcmp0 (type, "end") == 0)
    gtk_center_box_set_end_widget (GTK_CENTER_BOX (buildable), GTK_WIDGET (child));
  else
    GTK_BUILDER_WARN_INVALID_CHILD_TYPE (GTK_CENTER_BOX (buildable), type);
}

static void
gtk_center_box_buildable_init (GtkBuildableIface *iface)
{
  iface->add_child = gtk_center_box_buildable_add_child;
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
gtk_center_box_distribute (GtkCenterBox     *self,
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
gtk_center_box_measure_orientation (GtkWidget      *widget,
                                    GtkOrientation  orientation,
                                    int             for_size,
                                    int            *minimum,
                                    int            *natural,
                                    int            *minimum_baseline,
                                    int            *natural_baseline)
{
  GtkCenterBox *self = GTK_CENTER_BOX (widget);
  int min_baseline, nat_baseline;
  int start_min = 0;
  int start_nat = 0;
  int center_min = 0;
  int center_nat = 0;
  int end_min = 0;
  int end_nat = 0;

  if (self->start_widget)
    gtk_widget_measure (self->start_widget,
                        orientation,
                        for_size,
                        &start_min, &start_nat,
                        &min_baseline, &nat_baseline);

  if (self->center_widget)
    gtk_widget_measure (self->center_widget,
                        orientation,
                        for_size,
                        &center_min, &center_nat,
                        &min_baseline, &nat_baseline);

  if (self->end_widget)
    gtk_widget_measure (self->end_widget,
                        orientation,
                        for_size,
                        &end_min, &end_nat,
                        &min_baseline, &nat_baseline);

  *minimum = start_min + center_min + end_min;
  *natural = center_nat + 2 * MAX (start_nat, end_nat);
}

static void
gtk_center_box_measure_opposite (GtkWidget      *widget,
                                 GtkOrientation  orientation,
                                 int             for_size,
                                 int            *minimum,
                                 int            *natural,
                                 int            *minimum_baseline,
                                 int            *natural_baseline)
{
  GtkCenterBox *self = GTK_CENTER_BOX (widget);
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
    gtk_center_box_distribute (self, -1, for_size, sizes);

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
      int min_baseline;
      int nat_baseline;

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
gtk_center_box_measure (GtkWidget      *widget,
                        GtkOrientation  orientation,
                        int             for_size,
                        int            *minimum,
                        int            *natural,
                        int            *minimum_baseline,
                        int            *natural_baseline)
{
  GtkCenterBox *self = GTK_CENTER_BOX (widget);

  if (self->orientation == orientation)
    gtk_center_box_measure_orientation (widget, orientation, for_size, minimum, natural, minimum_baseline, natural_baseline);
  else
    gtk_center_box_measure_opposite (widget, orientation, for_size, minimum, natural, minimum_baseline, natural_baseline);
}

static void
gtk_center_box_size_allocate (GtkWidget     *widget,
                              GtkAllocation *allocation)
{
  GtkCenterBox *self = GTK_CENTER_BOX (widget);
  GtkAllocation child_allocation;
  GtkAllocation clip = *allocation;
  GtkAllocation child_clip;
  GtkWidget *child[3];
  int child_size[3];
  int child_pos[3];
  GtkRequestedSize sizes[3];
  int size;
  int for_size;
  int baseline;
  int i;

  GTK_WIDGET_CLASS (gtk_center_box_parent_class)->size_allocate (widget, allocation);

  if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      size = allocation->width;
      for_size = allocation->height;
      baseline = gtk_widget_get_allocated_baseline (widget);
    }
  else
    {
      size = allocation->height;
      for_size = allocation->width;
      baseline = -1;
    }

  /* Allocate child sizes */

  gtk_center_box_distribute (self, for_size, size, sizes);

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
          switch (self->baseline_pos)
            {
            case GTK_BASELINE_POSITION_TOP:
              baseline = min_above;
              break;
            case GTK_BASELINE_POSITION_CENTER:
              baseline = min_above + (allocation->height - (min_above + min_below)) / 2;
              break;
            case GTK_BASELINE_POSITION_BOTTOM:
              baseline = allocation->height - min_below;
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

  child_allocation = *allocation;

  for (i = 0; i < 3; i++)
    {
      if (child[i] == NULL)
        continue;

      if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
        {
          child_allocation.x = allocation->x + child_pos[i];
          child_allocation.y = allocation->y;
          child_allocation.width = child_size[i];
          child_allocation.height = allocation->height;
        }
      else
        {
          child_allocation.x = allocation->x;
          child_allocation.y = allocation->y + child_pos[i];
          child_allocation.width = allocation->width;
          child_allocation.height = child_size[i];
        }

      gtk_widget_size_allocate_with_baseline (child[i], &child_allocation, allocation->y + baseline);
      gtk_widget_get_clip (child[i], &child_clip);
      gdk_rectangle_union (&clip, &clip, &child_clip);
    }

  gtk_widget_set_clip (widget, &clip);
}

static void
gtk_center_box_snapshot (GtkWidget   *widget,
                         GtkSnapshot *snapshot)
{
  GtkCenterBox *self = GTK_CENTER_BOX (widget);

  if (self->start_widget)
    gtk_widget_snapshot_child (widget, self->start_widget, snapshot);

  if (self->center_widget)
    gtk_widget_snapshot_child (widget, self->center_widget, snapshot);

  if (self->end_widget)
    gtk_widget_snapshot_child (widget, self->end_widget, snapshot);
}

static void
update_css_node_order (GtkCenterBox *self)
{
  GtkCssNode *parent;
  GtkCssNode *first;
  GtkCssNode *last;

  parent = gtk_widget_get_css_node (GTK_WIDGET (self));

  if (gtk_widget_get_direction (GTK_WIDGET (self)) == GTK_TEXT_DIR_LTR)
    {
      first = self->start_widget ? gtk_widget_get_css_node (self->start_widget) : NULL;
      last = self->end_widget ? gtk_widget_get_css_node (self->end_widget) : NULL;
    }
  else
    {
      first = self->end_widget ? gtk_widget_get_css_node (self->end_widget) : NULL;
      last = self->start_widget ? gtk_widget_get_css_node (self->start_widget) : NULL;
    }

  if (first)
    gtk_css_node_insert_after (parent, first, NULL);
  if (last)
    gtk_css_node_insert_before (parent, last, NULL);
}

static void
gtk_center_box_direction_changed (GtkWidget        *widget,
                                  GtkTextDirection  previous_direction)
{
  update_css_node_order (GTK_CENTER_BOX (widget));
}

static GtkSizeRequestMode
gtk_center_box_get_request_mode (GtkWidget *widget)
{
  GtkCenterBox *self = GTK_CENTER_BOX (widget);
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

static void
gtk_center_box_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkCenterBox *self = GTK_CENTER_BOX (object);

  switch (prop_id)
    {
    case PROP_BASELINE_POSITION:
      gtk_center_box_set_baseline_position (self, g_value_get_enum (value));
      break;

    case PROP_ORIENTATION:
      {
        GtkOrientation orientation = g_value_get_enum (value);
        if (self->orientation != orientation)
          {
            self->orientation = orientation;
            _gtk_orientable_set_style_classes (GTK_ORIENTABLE (self));
            gtk_widget_queue_resize (GTK_WIDGET (self));
            g_object_notify (object, "orientation");
          }
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_center_box_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkCenterBox *self = GTK_CENTER_BOX (object);

  switch (prop_id)
    {
    case PROP_BASELINE_POSITION:
      g_value_set_enum (value, self->baseline_pos);
      break;

    case PROP_ORIENTATION:
      g_value_set_enum (value, self->orientation);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_center_box_class_init (GtkCenterBoxClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->set_property = gtk_center_box_set_property;
  object_class->get_property = gtk_center_box_get_property;

  widget_class->measure = gtk_center_box_measure;
  widget_class->size_allocate = gtk_center_box_size_allocate;
  widget_class->snapshot = gtk_center_box_snapshot;
  widget_class->direction_changed = gtk_center_box_direction_changed;
  widget_class->get_request_mode = gtk_center_box_get_request_mode;

  g_object_class_override_property (object_class, PROP_ORIENTATION, "orientation");

  g_object_class_install_property (object_class, PROP_BASELINE_POSITION,
          g_param_spec_enum ("baseline-position",
                             P_("Baseline position"),
                             P_("The position of the baseline aligned widgets if extra space is available"),
                             GTK_TYPE_BASELINE_POSITION,
                             GTK_BASELINE_POSITION_CENTER,
                             GTK_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));


  gtk_widget_class_set_accessible_role (widget_class, ATK_ROLE_FILLER);
  gtk_widget_class_set_css_name (widget_class, "box");
}

static void
gtk_center_box_init (GtkCenterBox *self)
{
  gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

  self->start_widget = NULL;
  self->center_widget = NULL;
  self->end_widget = NULL;

  self->orientation = GTK_ORIENTATION_HORIZONTAL;
  self->baseline_pos = GTK_BASELINE_POSITION_CENTER;
}

/**
 * gtk_center_box_new:
 *
 * Creates a new #GtkCenterBox.
 *
 * Returns: the new #GtkCenterBox.
 *
 * Since: 3.92
 */
GtkWidget *
gtk_center_box_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_CENTER_BOX, NULL));
}

/**
 * gtk_center_box_set_start_widget:
 * @self: a #GtkCenterBox
 * @child: the child
 *
 * Sets the start widget.
 *
 * Since: 3.92
 */
void
gtk_center_box_set_start_widget (GtkCenterBox *self,
                                 GtkWidget    *child)
{
  if (self->start_widget)
    gtk_widget_unparent (self->start_widget);

  self->start_widget = child;
  if (child)
    gtk_widget_set_parent (child, GTK_WIDGET (self));

  update_css_node_order (self);
}

/**
 * gtk_center_box_set_center_widget:
 * @self: a #GtkCenterBox
 * @child: the child
 *
 * Sets the center widget.
 *
 * Since: 3.92
 */
void
gtk_center_box_set_center_widget (GtkCenterBox *self,
                                 GtkWidget     *child)
{
  if (self->center_widget)
    gtk_widget_unparent (self->center_widget);

  self->center_widget = child;
  if (child)
    gtk_widget_set_parent (child, GTK_WIDGET (self));

  update_css_node_order (self);
}

/**
 * gtk_center_box_set_end_widget:
 * @self: a #GtkCenterBox
 * @child: the child
 *
 * Sets the end widget.
 *
 * Since: 3.92
 */
void
gtk_center_box_set_end_widget (GtkCenterBox *self,
                               GtkWidget    *child)
{
  if (self->end_widget)
    gtk_widget_unparent (self->end_widget);

  self->end_widget = child;
  if (child)
    gtk_widget_set_parent (child, GTK_WIDGET (self));

  update_css_node_order (self);
}

/**
 * gtk_center_box_get_start_widget:
 * @self: a #GtkCenterBox
 *
 * Gets the start widget.
 *
 * Returns: the start widget.
 *
 * Since: 3.92
 */
GtkWidget *
gtk_center_box_get_start_widget (GtkCenterBox *self)
{
  return self->start_widget;
}

/**
 * gtk_center_box_get_center_widget:
 * @self: a #GtkCenterBox
 *
 * Gets the center widget.
 *
 * Returns: the center widget.
 *
 * Since: 3.92
 */
GtkWidget *
gtk_center_box_get_center_widget (GtkCenterBox *self)
{
  return self->center_widget;
}

/**
 * gtk_center_box_get_end_widget:
 * @self: a #GtkCenterBox
 *
 * Gets the end widget.
 *
 * Returns: the end widget.
 *
 * Since: 3.92
 */
GtkWidget *
gtk_center_box_get_end_widget (GtkCenterBox *self)
{
  return self->end_widget;
}

/**
 * gtk_center_box_set_baseline_position:
 * @self: a #GtkCenterBox
 * @position: a #GtkBaselinePosition
 *
 * Sets the baseline position of a center box.
 *
 * This affects only horizontal boxes with at least one baseline
 * aligned child. If there is more vertical space available than
 * requested, and the baseline is not allocated by the parent then
 * @position is used to allocate the baseline wrt. the extra space
 * available.
 *
 * Since: 3.92
 */
void
gtk_center_box_set_baseline_position (GtkCenterBox        *self,
                                      GtkBaselinePosition  position)
{
  g_return_if_fail (GTK_IS_CENTER_BOX (self));

  if (self->baseline_pos != position)
    {
      self->baseline_pos = position;
      g_object_notify (G_OBJECT (self), "baseline-position");
      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}

/**
 * gtk_center_box_get_baseline_position:
 * @self: a #GtkCenterBox
 *
 * Gets the value set by gtk_center_box_set_baseline_position().
 *
 * Returns: the baseline position
 *
 * Since: 3.92
 */
GtkBaselinePosition
gtk_center_box_get_baseline_position (GtkCenterBox *self)
{
  g_return_val_if_fail (GTK_IS_CENTER_BOX (self), GTK_BASELINE_POSITION_CENTER);

  return self->baseline_pos;
}

