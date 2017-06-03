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

struct _GtkCenterBox
{
  GtkWidget parent_instance;

  GtkWidget *start_widget;
  GtkWidget *center_widget;
  GtkWidget *end_widget;
};


struct _GtkCenterBoxClass
{
  GtkWidgetClass parent_class;
};


G_DEFINE_TYPE (GtkCenterBox, gtk_center_box, GTK_TYPE_WIDGET);


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

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum = start_min + center_min + end_min;
      *natural = center_nat + 2 * MAX (start_nat, end_nat);
    }
  else /* GTK_ORIENTATION_VERTICAL */
    {
      *minimum = MAX (start_min, MAX (center_min, end_min));
      *natural = MAX (start_nat, MAX (center_nat, end_nat));
    }
}

static void
gtk_center_box_size_allocate (GtkWidget     *widget,
                              GtkAllocation *allocation)
{
  GtkCenterBox *self = GTK_CENTER_BOX (widget);
  GtkAllocation child_allocation;
  GtkAllocation clip = *allocation;
  GtkAllocation child_clip;
  GtkWidget *left;
  GtkWidget *right;
  int left_size = 0;
  int left_min = 0;
  int left_nat = 0;
  int center_size = 0;
  int center_min = 0;
  int center_nat = 0;
  int right_size = 0;
  int right_min = 0;
  int right_nat = 0;
  int avail;

  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR)
    {
      left = self->start_widget;
      right = self->end_widget;
    }
  else
    {
      right = self->start_widget;
      left = self->end_widget;
    }

  GTK_WIDGET_CLASS (gtk_center_box_parent_class)->size_allocate (widget, allocation);

  if (left)
    gtk_widget_measure (left, GTK_ORIENTATION_HORIZONTAL,
                        allocation->height,
                        &left_min, &left_nat, NULL, NULL);

  if (right)
    gtk_widget_measure (right, GTK_ORIENTATION_HORIZONTAL,
                        allocation->height,
                        &right_min, &right_nat, NULL, NULL);

  if (self->center_widget)
    gtk_widget_measure (self->center_widget, GTK_ORIENTATION_HORIZONTAL,
                        allocation->height,
                        &center_min, &center_nat, NULL, NULL);

  child_allocation.y = allocation->y;
  child_allocation.height = allocation->height;

  center_size = CLAMP (allocation->width - (left_min + right_min), center_min, center_nat);

  if (left)
    {
      avail = MIN ((allocation->width - center_size) / 2, allocation->width - (center_size + right_min));
      child_allocation.width = CLAMP (avail, left_min, left_nat);
      child_allocation.x = allocation->x;

      gtk_widget_size_allocate (left, &child_allocation);
      gtk_widget_get_clip (left, &child_clip);
      gdk_rectangle_union (&clip, &clip, &child_clip);
      left_size = child_allocation.width;
    }

  if (right)
    {
      avail = MIN ((allocation->width - center_size) / 2, allocation->width - (center_size + left_min));
      child_allocation.width = CLAMP (avail, right_min, right_nat);
      child_allocation.x = allocation->x + allocation->width - child_allocation.width;

      gtk_widget_size_allocate (right, &child_allocation);
      gtk_widget_get_clip (right, &child_clip);
      gdk_rectangle_union (&clip, &clip, &child_clip);
      right_size = child_allocation.width;
    }

  if (self->center_widget)
    {
      child_allocation.width = center_size;
      child_allocation.x = (allocation->width / 2) - (child_allocation.width / 2);

      /* Push in from start/end */
      if (left_size > child_allocation.x)
        child_allocation.x = left_size;
      else if (allocation->width - right_size < child_allocation.x + child_allocation.width)
        child_allocation.x = allocation->width - child_allocation.width - right_size;

      child_allocation.x += allocation->x;
      gtk_widget_size_allocate (self->center_widget, &child_allocation);
      gtk_widget_get_clip (self->center_widget, &child_clip);
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

static void
gtk_center_box_class_init (GtkCenterBoxClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->measure = gtk_center_box_measure;
  widget_class->size_allocate = gtk_center_box_size_allocate;
  widget_class->snapshot = gtk_center_box_snapshot;
  widget_class->direction_changed = gtk_center_box_direction_changed;

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
