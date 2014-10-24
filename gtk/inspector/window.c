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
#include "object-tree.h"
#include "selector.h"
#include "size-groups.h"
#include "style-prop-list.h"
#include "data-list.h"
#include "signals-list.h"
#include "actions.h"
#include "menu.h"
#include "misc-info.h"
#include "gestures.h"

#include "gtklabel.h"
#include "gtkbutton.h"
#include "gtkstack.h"
#include "gtktreeviewcolumn.h"
#include "gtkwindow.h"
#include "gtkwindowgroup.h"

G_DEFINE_TYPE (GtkInspectorWindow, gtk_inspector_window, GTK_TYPE_WINDOW)

static gboolean
set_selected_object (GtkInspectorWindow *iw,
                     GObject            *selected)
{
  if (!gtk_inspector_prop_list_set_object (GTK_INSPECTOR_PROP_LIST (iw->prop_list), selected))
    return FALSE;

  gtk_inspector_prop_list_set_object (GTK_INSPECTOR_PROP_LIST (iw->child_prop_list), selected);
  gtk_inspector_style_prop_list_set_object (GTK_INSPECTOR_STYLE_PROP_LIST (iw->style_prop_list), selected);
  gtk_inspector_signals_list_set_object (GTK_INSPECTOR_SIGNALS_LIST (iw->signals_list), selected);
  gtk_inspector_object_hierarchy_set_object (GTK_INSPECTOR_OBJECT_HIERARCHY (iw->object_hierarchy), selected);
  gtk_inspector_selector_set_object (GTK_INSPECTOR_SELECTOR (iw->selector), selected);
  gtk_inspector_misc_info_set_object (GTK_INSPECTOR_MISC_INFO (iw->misc_info), selected);
  gtk_inspector_classes_list_set_object (GTK_INSPECTOR_CLASSES_LIST (iw->classes_list), selected);
  gtk_inspector_css_editor_set_object (GTK_INSPECTOR_CSS_EDITOR (iw->widget_css_editor), selected);
  gtk_inspector_size_groups_set_object (GTK_INSPECTOR_SIZE_GROUPS (iw->size_groups), selected);
  gtk_inspector_data_list_set_object (GTK_INSPECTOR_DATA_LIST (iw->data_list), selected);
  gtk_inspector_actions_set_object (GTK_INSPECTOR_ACTIONS (iw->actions), selected);
  gtk_inspector_menu_set_object (GTK_INSPECTOR_MENU (iw->menu), selected);
  gtk_inspector_gestures_set_object (GTK_INSPECTOR_GESTURES (iw->gestures), selected);

  return TRUE;
}

static void
on_object_activated (GtkInspectorObjectTree *wt,
                     GObject                *selected,
                     const gchar            *name,
                     GtkInspectorWindow     *iw)
{
  const gchar *tab;

  if (!set_selected_object (iw, selected))
    return;

  tab = g_object_get_data (G_OBJECT (wt), "next-tab");
  if (tab)
    gtk_stack_set_visible_child_name (GTK_STACK (iw->object_details), tab);

  gtk_stack_set_visible_child_name (GTK_STACK (iw->object_stack), "object-details");
  gtk_stack_set_visible_child_name (GTK_STACK (iw->object_buttons), "details");
}

static void
on_object_selected (GtkInspectorObjectTree *wt,
                    GObject                *selected,
                    GtkInspectorWindow     *iw)
{
  gtk_widget_set_sensitive (iw->object_details_button, selected != NULL);
  if (GTK_IS_WIDGET (selected))
    gtk_inspector_flash_widget (iw, GTK_WIDGET (selected));
}

static void
close_object_details (GtkWidget *button, GtkInspectorWindow *iw)
{
  gtk_stack_set_visible_child_name (GTK_STACK (iw->object_stack), "object-tree");
  gtk_stack_set_visible_child_name (GTK_STACK (iw->object_buttons), "list");
}

static void
open_object_details (GtkWidget *button, GtkInspectorWindow *iw)
{
  GObject *selected;

  selected = gtk_inspector_object_tree_get_selected (GTK_INSPECTOR_OBJECT_TREE (iw->object_tree));
 
  if (!set_selected_object (iw, selected))
    return;

  gtk_stack_set_visible_child_name (GTK_STACK (iw->object_stack), "object-details");
  gtk_stack_set_visible_child_name (GTK_STACK (iw->object_buttons), "details");
}

static void
visible_child_name_changed (GObject *obj, GParamSpec *pspec, GtkInspectorWindow *iw)
{
  const gchar *child;
  gboolean objects_visible;

  child = gtk_stack_get_visible_child_name (GTK_STACK (iw->top_stack));
  objects_visible = g_strcmp0 (child, "objects") == 0;

  gtk_widget_set_visible (iw->select_object, objects_visible);
  gtk_widget_set_visible (iw->object_buttons, objects_visible);
}

static void
gtk_inspector_window_init (GtkInspectorWindow *iw)
{
  gtk_widget_init_template (GTK_WIDGET (iw));

  gtk_window_group_add_window (gtk_window_group_new (), GTK_WINDOW (iw));

  g_signal_connect (iw->top_stack, "notify::visible-child-name",
                    G_CALLBACK (visible_child_name_changed), iw);
  g_signal_connect (iw->object_stack, "notify::visible-child-name",
                    G_CALLBACK (visible_child_name_changed), iw);
}

static void
gtk_inspector_window_constructed (GObject *object)
{
  GtkInspectorWindow *iw = GTK_INSPECTOR_WINDOW (object);

  G_OBJECT_CLASS (gtk_inspector_window_parent_class)->constructed (object);

  gtk_inspector_object_tree_scan (GTK_INSPECTOR_OBJECT_TREE (iw->object_tree), NULL);
}

static void
gtk_inspector_window_class_init (GtkInspectorWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gtk_inspector_window_constructed;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/inspector/window.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, top_stack);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, object_stack);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, object_tree);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, object_details);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, object_buttons);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, object_details_button);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, select_object);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, prop_list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, child_prop_list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, signals_list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, classes_list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, style_prop_list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, widget_css_editor);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, object_hierarchy);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, selector);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, size_groups);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, data_list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, actions);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, menu);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, misc_info);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, gestures);

  gtk_widget_class_bind_template_callback (widget_class, gtk_inspector_on_inspect);
  gtk_widget_class_bind_template_callback (widget_class, on_object_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_object_selected);
  gtk_widget_class_bind_template_callback (widget_class, open_object_details);
  gtk_widget_class_bind_template_callback (widget_class, close_object_details);
}

static GdkScreen *
get_inspector_screen (void)
{
  static GdkDisplay *display = NULL;

  if (display == NULL)
    {
      const gchar *name;
      const gchar *backends;

      backends = g_getenv ("GTK_INSPECTOR_BACKEND");

      if (backends)
        gdk_set_allowed_backends (backends);

      name = g_getenv ("GTK_INSPECTOR_DISPLAY");
      display = gdk_display_open (name);

      if (display)
        g_debug ("Using display %s for GtkInspector", name);
      else
        g_message ("Failed to open display %s", name);
    }

  if (!display)
    {
      display = gdk_display_open (NULL);
      if (display)
        g_debug ("Using default display for GtkInspector");
      else
        g_message ("Failed to separate connection to default display");
    }

  if (!display)
    display = gdk_display_get_default ();

  return gdk_display_get_default_screen (display);
}

GtkWidget *
gtk_inspector_window_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_INSPECTOR_WINDOW,
                                   "screen", get_inspector_screen (),
                                   NULL));
}

// vim: set et sw=2 ts=2:
