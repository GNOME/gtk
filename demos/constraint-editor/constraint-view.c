/* Copyright (C) 2019 Red Hat, Inc.
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

#include <gtk/gtk.h>
#include "constraint-view.h"

struct _ConstraintView
{
  GtkWidget parent;

  GListStore *store;

  GtkWidget *drag_widget;
};

G_DEFINE_TYPE (ConstraintView, constraint_view, GTK_TYPE_WIDGET);

static void
constraint_view_dispose (GObject *object)
{
  ConstraintView *view = CONSTRAINT_VIEW (object);
  GtkWidget *child;

  while ((child = gtk_widget_get_first_child (GTK_WIDGET (view))) != NULL)
    gtk_widget_unparent (child);

  g_clear_object (&view->store);

  G_OBJECT_CLASS (constraint_view_parent_class)->dispose (object);
}

static void
constraint_view_class_init (ConstraintViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = constraint_view_dispose;

  gtk_widget_class_set_css_name (widget_class, "constraintview");
}

static void
update_weak_position (ConstraintView *self,
                      GtkWidget      *child,
                      double          x,
                      double          y)
{
  GtkLayoutManager *manager;
  GtkConstraint *constraint;

  manager = gtk_widget_get_layout_manager (GTK_WIDGET (self));
  constraint = (GtkConstraint *)g_object_get_data (G_OBJECT (child), "x-constraint");
  if (constraint)
    {
      gtk_constraint_layout_remove_constraint (GTK_CONSTRAINT_LAYOUT (manager),
                                               constraint);
      g_object_set_data (G_OBJECT (child), "x-constraint", NULL);
    }
  constraint = gtk_constraint_new_constant (child,
                                            GTK_CONSTRAINT_ATTRIBUTE_CENTER_X,
                                            GTK_CONSTRAINT_RELATION_EQ,
                                            x,
                                            GTK_CONSTRAINT_STRENGTH_WEAK);
  gtk_constraint_layout_add_constraint (GTK_CONSTRAINT_LAYOUT (manager),
                                        constraint);
  g_object_set_data (G_OBJECT (child), "x-constraint", constraint);

  constraint = (GtkConstraint *)g_object_get_data (G_OBJECT (child), "y-constraint");
  if (constraint)
    {
      gtk_constraint_layout_remove_constraint (GTK_CONSTRAINT_LAYOUT (manager),
                                               constraint);
      g_object_set_data (G_OBJECT (child), "y-constraint", NULL);
    }
  constraint = gtk_constraint_new_constant (child,
                                            GTK_CONSTRAINT_ATTRIBUTE_CENTER_Y,
                                            GTK_CONSTRAINT_RELATION_EQ,
                                            y,
                                            GTK_CONSTRAINT_STRENGTH_WEAK);
  gtk_constraint_layout_add_constraint (GTK_CONSTRAINT_LAYOUT (manager),
                                        constraint);
  g_object_set_data (G_OBJECT (child), "y-constraint", constraint);
}

static void
drag_begin (GtkGestureDrag *drag,
            double          start_x,
            double          start_y,
            ConstraintView *self)
{
  GtkWidget *widget;

  widget = gtk_widget_pick (GTK_WIDGET (self), start_x, start_y, GTK_PICK_DEFAULT);

  if (GTK_IS_LABEL (widget))
    {
      widget = gtk_widget_get_ancestor (widget, GTK_TYPE_FRAME);
      if (widget &&
          gtk_widget_get_parent (widget) == (GtkWidget *)self)
        {
          self->drag_widget = widget;
        }
    }
}

static void
drag_update (GtkGestureDrag *drag,
             double          offset_x,
             double          offset_y,
             ConstraintView *self)
{
  double x, y;

  if (!self->drag_widget)
    return;

  gtk_gesture_drag_get_start_point (drag, &x, &y);
  update_weak_position (self, self->drag_widget, x + offset_x, y + offset_y);
}

static void
drag_end (GtkGestureDrag *drag,
          double          offset_x,
          double          offset_y,
          ConstraintView *self)
{
  self->drag_widget = NULL;
}

static void
constraint_view_init (ConstraintView *self)
{
  GtkEventController *controller;

  gtk_widget_set_layout_manager (GTK_WIDGET (self),
                                 gtk_constraint_layout_new ());

  self->store = g_list_store_new (G_TYPE_OBJECT);

  controller = (GtkEventController *)gtk_gesture_drag_new ();
  g_signal_connect (controller, "drag-begin", G_CALLBACK (drag_begin), self);
  g_signal_connect (controller, "drag-update", G_CALLBACK (drag_update), self);
  g_signal_connect (controller, "drag-end", G_CALLBACK (drag_end), self);
  gtk_widget_add_controller (GTK_WIDGET (self), controller);
}

ConstraintView *
constraint_view_new (void)
{
  return g_object_new (CONSTRAINT_VIEW_TYPE, NULL);
}

void
constraint_view_add_child (ConstraintView *view,
                           const char     *name)
{
  GtkWidget *frame;
  GtkWidget *label;

  label = gtk_label_new (name);
  frame = gtk_frame_new (NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (frame), "child");
  g_object_set_data_full (G_OBJECT (frame), "name", g_strdup (name), g_free);
  gtk_container_add (GTK_CONTAINER (frame), label);
  gtk_widget_set_parent (frame, GTK_WIDGET (view));

  update_weak_position (view, frame, 100, 100);

  g_list_store_append (view->store, frame);
}

void
constraint_view_remove_child (ConstraintView *view,
                              GtkWidget      *child)
{
  int i;

  gtk_widget_unparent (child);

  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (view->store)); i++)
    {
      if (g_list_model_get_item (G_LIST_MODEL (view->store), i) == (GObject*)child)
        {
          g_list_store_remove (view->store, i);
          break;
        }
    }
}

void
constraint_view_add_guide (ConstraintView *view,
                           GtkConstraintGuide *guide)
{
  GtkLayoutManager *manager;
  GtkWidget *frame;
  GtkWidget *label;
  const char *name;
  GtkConstraint *constraint;

  name = (const char *)g_object_get_data (G_OBJECT (guide), "name");

  label = gtk_label_new (name);
  frame = gtk_frame_new (NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (frame), "guide");
  g_object_set_data_full (G_OBJECT (frame), "name", g_strdup (name), g_free);
  gtk_container_add (GTK_CONTAINER (frame), label);
  gtk_widget_insert_after (frame, GTK_WIDGET (view), NULL);

  g_object_set_data (G_OBJECT (guide), "frame", frame);
  g_object_set_data (G_OBJECT (guide), "label", label);

  manager = gtk_widget_get_layout_manager (GTK_WIDGET (view));
  gtk_constraint_layout_add_guide (GTK_CONSTRAINT_LAYOUT (manager),
                                   g_object_ref (guide));

  constraint = gtk_constraint_new (frame,
                                   GTK_CONSTRAINT_ATTRIBUTE_LEFT,
                                   GTK_CONSTRAINT_RELATION_EQ,
                                   guide,
                                   GTK_CONSTRAINT_ATTRIBUTE_LEFT,
                                   1.0, 0.0,
                                   GTK_CONSTRAINT_STRENGTH_REQUIRED);
  gtk_constraint_layout_add_constraint (GTK_CONSTRAINT_LAYOUT (manager),
                                        constraint);
  g_object_set_data (G_OBJECT (guide), "left-constraint", constraint);

  constraint = gtk_constraint_new (frame,
                                   GTK_CONSTRAINT_ATTRIBUTE_TOP,
                                   GTK_CONSTRAINT_RELATION_EQ,
                                   guide,
                                   GTK_CONSTRAINT_ATTRIBUTE_TOP,
                                   1.0, 0.0,
                                   GTK_CONSTRAINT_STRENGTH_REQUIRED);
  gtk_constraint_layout_add_constraint (GTK_CONSTRAINT_LAYOUT (manager),
                                        constraint);
  g_object_set_data (G_OBJECT (guide), "top-constraint", constraint);

  constraint = gtk_constraint_new (frame,
                                   GTK_CONSTRAINT_ATTRIBUTE_WIDTH,
                                   GTK_CONSTRAINT_RELATION_EQ,
                                   guide,
                                   GTK_CONSTRAINT_ATTRIBUTE_WIDTH,
                                   1.0, 0.0,
                                   GTK_CONSTRAINT_STRENGTH_REQUIRED);
  gtk_constraint_layout_add_constraint (GTK_CONSTRAINT_LAYOUT (manager),
                                        constraint);
  g_object_set_data (G_OBJECT (guide), "width-constraint", constraint);

  constraint = gtk_constraint_new (frame,
                                   GTK_CONSTRAINT_ATTRIBUTE_HEIGHT,
                                   GTK_CONSTRAINT_RELATION_EQ,
                                   guide,
                                   GTK_CONSTRAINT_ATTRIBUTE_HEIGHT,
                                   1.0, 0.0,
                                   GTK_CONSTRAINT_STRENGTH_REQUIRED);
  gtk_constraint_layout_add_constraint (GTK_CONSTRAINT_LAYOUT (manager),
                                        constraint);
  g_object_set_data (G_OBJECT (guide), "height-constraint", constraint);

  update_weak_position (view, frame, 150, 150);

  g_list_store_append (view->store, guide);
}

void
constraint_view_guide_changed (ConstraintView     *view,
                               GtkConstraintGuide *guide)
{
  GtkWidget *label;
  const char *name;
  int i;

  name = (const char *)g_object_get_data (G_OBJECT (guide), "name");
  label = (GtkWidget *)g_object_get_data (G_OBJECT (guide), "label");
  gtk_label_set_label (GTK_LABEL (label), name);

  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (view->store)); i++)
    {
      if (g_list_model_get_item (G_LIST_MODEL (view->store), i) == (GObject*)guide)
        {
          g_list_model_items_changed (G_LIST_MODEL (view->store), i, 1, 1);
          break;
        }
    }
}

void
constraint_view_remove_guide (ConstraintView     *view,
                              GtkConstraintGuide *guide)
{
  GtkLayoutManager *manager;
  GtkWidget *frame;
  GtkConstraint *constraint;
  int i;

  manager = gtk_widget_get_layout_manager (GTK_WIDGET (view));

  constraint = (GtkConstraint*)g_object_get_data (G_OBJECT (guide), "left-constraint");
  gtk_constraint_layout_remove_constraint (GTK_CONSTRAINT_LAYOUT (manager),
                                           constraint);
  constraint = (GtkConstraint*)g_object_get_data (G_OBJECT (guide), "top-constraint");
  gtk_constraint_layout_remove_constraint (GTK_CONSTRAINT_LAYOUT (manager),
                                           constraint);
  constraint = (GtkConstraint*)g_object_get_data (G_OBJECT (guide), "width-constraint");
  gtk_constraint_layout_remove_constraint (GTK_CONSTRAINT_LAYOUT (manager),
                                           constraint);
  constraint = (GtkConstraint*)g_object_get_data (G_OBJECT (guide), "height-constraint");
  gtk_constraint_layout_remove_constraint (GTK_CONSTRAINT_LAYOUT (manager),
                                           constraint);

  frame = (GtkWidget *)g_object_get_data (G_OBJECT (guide), "frame");
  gtk_widget_unparent (frame);

  gtk_constraint_layout_remove_guide (GTK_CONSTRAINT_LAYOUT (manager),
                                      guide);

  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (view->store)); i++)
    {
      if (g_list_model_get_item (G_LIST_MODEL (view->store), i) == (GObject*)guide)
        {
          g_list_store_remove (view->store, i);
          break;
        }
    }
}

void
constraint_view_add_constraint (ConstraintView *view,
                                GtkConstraint  *constraint)
{
  GtkLayoutManager *manager;

  manager = gtk_widget_get_layout_manager (GTK_WIDGET (view));
  gtk_constraint_layout_add_constraint (GTK_CONSTRAINT_LAYOUT (manager),
                                        g_object_ref (constraint));

  g_list_store_append (view->store, constraint);
}

void
constraint_view_remove_constraint (ConstraintView *view,
                                   GtkConstraint  *constraint)
{
  GtkLayoutManager *manager;
  int i;

  manager = gtk_widget_get_layout_manager (GTK_WIDGET (view));
  gtk_constraint_layout_remove_constraint (GTK_CONSTRAINT_LAYOUT (manager),
                                           constraint);
  for (i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (view->store)); i++)
    {
      if (g_list_model_get_item (G_LIST_MODEL (view->store), i) == (GObject*)constraint)
        {
          g_list_store_remove (view->store, i);
          break;
        }
    }
}

GListModel *
constraint_view_get_model (ConstraintView *view)
{
  return G_LIST_MODEL (view->store);
}
