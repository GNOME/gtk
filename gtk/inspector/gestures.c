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

#include "gestures.h"
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

struct _GtkInspectorGesturesPrivate
{
  GtkWidget *listbox;
  GtkSizeGroup *sizegroup;
  GtkInspectorObjectTree *object_tree;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkInspectorGestures, gtk_inspector_gestures, GTK_TYPE_BOX)

static void
row_activated (GtkListBox           *box,
               GtkListBoxRow        *row,
               GtkInspectorGestures *sl)
{
  GObject *gesture;
  
  gesture = G_OBJECT (g_object_get_data (G_OBJECT (row), "gesture"));
  gtk_inspector_object_tree_select_object (sl->priv->object_tree, gesture);
}

static void
gtk_inspector_gestures_init (GtkInspectorGestures *sl)
{
  GtkWidget *frame;

  sl->priv = gtk_inspector_gestures_get_instance_private (sl);
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
  gtk_widget_show (frame);
  gtk_widget_set_halign (frame, GTK_ALIGN_CENTER);

  sl->priv->listbox = gtk_list_box_new ();
  g_signal_connect (sl->priv->listbox, "row-activated", G_CALLBACK (row_activated), sl);
  gtk_container_add (GTK_CONTAINER (frame), sl->priv->listbox);
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (sl->priv->listbox), GTK_SELECTION_NONE);

  gtk_container_add (GTK_CONTAINER (sl), frame);

}

static void
clear_all (GtkInspectorGestures *sl)
{
  GList *children, *l;
  GtkWidget *child;

  children = gtk_container_get_children (GTK_CONTAINER (sl->priv->listbox));
  for (l = children; l; l = l->next)
    {
      child = l->data;
      gtk_container_remove (GTK_CONTAINER (sl->priv->listbox), child);
    }
  g_list_free (children);
}

static void
phase_changed_cb (GtkComboBox *combo, GtkInspectorGestures *sl)
{
  GtkWidget *row;
  GtkPropagationPhase phase;
  GtkEventController *controller;

  phase = gtk_combo_box_get_active (combo);
  row = gtk_widget_get_ancestor (GTK_WIDGET (combo), GTK_TYPE_LIST_BOX_ROW);
  controller = GTK_EVENT_CONTROLLER (g_object_get_data (G_OBJECT (row), "gesture"));
  gtk_event_controller_set_propagation_phase (controller, phase);
}

static void
add_controller (GtkInspectorGestures *sl,
                GObject              *object,
                GtkEventController   *controller)
{
  GtkWidget *row;
  GtkWidget *box;
  GtkWidget *label;
  GtkWidget *combo;

  row = gtk_list_box_row_new ();
  gtk_container_add (GTK_CONTAINER (sl->priv->listbox), row);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 40);
  gtk_container_add (GTK_CONTAINER (row), box);
  g_object_set (box, "margin", 10, NULL);
  gtk_widget_show (box);
  label = gtk_label_new (G_OBJECT_TYPE_NAME (controller));
  g_object_set (label, "xalign", 0.0, NULL);
  gtk_container_add (GTK_CONTAINER (box), label);
  gtk_size_group_add_widget (sl->priv->sizegroup, label);
  gtk_widget_show (label);
  gtk_widget_set_halign (label, GTK_ALIGN_START);
  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE);

  combo = gtk_combo_box_text_new ();
  gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (combo), GTK_PHASE_NONE, C_("event phase", "None"));
  gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (combo), GTK_PHASE_CAPTURE, C_("event phase", "Capture"));
  gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (combo), GTK_PHASE_BUBBLE, C_("event phase", "Bubble"));
  gtk_combo_box_text_insert_text (GTK_COMBO_BOX_TEXT (combo), GTK_PHASE_TARGET, C_("event phase", "Target"));
  gtk_combo_box_set_active (GTK_COMBO_BOX (combo), gtk_event_controller_get_propagation_phase (controller));
  gtk_container_add (GTK_CONTAINER (box), combo);
  gtk_widget_show (combo);
  gtk_widget_set_halign (label, GTK_ALIGN_END);
  gtk_widget_set_valign (label, GTK_ALIGN_BASELINE);

  g_object_set_data (G_OBJECT (row), "gesture", controller);
  g_signal_connect (combo, "changed", G_CALLBACK (phase_changed_cb), sl);
}

void
gtk_inspector_gestures_set_object (GtkInspectorGestures *sl,
                                   GObject              *object)
{
  GtkPropagationPhase phase;
  GList *list, *l;

  clear_all (sl);

  if (!GTK_IS_WIDGET (object))
    return;

  for (phase = GTK_PHASE_NONE; phase <= GTK_PHASE_TARGET; phase++)
    {
      list = _gtk_widget_list_controllers (GTK_WIDGET (object), phase);
      for (l = list; l; l = l->next)
        {
          add_controller (sl, object, l->data);
        }
      g_list_free (list);
    }
}

static void
get_property (GObject    *object,
              guint       param_id,
              GValue     *value,
              GParamSpec *pspec)
{
  GtkInspectorGestures *sl = GTK_INSPECTOR_GESTURES (object);

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
  GtkInspectorGestures *sl = GTK_INSPECTOR_GESTURES (object);

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
gtk_inspector_gestures_class_init (GtkInspectorGesturesClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = get_property;
  object_class->set_property = set_property;

  g_object_class_install_property (object_class, PROP_OBJECT_TREE,
      g_param_spec_object ("object-tree", "Widget Tree", "Widget tree",
                           GTK_TYPE_WIDGET, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

// vim: set et sw=2 ts=2:
