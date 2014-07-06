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

#include "config.h"
#include <glib/gi18n-lib.h>
#include <stdlib.h>
#include "window.h"
#include "prop-list.h"
#include "classes-list.h"
#include "css-editor.h"
#include "object-hierarchy.h"
#include "widget-tree.h"
#include "python-hooks.h"
#include "python-shell.h"
#include "button-path.h"
#include "size-groups.h"
#include "style-prop-list.h"
#include "data-list.h"
#include "signals-list.h"
#include "actions.h"
#include "menu.h"
#include "gestures.h"


G_DEFINE_TYPE (GtkInspectorWindow, gtk_inspector_window, GTK_TYPE_WINDOW)

extern void on_inspect (GtkWidget *button, GtkInspectorWindow *iw);

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
  GtkWidget *notebook;
  const gchar *tab;
  gint page_num;

  if (!gtk_inspector_prop_list_set_object (GTK_INSPECTOR_PROP_LIST (iw->prop_list), selected))
    return;

  gtk_inspector_prop_list_set_object (GTK_INSPECTOR_PROP_LIST (iw->child_prop_list), selected);
  gtk_inspector_style_prop_list_set_object (GTK_INSPECTOR_STYLE_PROP_LIST (iw->style_prop_list), selected);
  gtk_inspector_signals_list_set_object (GTK_INSPECTOR_SIGNALS_LIST (iw->signals_list), selected);
  gtk_inspector_object_hierarchy_set_object (GTK_INSPECTOR_OBJECT_HIERARCHY (iw->object_hierarchy), selected);
  gtk_inspector_button_path_set_object (GTK_INSPECTOR_BUTTON_PATH (iw->button_path), selected);
  gtk_inspector_classes_list_set_object (GTK_INSPECTOR_CLASSES_LIST (iw->classes_list), selected);
  gtk_inspector_css_editor_set_object (GTK_INSPECTOR_CSS_EDITOR (iw->widget_css_editor), selected);
  gtk_inspector_size_groups_set_object (GTK_INSPECTOR_SIZE_GROUPS (iw->size_groups), selected);
  gtk_inspector_data_list_set_object (GTK_INSPECTOR_DATA_LIST (iw->data_list), selected);
  gtk_inspector_actions_set_object (GTK_INSPECTOR_ACTIONS (iw->actions), selected);
  gtk_inspector_menu_set_object (GTK_INSPECTOR_MENU (iw->menu), selected);
  gtk_inspector_gestures_set_object (GTK_INSPECTOR_GESTURES (iw->gestures), selected);

  notebook = gtk_widget_get_parent (iw->prop_list);
  tab = g_object_get_data (G_OBJECT (wt), "next-tab");
  if (g_strcmp0 (tab, "properties") == 0)
    {
      page_num = gtk_notebook_page_num (GTK_NOTEBOOK (notebook), iw->prop_list);
      gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), page_num);
    }
  else if (g_strcmp0 (tab, "data") == 0)
    {
      page_num = gtk_notebook_page_num (GTK_NOTEBOOK (notebook), iw->data_list);
      gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), page_num);
    }
  else if (g_strcmp0 (tab, "actions") == 0)
    {
      page_num = gtk_notebook_page_num (GTK_NOTEBOOK (notebook), iw->actions);
      gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), page_num);
    }

  if (GTK_IS_WIDGET (selected))
    gtk_inspector_flash_widget (iw, GTK_WIDGET (selected));
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

  title = g_strdup_printf (_("GTK+ Inspector — %s"), g_get_application_name ());
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
gtk_inspector_window_select_initially (GtkInspectorWindow *iw)
{
  GList *toplevels, *l;
  GtkWidget *widget;

  toplevels = gtk_window_list_toplevels ();
  widget = NULL;
  for (l = toplevels; l; l = l->next)
    {
      if (gtk_widget_get_mapped (GTK_WIDGET (l->data)) &&
          GTK_IS_WINDOW (l->data) &&
          !GTK_INSPECTOR_IS_WINDOW (l->data))
        {
          widget = l->data;
          break;
        }
    }
  g_list_free (toplevels);

  if (widget)
    gtk_inspector_widget_tree_scan (GTK_INSPECTOR_WIDGET_TREE (iw->widget_tree), widget);
}

static void
gtk_inspector_window_constructed (GObject *object)
{
  gtk_inspector_window_select_initially (GTK_INSPECTOR_WINDOW (object));

  G_OBJECT_CLASS (gtk_inspector_window_parent_class)->constructed (object);
}

static void
gtk_inspector_window_class_init (GtkInspectorWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gtk_inspector_window_constructed;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/inspector/window.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, top_notebook);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, widget_tree);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, prop_list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, child_prop_list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, signals_list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, button_path);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, classes_list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, style_prop_list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, widget_css_editor);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, object_hierarchy);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, python_shell);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, widget_popup);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, size_groups);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, data_list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, actions);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, menu);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, gestures);

  gtk_widget_class_bind_template_callback (widget_class, on_inspect);
  gtk_widget_class_bind_template_callback (widget_class, on_widget_tree_selection_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_send_widget_to_shell_activate);
}

GtkWidget *
gtk_inspector_window_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_INSPECTOR_WINDOW, NULL));
}

// vim: set et sw=2 ts=2:
