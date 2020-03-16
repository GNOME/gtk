/*
 * Copyright (c) 2020 Red Hat, Inc.
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
#include "gtklabel.h"
#include "gtklistbox.h"
#include "gtksizegroup.h"
#include "gtkstack.h"
#include "gtkshortcut.h"
#include "gtkshortcuttrigger.h"
#include "gtkshortcutcontroller.h"

struct _GtkInspectorShortcuts
{
  GtkWidget parent;

  GtkWidget *box;
  GtkWidget *list;

  GtkSizeGroup *trigger;
  GtkSizeGroup *action;
};

G_DEFINE_TYPE (GtkInspectorShortcuts, gtk_inspector_shortcuts, GTK_TYPE_WIDGET)

static void
gtk_inspector_shortcuts_init (GtkInspectorShortcuts *sl)
{
  gtk_widget_init_template (GTK_WIDGET (sl));
}

static GtkWidget *
create_row (gpointer item,
            gpointer user_data)
{
  GtkShortcut *shortcut = GTK_SHORTCUT (item);
  GtkInspectorShortcuts *sl = GTK_INSPECTOR_SHORTCUTS (user_data);
  GtkShortcutTrigger *trigger;
  GtkShortcutAction *action;
  char *s;
  GtkWidget *row;
  GtkWidget *label;

  trigger = gtk_shortcut_get_trigger (shortcut);
  action = gtk_shortcut_get_action (shortcut);

  row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);

  s = gtk_shortcut_trigger_to_string (trigger);
  label = gtk_label_new (s);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  g_free (s);
  gtk_container_add (GTK_CONTAINER (row), label);
  gtk_size_group_add_widget (sl->trigger, label);

  s = gtk_shortcut_action_to_string (action);
  label = gtk_label_new (s);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  g_free (s);
  gtk_container_add (GTK_CONTAINER (row), label);
  gtk_size_group_add_widget (sl->action, label);

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

  if (GTK_IS_SHORTCUT_CONTROLLER (object))
    {
      g_object_set (page, "visible", TRUE, NULL);
      gtk_list_box_bind_model (GTK_LIST_BOX (sl->list),
                               G_LIST_MODEL (object),
                               create_row,
                               sl,
                               NULL);
    }
  else
    {
      g_object_set (page, "visible", FALSE, NULL);
      gtk_list_box_bind_model (GTK_LIST_BOX (sl->list),
                               NULL,
                               NULL,
                               NULL,
                               NULL);
    }
}

static void
gtk_inspector_shortcuts_measure (GtkWidget      *widget,
                                 GtkOrientation  orientation,
                                 int             for_size,
                                 int            *minimum,
                                 int            *natural,
                                 int            *minimum_baseline,
                                 int            *natural_baseline)
{
  GtkInspectorShortcuts *shortcuts = GTK_INSPECTOR_SHORTCUTS (widget);

  gtk_widget_measure (shortcuts->box,
                      orientation,
                      for_size,
                      minimum, natural,
                      minimum_baseline, natural_baseline);
}

static void
gtk_inspector_shortcuts_size_allocate (GtkWidget *widget,
                                       int        width,
                                       int        height,
                                       int        baseline)
{
  GtkInspectorShortcuts *shortcuts = GTK_INSPECTOR_SHORTCUTS (widget);

  gtk_widget_size_allocate (shortcuts->box,
                            &(GtkAllocation) { 0, 0, width, height },
                            baseline);
}


static void
gtk_inspector_shortcuts_class_init (GtkInspectorShortcutsClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->measure = gtk_inspector_shortcuts_measure;
  widget_class->size_allocate = gtk_inspector_shortcuts_size_allocate;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/shortcuts.ui");
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorShortcuts, box);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorShortcuts, list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorShortcuts, trigger);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorShortcuts, action);
}
