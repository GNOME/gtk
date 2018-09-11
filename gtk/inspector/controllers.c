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
#include "object-tree.h"

#include "gtksizegroup.h"
#include "gtkcomboboxtext.h"
#include "gtklistbox.h"
#include "gtkgesture.h"
#include "gtklabel.h"
#include "gtkframe.h"
#include "gtkwidgetprivate.h"

enum
{
  PROP_0,
  PROP_OBJECT_TREE
};

struct _GtkInspectorControllersPrivate
{
  GtkWidget *listbox;
  GListModel *model;
  GtkSizeGroup *sizegroup;
  GtkInspectorObjectTree *object_tree;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorControllers, gtk_inspector_controllers, GTK_TYPE_BOX)

static void
row_activated (GtkListBox              *box,
               GtkListBoxRow           *row,
               GtkInspectorControllers *sl)
{
  GObject *controller;
  
  controller = G_OBJECT (g_object_get_data (G_OBJECT (row), "controller"));
  gtk_inspector_object_tree_select_object (sl->priv->object_tree, controller);
}

static void
gtk_inspector_controllers_init (GtkInspectorControllers *sl)
{
  GtkWidget *frame;

  sl->priv = gtk_inspector_controllers_get_instance_private (sl);
  sl->priv->sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  g_object_set (sl,
                "orientation", GTK_ORIENTATION_VERTICAL,
                "margin-start", 60,
                "margin-end", 60,
                "margin-top", 60,
                "margin-bottom", 30,
                "spacing", 10,
                NULL);

  frame = gtk_frame_new (NULL);
  gtk_widget_set_halign (frame, GTK_ALIGN_CENTER);

  sl->priv->listbox = gtk_list_box_new ();
  g_signal_connect (sl->priv->listbox, "row-activated", G_CALLBACK (row_activated), sl);
  gtk_container_add (GTK_CONTAINER (frame), sl->priv->listbox);
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (sl->priv->listbox), GTK_SELECTION_NONE);

  gtk_container_add (GTK_CONTAINER (sl), frame);
}

static void
phase_changed_cb (GtkComboBox             *combo,
                  GtkInspectorControllers *sl)
{
  GtkWidget *row;
  GtkPropagationPhase phase;
  GtkEventController *controller;

  phase = gtk_combo_box_get_active (combo);
  row = gtk_widget_get_ancestor (GTK_WIDGET (combo), GTK_TYPE_LIST_BOX_ROW);
  controller = GTK_EVENT_CONTROLLER (g_object_get_data (G_OBJECT (row), "controller"));
  gtk_event_controller_set_propagation_phase (controller, phase);
}

static GtkWidget *
create_controller_widget (gpointer item,
                          gpointer user_data)
{
  GtkEventController *controller = item;
  GtkInspectorControllers *sl = user_data;
  GtkWidget *row;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *combo;

  row = gtk_list_box_row_new ();
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 40);
  gtk_container_add (GTK_CONTAINER (row), box);
  g_object_set (box, "margin", 10, NULL);
  label = gtk_label_new (G_OBJECT_TYPE_NAME (controller));
  g_object_set (label, "xalign", 0.0, NULL);
  gtk_container_add (GTK_CONTAINER (box), label);
  gtk_size_group_add_widget (sl->priv->sizegroup, label);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE);

  combo = gtk_combo_box_text_new ();
  gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (combo), GTK_PHASE_NONE, C_("event phase", "None"));
  gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (combo), GTK_PHASE_CAPTURE, C_("event phase", "Capture"));
  gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (combo), GTK_PHASE_BUBBLE, C_("event phase", "Bubble"));
  gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (combo), GTK_PHASE_TARGET, C_("event phase", "Target"));
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), gtk_event_controller_get_propagation_phase (controller));
  gtk_container_add (GTK_CONTAINER (box), combo);
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE);

  g_object_set_data (G_OBJECT (row), "controller", controller);
  g_signal_connect (combo, "changed", G_CALLBACK (phase_changed_cb), sl);

  return row;
}

void
gtk_inspector_controllers_set_object (GtkInspectorControllers *sl,
                                      GObject                 *object)
{
  GtkInspectorControllersPrivate *priv = sl->priv;

  if (!GTK_IS_WIDGET (object))
    return;

  priv->model = gtk_widget_observe_controllers (GTK_WIDGET (object));
  gtk_list_box_bind_model (GTK_LIST_BOX (priv->listbox),
                           priv->model,
                           create_controller_widget,
                           sl,
                           NULL);
  g_object_unref (priv->model);
}

static void
get_property (GObject    *object,
              guint       param_id,
              GValue     *value,
              GParamSpec *pspec)
{
  GtkInspectorControllers *sl = GTK_INSPECTOR_CONTROLLERS (object);

  switch (param_id)
    {
      case PROP_OBJECT_TREE:
        g_value_take_object (value, sl->priv->object_tree);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
set_property (GObject      *object,
              guint         param_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  GtkInspectorControllers *sl = GTK_INSPECTOR_CONTROLLERS (object);

  switch (param_id)
    {
      case PROP_OBJECT_TREE:
        sl->priv->object_tree = g_value_get_object (value);
        break;

      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        break;
    }
}

static void
gtk_inspector_controllers_class_init (GtkInspectorControllersClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = get_property;
  object_class->set_property = set_property;

  g_object_class_install_property (object_class, PROP_OBJECT_TREE,
      g_param_spec_object ("object-tree", "Widget Tree", "Widget tree",
                           GTK_TYPE_WIDGET, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

// vim: set et sw=2 ts=2:
