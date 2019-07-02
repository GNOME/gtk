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

  GListModel *model;

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

  g_clear_object (&view->model);

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
  if (x != -100)
    {
      constraint = gtk_constraint_new_constant (child,
                                                GTK_CONSTRAINT_ATTRIBUTE_CENTER_X,
                                                GTK_CONSTRAINT_RELATION_EQ,
                                                x,
                                                GTK_CONSTRAINT_STRENGTH_WEAK);
      g_object_set_data (G_OBJECT (constraint), "internal", "yes");
      gtk_constraint_layout_add_constraint (GTK_CONSTRAINT_LAYOUT (manager),
                                            constraint);
      g_object_set_data (G_OBJECT (child), "x-constraint", constraint);
    }

  constraint = (GtkConstraint *)g_object_get_data (G_OBJECT (child), "y-constraint");
  if (constraint)
    {
      gtk_constraint_layout_remove_constraint (GTK_CONSTRAINT_LAYOUT (manager),
                                                   constraint);
      g_object_set_data (G_OBJECT (child), "y-constraint", NULL);
    }
  if (y != -100)
    {
      constraint = gtk_constraint_new_constant (child,
                                                GTK_CONSTRAINT_ATTRIBUTE_CENTER_Y,
                                                GTK_CONSTRAINT_RELATION_EQ,
                                                y,
                                                GTK_CONSTRAINT_STRENGTH_WEAK);
      g_object_set_data (G_OBJECT (constraint), "internal", "yes");
      gtk_constraint_layout_add_constraint (GTK_CONSTRAINT_LAYOUT (manager),
                                            constraint);
      g_object_set_data (G_OBJECT (child), "y-constraint", constraint);
    }
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

static gboolean
omit_internal (gpointer item, gpointer user_data)
{
  if (g_object_get_data (G_OBJECT (item), "internal"))
    return FALSE;

  return TRUE;
}

static void
constraint_view_init (ConstraintView *self)
{
  GtkLayoutManager *manager;
  GtkEventController *controller;
  GListStore *list;
  GListModel *all_children;
  GListModel *all_constraints;
  GListModel *guides;
  GListModel *children;
  GListModel *constraints;

  manager = gtk_constraint_layout_new ();
  gtk_widget_set_layout_manager (GTK_WIDGET (self), manager);

  all_children = gtk_widget_observe_children (GTK_WIDGET (self));
  all_constraints = gtk_constraint_layout_observe_constraints (GTK_CONSTRAINT_LAYOUT (manager));
  guides = gtk_constraint_layout_observe_guides (GTK_CONSTRAINT_LAYOUT (manager));
  constraints = (GListModel *)gtk_filter_list_model_new (all_constraints, omit_internal, NULL, NULL);
  children = (GListModel *)gtk_filter_list_model_new (all_children, omit_internal, NULL, NULL);

  list = g_list_store_new (G_TYPE_LIST_MODEL);
  g_list_store_append (list, children);
  g_list_store_append (list, guides);
  g_list_store_append (list, constraints);
  self->model = G_LIST_MODEL (gtk_flatten_list_model_new (G_TYPE_OBJECT, G_LIST_MODEL (list)));
  g_object_unref (children);
  g_object_unref (guides);
  g_object_unref (constraints);
  g_object_unref (all_children);
  g_object_unref (all_constraints);
  g_object_unref (list);


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
}

void
constraint_view_remove_child (ConstraintView *view,
                              GtkWidget      *child)
{
  update_weak_position (view, child, -100, -100);
  gtk_widget_unparent (child);
}

void
constraint_view_add_guide (ConstraintView *view,
                           GtkConstraintGuide *guide)
{
  GtkConstraintLayout *layout;
  GtkWidget *frame;
  GtkWidget *label;
  const char *name;
  GtkConstraint *constraint;
  struct {
    const char *name;
    GtkConstraintAttribute attr;
  } names[] = {
    { "left-constraint", GTK_CONSTRAINT_ATTRIBUTE_LEFT },
    { "top-constraint", GTK_CONSTRAINT_ATTRIBUTE_TOP },
    { "width-constraint", GTK_CONSTRAINT_ATTRIBUTE_WIDTH },
    { "height-constraint", GTK_CONSTRAINT_ATTRIBUTE_HEIGHT },
  };
  int i;

  name = (const char *)g_object_get_data (G_OBJECT (guide), "name");

  label = gtk_label_new (name);
  frame = gtk_frame_new (NULL);
  gtk_style_context_add_class (gtk_widget_get_style_context (frame), "guide");
  g_object_set_data (G_OBJECT (frame), "internal", "yes");
  gtk_container_add (GTK_CONTAINER (frame), label);
  gtk_widget_insert_after (frame, GTK_WIDGET (view), NULL);

  g_object_set_data (G_OBJECT (guide), "frame", frame);
  g_object_set_data (G_OBJECT (guide), "label", label);

  layout = GTK_CONSTRAINT_LAYOUT (gtk_widget_get_layout_manager (GTK_WIDGET (view)));
  gtk_constraint_layout_add_guide (layout, g_object_ref (guide));

  for (i = 0; i < G_N_ELEMENTS (names); i++)
    {
      constraint = gtk_constraint_new (frame,
                                       names[i].attr,
                                       GTK_CONSTRAINT_RELATION_EQ,
                                       guide,
                                       names[i].attr,
                                       1.0, 0.0,
                                       GTK_CONSTRAINT_STRENGTH_REQUIRED);
      g_object_set_data (G_OBJECT (constraint), "internal", "yes");
      gtk_constraint_layout_add_constraint (layout, constraint);
      g_object_set_data (G_OBJECT (guide), names[i].name, constraint);
    }

  update_weak_position (view, frame, 150, 150);
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
  for (i = 0; i < g_list_model_get_n_items (view->model); i++)
    {
      if (g_list_model_get_item (view->model, i) == (GObject*)guide)
        {
          g_list_model_items_changed (view->model, i, 1, 1);
          break;
        }
    }
}

void
constraint_view_remove_guide (ConstraintView     *view,
                              GtkConstraintGuide *guide)
{
  GtkConstraintLayout *layout;
  GtkWidget *frame;
  GtkConstraint *constraint;
  const char *names[] = {
    "left-constraint",
    "top-constraint",
    "width-constraint",
    "height-constraint"
  };
  int i;

  layout = GTK_CONSTRAINT_LAYOUT (gtk_widget_get_layout_manager (GTK_WIDGET (view)));

  for (i = 0; i < G_N_ELEMENTS (names); i++)
    {
      constraint = (GtkConstraint*)g_object_get_data (G_OBJECT (guide), names[i]);
      gtk_constraint_layout_remove_constraint (layout, constraint);
    }

  frame = (GtkWidget *)g_object_get_data (G_OBJECT (guide), "frame");
  update_weak_position (view, frame, -100, -100);
  gtk_widget_unparent (frame);

  gtk_constraint_layout_remove_guide (layout, guide);
}

void
constraint_view_add_constraint (ConstraintView *view,
                                GtkConstraint  *constraint)
{
  GtkLayoutManager *manager;

  manager = gtk_widget_get_layout_manager (GTK_WIDGET (view));
  gtk_constraint_layout_add_constraint (GTK_CONSTRAINT_LAYOUT (manager),
                                        g_object_ref (constraint));
}

void
constraint_view_remove_constraint (ConstraintView *view,
                                   GtkConstraint  *constraint)
{
  GtkLayoutManager *manager;

  manager = gtk_widget_get_layout_manager (GTK_WIDGET (view));
  gtk_constraint_layout_remove_constraint (GTK_CONSTRAINT_LAYOUT (manager),
                                           constraint);
}

GListModel *
constraint_view_get_model (ConstraintView *view)
{
  return view->model;
}
