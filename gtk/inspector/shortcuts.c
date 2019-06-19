/*
 * Copyright (c) 2019 Red Hat, Inc.
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

#include "shortcuts.h"

#include "gtkliststore.h"
#include "gtkwidgetprivate.h"
#include "gtkpopover.h"
#include "gtklabel.h"
#include "gtkstack.h"
#include "gtklistbox.h"
#include "gtkstylecontext.h"
#include "gtksizegroup.h"
#include "gtkshortcutaction.h"
#include "gtkshortcuttrigger.h"
#include "gtkshortcutcontroller.h"
#include "gtkshortcut.h"

struct _GtkInspectorShortcuts
{
  GtkBox parent;
  GtkWidget *list;
  GtkSizeGroup *name;
  GtkSizeGroup *trigger;
  GtkSizeGroup *action;
  GtkSizeGroup *arguments;
  GtkShortcutController *controller;
};

typedef GtkBoxClass GtkInspectorShortcutsClass;

G_DEFINE_TYPE (GtkInspectorShortcuts, gtk_inspector_shortcuts, GTK_TYPE_BOX)

static void
gtk_inspector_shortcuts_init (GtkInspectorShortcuts *sl)
{
  gtk_widget_init_template (GTK_WIDGET (sl));
}

static GtkWidget *
create_row (gpointer item,
            gpointer user_data)
{
  GtkShortcut *shortcut = item;
  GtkInspectorShortcuts *sl = user_data;
  GtkShortcutTrigger *trigger;
  GtkShortcutAction *action;
  GVariant *arguments;
  GtkWidget *row;
  GtkWidget *box;
  GtkWidget *label;
  char *str;

  trigger = gtk_shortcut_get_trigger (shortcut);
  action = gtk_shortcut_get_action (shortcut);
  arguments = gtk_shortcut_get_arguments (shortcut);

  row = gtk_list_box_row_new ();
  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (row), box);

  if (trigger)
    str = gtk_shortcut_trigger_to_string (trigger);
  else
    str = NULL;
  label = gtk_label_new (str);
  gtk_style_context_add_class (gtk_widget_get_style_context (label), "cell");
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_size_group_add_widget (sl->trigger, label);
  gtk_container_add (GTK_CONTAINER (box), label);
  g_free (str);

  if (action)
    str = gtk_shortcut_action_to_string (action);
  else
    str = NULL;
  label = gtk_label_new (str);
  gtk_style_context_add_class (gtk_widget_get_style_context (label), "cell");
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
  gtk_size_group_add_widget (sl->action, label);
  gtk_container_add (GTK_CONTAINER (box), label);

  if (arguments)
    str = g_variant_print (arguments, FALSE);
  else
    str = NULL;
  label = gtk_label_new (str);
  gtk_style_context_add_class (gtk_widget_get_style_context (label), "cell");
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_size_group_add_widget (sl->arguments, label);
  gtk_container_add (GTK_CONTAINER (box), label);

  return row;
}

void
gtk_inspector_shortcuts_set_object (GtkInspectorShortcuts *sl,
                                    GObject               *object)
{
  GtkWidget *stack;
  GtkStackPage *page;

  stack = gtk_widget_get_parent (GTK_WIDGET (sl));
  page = gtk_stack_get_page (GTK_STACK (stack), GTK_WIDGET (sl));

  if (sl->controller)
    {
      g_object_set (page, "visible", FALSE, NULL);
      gtk_list_box_bind_model (GTK_LIST_BOX (sl->list),
                               NULL,
                               create_row,
                               sl,
                               NULL);
      g_clear_object (&sl->controller);
    }

  if (GTK_IS_SHORTCUT_CONTROLLER (object))
    {
      GListModel *model;
      g_set_object (&sl->controller, GTK_SHORTCUT_CONTROLLER (object));
      g_object_get (object, "model", &model, NULL);
      gtk_list_box_bind_model (GTK_LIST_BOX (sl->list),
                               model,
                               create_row,
                               sl,
                               NULL);
      g_object_set (page, "visible", TRUE, NULL);
      g_object_unref (model);
    }
}

static void
gtk_inspector_shortcuts_class_init (GtkInspectorShortcutsClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/shortcuts.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkInspectorShortcuts, list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorShortcuts, trigger);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorShortcuts, action);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorShortcuts, arguments);
}

// vim: set et sw=2 ts=2:
