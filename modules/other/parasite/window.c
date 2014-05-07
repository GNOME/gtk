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

G_DEFINE_TYPE (ParasiteWindow, parasite_window, GTK_TYPE_WINDOW)

extern void on_inspect (GtkWidget *button, ParasiteWindow *pw);

static void
on_graphic_updates_toggled (GtkToggleButton *button,
                            ParasiteWindow  *parasite)
{
  gdk_window_set_debug_updates (gtk_toggle_button_get_active (button));
}

static gboolean
on_widget_tree_button_press (ParasiteWidgetTree *widget_tree,
                             GdkEventButton     *event,
                             ParasiteWindow     *parasite)
{
  if (event->button == 3)
    gtk_menu_popup (GTK_MENU (parasite->widget_popup), NULL, NULL,
                    NULL, NULL, event->button, event->time);

  return FALSE;
}

static void
on_widget_tree_selection_changed (ParasiteWidgetTree *wt,
                                  ParasiteWindow     *pw)
{
  GObject *selected = parasite_widget_tree_get_selected_object (wt);

  if (selected != NULL)
    {
      if (!parasite_prop_list_set_object (PARASITE_PROP_LIST (pw->prop_list), selected))
        return;

      parasite_prop_list_set_object (PARASITE_PROP_LIST (pw->child_prop_list), selected);
      parasite_object_hierarchy_set_object (PARASITE_OBJECT_HIERARCHY (pw->object_hierarchy), selected);

      if (GTK_IS_WIDGET (selected))
        {
          GtkWidget *widget = GTK_WIDGET (selected);

          gtkparasite_flash_widget (pw, widget);
          parasite_button_path_set_widget (PARASITE_BUTTON_PATH (pw->button_path), widget);
          parasite_classes_list_set_widget (PARASITE_CLASSES_LIST (pw->classes_list), widget);
          parasite_css_editor_set_widget (PARASITE_CSS_EDITOR (pw->widget_css_editor), widget);
        }
      else
        {
          gtk_widget_set_sensitive (pw->classes_list, FALSE);
          gtk_widget_set_sensitive (pw->widget_css_editor, FALSE);
        }
    }
}

static void
on_send_widget_to_shell_activate (GtkWidget      *menuitem,
                                  ParasiteWindow *parasite)
{
  gchar *str;
  GObject *object;

  object = parasite_widget_tree_get_selected_object (PARASITE_WIDGET_TREE (parasite->widget_tree));

  if (!object)
    return;

  str = g_strdup_printf ("parasite.gobj(%p)", object);
  parasite_python_shell_append_text (PARASITE_PYTHON_SHELL (parasite->python_shell),
                                     str,
                                     NULL);

  g_free (str);
  parasite_python_shell_focus (PARASITE_PYTHON_SHELL (parasite->python_shell));
}

static void
parasite_window_init (ParasiteWindow *pw)
{
  gchar *title;

  gtk_widget_init_template (GTK_WIDGET (pw));

  gtk_window_group_add_window (gtk_window_group_new (), GTK_WINDOW (pw));

  title = g_strconcat ("Parasite - ", g_get_application_name (), NULL);
  gtk_window_set_title (GTK_WINDOW (pw), title);
  g_free (title);

  if (parasite_python_is_enabled ())
    {
      gtk_widget_show (pw->python_shell);
      g_signal_connect (G_OBJECT (pw->widget_tree), "button-press-event",
                        G_CALLBACK (on_widget_tree_button_press), pw);
    }
}

static void
parasite_window_class_init (ParasiteWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/parasite/window.ui");

  gtk_widget_class_bind_template_child (widget_class, ParasiteWindow, widget_tree);
  gtk_widget_class_bind_template_child (widget_class, ParasiteWindow, prop_list);
  gtk_widget_class_bind_template_child (widget_class, ParasiteWindow, child_prop_list);
  gtk_widget_class_bind_template_child (widget_class, ParasiteWindow, button_path);
  gtk_widget_class_bind_template_child (widget_class, ParasiteWindow, classes_list);
  gtk_widget_class_bind_template_child (widget_class, ParasiteWindow, widget_css_editor);
  gtk_widget_class_bind_template_child (widget_class, ParasiteWindow, object_hierarchy);
  gtk_widget_class_bind_template_child (widget_class, ParasiteWindow, python_shell);
  gtk_widget_class_bind_template_child (widget_class, ParasiteWindow, widget_popup);

  gtk_widget_class_bind_template_callback (widget_class, on_inspect);
  gtk_widget_class_bind_template_callback (widget_class, on_graphic_updates_toggled);
  gtk_widget_class_bind_template_callback (widget_class, on_widget_tree_selection_changed);
  gtk_widget_class_bind_template_callback (widget_class, on_send_widget_to_shell_activate);
}

GtkWidget *
parasite_window_new (void)
{
  return GTK_WIDGET (g_object_new (PARASITE_TYPE_WINDOW, NULL));
}

// vim: set et sw=2 ts=2:
