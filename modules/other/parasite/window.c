/*
 * Copyright (c) 2008-2009  Christian Hammond
 * Copyright (c) 2008-2009  David Trowbridge
 * Copyright (c) 2013 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdlib.h>
#include "window.h"
#include "prop-list.h"
#include "property-cell-renderer.h"
#include "classes-list.h"
#include "css-editor.h"
#include "object-hierarchy.h"
#include "widget-tree.h"
#include "python-hooks.h"
#include "python-shell.h"
#include "button-path.h"
#include "themes.h"

G_DEFINE_TYPE (GtkInspectorWindow, gtk_inspector_window, GTK_TYPE_WINDOW)

extern void on_inspect (GtkWidget *button, GtkInspectorWindow *iw);

static void
on_graphic_updates_toggled (GtkToggleButton    *button,
                            GtkInspectorWindow *iw)
{
  gdk_window_set_debug_updates (gtk_toggle_button_get_active (button));
}

static gboolean
on_widget_tree_button_press (GtkInspectorWidgetTree *wt,
                             GdkEventButton         *event,
                             GtkInspectorWindow     *iw)
{
  if (event->button == 3)
    gtk_menu_popup (GTK_MENU (iw->widget_popup), NULL, NULL,
                    NULL, NULL, event->button, event->time);

  return FALSE;
}

static void
on_widget_tree_selection_changed (GtkInspectorWidgetTree *wt,
                                  GtkInspectorWindow     *iw)
{
  GObject *selected = gtk_inspector_widget_tree_get_selected_object (wt);

  if (selected != NULL)
    {
      if (!gtk_inspector_prop_list_set_object (GTK_INSPECTOR_PROP_LIST (iw->prop_list), selected))
        return;

      gtk_inspector_prop_list_set_object (GTK_INSPECTOR_PROP_LIST (iw->child_prop_list), selected);
      gtk_inspector_object_hierarchy_set_object (GTK_INSPECTOR_OBJECT_HIERARCHY (iw->object_hierarchy), selected);

      if (GTK_IS_WIDGET (selected))
        {
          GtkWidget *widget = GTK_WIDGET (selected);

          gtk_inspector_flash_widget (iw, widget);
          gtk_inspector_button_path_set_widget (GTK_INSPECTOR_BUTTON_PATH (iw->button_path), widget);
          gtk_inspector_classes_list_set_widget (GTK_INSPECTOR_CLASSES_LIST (iw->classes_list), widget);
          gtk_inspector_css_editor_set_widget (GTK_INSPECTOR_CSS_EDITOR (iw->widget_css_editor), widget);
        }
      else
        {
          gtk_widget_set_sensitive (iw->classes_list, FALSE);
          gtk_widget_set_sensitive (iw->widget_css_editor, FALSE);
        }
    }
}

static void
on_send_widget_to_shell_activate (GtkWidget          *menuitem,
                                  GtkInspectorWindow *iw)
{
  gchar *str;
  GObject *object;

  object = gtk_inspector_widget_tree_get_selected_object (GTK_INSPECTOR_WIDGET_TREE (iw->widget_tree));

  if (!object)
    return;

  str = g_strdup_printf ("gtk_inspector.gobj(%p)", object);
  gtk_inspector_python_shell_append_text (GTK_INSPECTOR_PYTHON_SHELL (iw->python_shell),
                                     str,
                                     NULL);

  g_free (str);
  gtk_inspector_python_shell_focus (GTK_INSPECTOR_PYTHON_SHELL (iw->python_shell));
}

static void
gtk_inspector_window_init (GtkInspectorWindow *iw)
{
  gchar *title;

  gtk_widget_init_template (GTK_WIDGET (iw));

  gtk_window_group_add_window (gtk_window_group_new (), GTK_WINDOW (iw));

  title = g_strdup_printf ("GTK+ Inspector — %s", g_get_application_name ());
  gtk_window_set_title (GTK_WINDOW (iw), title);
  g_free (title);

  if (gtk_inspector_python_is_enabled ())
    {
      gtk_widget_show (iw->python_shell);
      g_signal_connect (G_OBJECT (iw->widget_tree), "button-press-event",
                        G_CALLBACK (on_widget_tree_button_press), iw);
    }
}

static void
gtk_inspector_window_class_init (GtkInspectorWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/inspector/window.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, widget_tree);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, prop_list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, child_prop_list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, button_path);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, classes_list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, widget_css_editor);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, object_hierarchy);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, python_shell);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, widget_popup);

  gtk_widget_class_bind_template_callback (widget_class, on_inspect);
  gtk_widget_class_bind_template_callback (widget_class, on_graphic_updates_toggled);
  gtk_widget_class_bind_template_callback (widget_class, on_widget_tree_selection_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_send_widget_to_shell_activate);
}

GtkWidget *
gtk_inspector_window_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_INSPECTOR_WINDOW, NULL));
}

// vim: set et sw=2 ts=2:
