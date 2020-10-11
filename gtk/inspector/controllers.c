/*
 * Copyright (c) 2014 Red Hat, Inc.
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
#include <glib/gi18n-lib.h>

#include "controllers.h"

#include "gtkbinlayout.h"
#include "gtkdropdown.h"
#include "gtkbox.h"
#include "gtkcustomsorter.h"
#include "gtkflattenlistmodel.h"
#include "gtkframe.h"
#include "gtkgesture.h"
#include "gtklabel.h"
#include "gtklistbox.h"
#include "gtkmaplistmodel.h"
#include "gtkpropertylookuplistmodelprivate.h"
#include "gtkscrolledwindow.h"
#include "gtksizegroup.h"
#include "gtksortlistmodel.h"
#include "gtkstack.h"
#include "gtkwidgetprivate.h"
#include "window.h"

struct _GtkInspectorControllers
{
  GtkWidget parent_instance;

  GtkWidget *listbox;
  GtkPropertyLookupListModel *model;
  GtkSizeGroup *sizegroup;
};

struct _GtkInspectorControllersClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (GtkInspectorControllers, gtk_inspector_controllers, GTK_TYPE_WIDGET)

static void
row_activated (GtkListBox              *box,
               GtkListBoxRow           *row,
               GtkInspectorControllers *self)
{
  GtkInspectorWindow *iw;
  GObject *controller;

  iw = GTK_INSPECTOR_WINDOW (gtk_widget_get_ancestor (GTK_WIDGET (self), GTK_TYPE_INSPECTOR_WINDOW));

  controller = G_OBJECT (g_object_get_data (G_OBJECT (row), "controller"));

  gtk_inspector_window_push_object (iw, controller, CHILD_KIND_CONTROLLER, 0);
}

static void
gtk_inspector_controllers_init (GtkInspectorControllers *self)
{
  GtkWidget *sw, *box;

  self->sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  sw = gtk_scrolled_window_new ();

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  g_object_set (box,
                "margin-start", 60,
                "margin-end", 60,
                "margin-top", 60,
                "margin-bottom", 30,
                NULL);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), box);
  gtk_widget_set_hexpand (box, TRUE);
  gtk_widget_set_vexpand (box, TRUE);

  self->listbox = gtk_list_box_new ();
  gtk_widget_add_css_class (self->listbox, "frame");
  gtk_widget_set_halign (self->listbox, GTK_ALIGN_CENTER);
  g_signal_connect (self->listbox, "row-activated", G_CALLBACK (row_activated), self);
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (self->listbox), GTK_SELECTION_NONE);
  gtk_box_append (GTK_BOX (box), self->listbox);

  gtk_widget_set_parent (sw, GTK_WIDGET (self));
}

static void
phase_changed_cb (GtkDropDown             *dropdown,
                  GParamSpec              *pspec,
                  GtkInspectorControllers *self)
{
  GtkWidget *row;
  GtkPropagationPhase phase;
  GtkEventController *controller;

  phase = gtk_drop_down_get_selected (dropdown);
  row = gtk_widget_get_ancestor (GTK_WIDGET (dropdown), GTK_TYPE_LIST_BOX_ROW);
  controller = GTK_EVENT_CONTROLLER (g_object_get_data (G_OBJECT (row), "controller"));
  gtk_event_controller_set_propagation_phase (controller, phase);
}

static GtkWidget *
create_controller_widget (gpointer item,
                          gpointer user_data)
{
  GtkInspectorControllers *self = user_data;
  GtkEventController *controller = item;
  GtkWidget *row;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *dropdown;
  const char *phases[5];

  row = gtk_list_box_row_new ();
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 40);
  gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);
  gtk_widget_set_margin_start (box, 10);
  gtk_widget_set_margin_end (box, 10);
  gtk_widget_set_margin_top (box, 10);
  gtk_widget_set_margin_bottom (box, 10);
  label = gtk_label_new (G_OBJECT_TYPE_NAME (controller));
  g_object_set (label, "xalign", 0.0, NULL);
  gtk_box_append (GTK_BOX (box), label);
  gtk_size_group_add_widget (self->sizegroup, label);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE);

  phases[0] = C_("event phase", "None");
  phases[1] = C_("event phase", "Capture");
  phases[2] = C_("event phase", "Bubble");
  phases[3] = C_("event phase", "Target");
  phases[4] = NULL;

  dropdown = gtk_drop_down_new_from_strings (phases);
  gtk_drop_down_set_selected (GTK_DROP_DOWN (dropdown), gtk_event_controller_get_propagation_phase (controller));
  gtk_box_append (GTK_BOX (box), dropdown);
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE);

  g_object_set_data (G_OBJECT (row), "controller", controller);
  g_signal_connect (dropdown, "notify::selected", G_CALLBACK (phase_changed_cb), self);

  return row;
}

static gpointer
map_to_controllers (gpointer widget,
                    gpointer unused)
{
  gpointer result = gtk_widget_observe_controllers (widget);
  g_object_unref (widget);
  return result;
}

static int
compare_phases (GtkPropagationPhase first,
                GtkPropagationPhase second)
{
  int priorities[] = {
    [GTK_PHASE_NONE] = 0,
    [GTK_PHASE_CAPTURE] = 1,
    [GTK_PHASE_BUBBLE] = 3,
    [GTK_PHASE_TARGET] = 2
  };

  return priorities[first] - priorities[second];
}

static int
compare_controllers (gconstpointer _first,
                     gconstpointer _second,
                     gpointer      unused)
{
  GtkEventController *first = GTK_EVENT_CONTROLLER (_first);
  GtkEventController *second = GTK_EVENT_CONTROLLER (_second);
  GtkPropagationPhase first_phase, second_phase;
  GtkWidget *first_widget, *second_widget;
  int result;

  
  first_phase = gtk_event_controller_get_propagation_phase (first);
  second_phase = gtk_event_controller_get_propagation_phase (second);
  result = compare_phases (first_phase, second_phase);
  if (result != 0)
    return result;

  first_widget = gtk_event_controller_get_widget (first);
  second_widget = gtk_event_controller_get_widget (second);
  if (first_widget == second_widget)
    return 0;

  if (gtk_widget_is_ancestor (first_widget, second_widget))
    result = -1;
  else
    result = 1;

  if (first_phase == GTK_PHASE_BUBBLE)
    result = -result;

  return result;
}

void
gtk_inspector_controllers_set_object (GtkInspectorControllers *self,
                                      GObject                 *object)
{
  GtkWidget *stack;
  GtkStackPage *page;
  GtkMapListModel *map_model;
  GtkFlattenListModel *flatten_model;
  GtkSortListModel *sort_model;
  GtkSorter *sorter;

  stack = gtk_widget_get_parent (GTK_WIDGET (self));
  page = gtk_stack_get_page (GTK_STACK (stack), GTK_WIDGET (self));

  if (!GTK_IS_WIDGET (object))
    {
      g_object_set (page, "visible", FALSE, NULL);
      return;
    }

  g_object_set (page, "visible", TRUE, NULL);

  self->model = gtk_property_lookup_list_model_new (GTK_TYPE_WIDGET, "parent");
  gtk_property_lookup_list_model_set_object (self->model, object);

  map_model = gtk_map_list_model_new (G_LIST_MODEL (self->model), map_to_controllers, NULL, NULL);

  flatten_model = gtk_flatten_list_model_new (G_LIST_MODEL (map_model));

  sorter = GTK_SORTER (gtk_custom_sorter_new (compare_controllers, NULL, NULL));
  sort_model = gtk_sort_list_model_new (G_LIST_MODEL (flatten_model), sorter);

  gtk_list_box_bind_model (GTK_LIST_BOX (self->listbox),
                           G_LIST_MODEL (sort_model),
                           create_controller_widget,
                           self,
                           NULL);

  g_object_unref (sort_model);
}

static void
gtk_inspector_controllers_dispose (GObject *object)
{
  GtkInspectorControllers *self = GTK_INSPECTOR_CONTROLLERS (object);

  gtk_widget_unparent (gtk_widget_get_first_child (GTK_WIDGET (self)));

  g_clear_object (&self->sizegroup);

  G_OBJECT_CLASS (gtk_inspector_controllers_parent_class)->dispose (object);
}

static void
gtk_inspector_controllers_class_init (GtkInspectorControllersClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gtk_inspector_controllers_dispose;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

// vim: set et sw=2 ts=2:
