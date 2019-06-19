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
#include "controllers.h"
#include "css-editor.h"
#include "css-node-tree.h"
#include "object-tree.h"
#include "size-groups.h"
#include "data-list.h"
#include "actions.h"
#include "shortcuts.h"
#include "menu.h"
#include "misc-info.h"
#include "magnifier.h"
#include "recorder.h"

#include "gdk-private.h"
#include "gskrendererprivate.h"
#include "gtkbutton.h"
#include "gtkcsswidgetnodeprivate.h"
#include "gtklabel.h"
#include "gtkmodulesprivate.h"
#include "gtkprivate.h"
#include "gtknative.h"
#include "gtkstack.h"
#include "gtktreeviewcolumn.h"
#include "gtkwindowgroup.h"
#include "gtkrevealer.h"
#include "gtklayoutmanager.h"
#include "gtkcssprovider.h"
#include "gtkstylecontext.h"


G_DEFINE_TYPE (GtkInspectorWindow, gtk_inspector_window, GTK_TYPE_WINDOW)

static gboolean
set_selected_object (GtkInspectorWindow *iw,
                     GObject            *selected)
{
  GList *l;
  char *title;

  if (!gtk_inspector_prop_list_set_object (GTK_INSPECTOR_PROP_LIST (iw->prop_list), selected))
    return FALSE;

  title = gtk_inspector_get_object_title (selected);
  gtk_label_set_label (GTK_LABEL (iw->object_title), title);
  g_free (title);

  gtk_inspector_prop_list_set_layout_child (GTK_INSPECTOR_PROP_LIST (iw->layout_prop_list), selected);
  gtk_inspector_misc_info_set_object (GTK_INSPECTOR_MISC_INFO (iw->misc_info), selected);
  gtk_inspector_css_node_tree_set_object (GTK_INSPECTOR_CSS_NODE_TREE (iw->widget_css_node_tree), selected);
  gtk_inspector_size_groups_set_object (GTK_INSPECTOR_SIZE_GROUPS (iw->size_groups), selected);
  gtk_inspector_data_list_set_object (GTK_INSPECTOR_DATA_LIST (iw->data_list), selected);
  gtk_inspector_actions_set_object (GTK_INSPECTOR_ACTIONS (iw->actions), selected);
  gtk_inspector_shortcuts_set_object (GTK_INSPECTOR_SHORTCUTS (iw->shortcuts), selected);
  gtk_inspector_menu_set_object (GTK_INSPECTOR_MENU (iw->menu), selected);
  gtk_inspector_controllers_set_object (GTK_INSPECTOR_CONTROLLERS (iw->controllers), selected);
  gtk_inspector_magnifier_set_object (GTK_INSPECTOR_MAGNIFIER (iw->magnifier), selected);

  for (l = iw->extra_pages; l != NULL; l = l->next)
    g_object_set (l->data, "object", selected, NULL);

  return TRUE;
}

static void
on_object_activated (GtkInspectorObjectTree *wt,
                     GObject                *selected,
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
notify_node (GtkInspectorCssNodeTree *cnt,
             GParamSpec              *pspec,
             GtkInspectorWindow      *iw)
{
  GtkCssNode *node;
  GtkWidget *widget = NULL;

  for (node = gtk_inspector_css_node_tree_get_node (cnt);
       node != NULL;
       node = gtk_css_node_get_parent (node))
    {
      if (!GTK_IS_CSS_WIDGET_NODE (node))
        continue;

      widget = gtk_css_widget_node_get_widget (GTK_CSS_WIDGET_NODE (node));
      if (widget != NULL)
        break;
    }
  if (widget)
    gtk_inspector_flash_widget (iw, widget);
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

static gboolean
translate_visible_child_name (GBinding     *binding,
                              const GValue *from,
                              GValue       *to,
                              gpointer      user_data)
{
  GtkInspectorWindow *iw = user_data;
  const char *name;

  name = g_value_get_string (from);

  if (gtk_stack_get_child_by_name (GTK_STACK (iw->object_start_stack), name))
    g_value_set_string (to, name);
  else
    g_value_set_string (to, "empty");

  return TRUE;
}

static void
gtk_inspector_window_init (GtkInspectorWindow *iw)
{
  GIOExtensionPoint *extension_point;
  GList *l, *extensions;

  gtk_widget_init_template (GTK_WIDGET (iw));

  g_object_bind_property_full (iw->object_details, "visible-child-name",
                               iw->object_start_stack, "visible-child-name",
                               G_BINDING_SYNC_CREATE,
                               translate_visible_child_name,
                               NULL,
                               iw,
                               NULL);

  gtk_window_group_add_window (gtk_window_group_new (), GTK_WINDOW (iw));

  extension_point = g_io_extension_point_lookup ("gtk-inspector-page");
  extensions = g_io_extension_point_get_extensions (extension_point);

  for (l = extensions; l != NULL; l = l->next)
    {
      GIOExtension *extension = l->data;
      GType type;
      GtkWidget *widget;
      const char *name;
      char *title;
      GtkWidget *button;
      gboolean use_picker;

      type = g_io_extension_get_type (extension);

      widget = g_object_new (type, NULL);

      iw->extra_pages = g_list_prepend (iw->extra_pages, widget);

      name = g_io_extension_get_name (extension);
      g_object_get (widget, "title", &title, NULL);

      if (g_object_class_find_property (G_OBJECT_GET_CLASS (widget), "use-picker"))
        g_object_get (widget, "use-picker", &use_picker, NULL);
      else
        use_picker = FALSE;

      if (use_picker)
        {
          button = gtk_button_new_from_icon_name ("find-location-symbolic");
          gtk_widget_set_focus_on_click (button, FALSE);
          gtk_widget_set_halign (button, GTK_ALIGN_START);
          gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
          g_signal_connect (button, "clicked",
                            G_CALLBACK (gtk_inspector_on_inspect), iw);
        }
      else
        button = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

      gtk_stack_add_titled (GTK_STACK (iw->top_stack), widget, name, title);
      gtk_stack_add_named (GTK_STACK (iw->button_stack), button, name);
      gtk_widget_show (widget);
      gtk_widget_show (button);

      g_free (title);
    }
}

static void
gtk_inspector_window_constructed (GObject *object)
{
  GtkInspectorWindow *iw = GTK_INSPECTOR_WINDOW (object);

  G_OBJECT_CLASS (gtk_inspector_window_parent_class)->constructed (object);

  g_object_set_data (G_OBJECT (gdk_display_get_default ()), "-gtk-inspector", iw);
}

static void
gtk_inspector_window_dispose (GObject *object)
{
  g_object_set_data (G_OBJECT (gdk_display_get_default ()), "-gtk-inspector", NULL);

  G_OBJECT_CLASS (gtk_inspector_window_parent_class)->dispose (object);
}

static void
object_details_changed (GtkWidget          *combo,
                        GParamSpec         *pspec,
                        GtkInspectorWindow *iw)
{
  gtk_stack_set_visible_child_name (GTK_STACK (iw->object_center_stack), "title");
}

static void
toggle_sidebar (GtkWidget          *button,
                GtkInspectorWindow *iw)
{
  if (gtk_revealer_get_child_revealed (GTK_REVEALER (iw->sidebar_revealer)))
    {
      gtk_revealer_set_reveal_child (GTK_REVEALER (iw->sidebar_revealer), FALSE);
      gtk_button_set_icon_name (GTK_BUTTON (button), "go-next-symbolic");
    }
  else
    {
      gtk_revealer_set_reveal_child (GTK_REVEALER (iw->sidebar_revealer), TRUE);
      gtk_button_set_icon_name (GTK_BUTTON (button), "go-previous-symbolic");
    }
}

static void
gtk_inspector_window_realize (GtkWidget *widget)
{
  GskRenderer *renderer;
  GtkCssProvider *provider;

  GTK_WIDGET_CLASS (gtk_inspector_window_parent_class)->realize (widget);

  renderer = gtk_native_get_renderer (GTK_NATIVE (widget));
  gsk_renderer_set_debug_flags (renderer, 0);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider, "/org/gtk/libgtk/inspector/inspector.css");
  gtk_style_context_add_provider_for_display (gtk_widget_get_display (widget),
                                              GTK_STYLE_PROVIDER (provider),
                                              800);
  g_object_unref (provider);
}

static void
gtk_inspector_window_class_init (GtkInspectorWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gtk_inspector_window_constructed;
  object_class->dispose = gtk_inspector_window_dispose;
  widget_class->realize = gtk_inspector_window_realize;

  g_signal_new (g_intern_static_string ("event"),
                G_OBJECT_CLASS_TYPE (object_class),
                G_SIGNAL_RUN_LAST,
                0,
                g_signal_accumulator_true_handled,
                NULL,
                NULL,
                G_TYPE_BOOLEAN,
                1,
                GDK_TYPE_EVENT);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gtk/libgtk/inspector/window.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, top_stack);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, button_stack);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, object_stack);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, object_tree);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, object_details);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, object_start_stack);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, object_center_stack);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, object_buttons);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, object_details_button);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, select_object);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, prop_list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, layout_prop_list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, widget_css_node_tree);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, widget_recorder);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, object_title);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, size_groups);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, data_list);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, actions);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, shortcuts);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, menu);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, misc_info);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, controllers);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, magnifier);
  gtk_widget_class_bind_template_child (widget_class, GtkInspectorWindow, sidebar_revealer);

  gtk_widget_class_bind_template_callback (widget_class, gtk_inspector_on_inspect);
  gtk_widget_class_bind_template_callback (widget_class, on_object_activated);
  gtk_widget_class_bind_template_callback (widget_class, on_object_selected);
  gtk_widget_class_bind_template_callback (widget_class, open_object_details);
  gtk_widget_class_bind_template_callback (widget_class, close_object_details);
  gtk_widget_class_bind_template_callback (widget_class, object_details_changed);
  gtk_widget_class_bind_template_callback (widget_class, notify_node);
  gtk_widget_class_bind_template_callback (widget_class, toggle_sidebar);
}

static GdkDisplay *
get_inspector_display (void)
{
  static GdkDisplay *display = NULL;

  if (display == NULL)
    {
      const gchar *name;

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


  if (display)
    {
      const gchar *name;

      name = g_getenv ("GTK_INSPECTOR_RENDERER");

      g_object_set_data_full (G_OBJECT (display), "gsk-renderer",
                              g_strdup (name), g_free);

      gdk_display_set_debug_flags (display, 0);
      gtk_set_display_debug_flags (display, 0);
    }

  if (!display)
    display = gdk_display_get_default ();

  if (display == gdk_display_get_default ())
    g_message ("Using default display for GtkInspector; expect some spillover");

  return display;
}

GtkWidget *
gtk_inspector_window_new (void)
{
  return GTK_WIDGET (g_object_new (GTK_TYPE_INSPECTOR_WINDOW,
                                   "display", get_inspector_display (),
                                   NULL));
}

void
gtk_inspector_window_add_overlay (GtkInspectorWindow  *iw,
                                  GtkInspectorOverlay *overlay)
{
  iw->overlays = g_list_prepend (iw->overlays, g_object_ref (overlay));

  gtk_inspector_overlay_queue_draw (overlay);
}

void
gtk_inspector_window_remove_overlay (GtkInspectorWindow  *iw,
                                     GtkInspectorOverlay *overlay)
{
  GList *item;

  item = g_list_find (iw->overlays, overlay);
  if (item == NULL)
    return;

  gtk_inspector_overlay_queue_draw (overlay);

  iw->overlays = g_list_delete_link (iw->overlays, item);
}

static GtkInspectorWindow *
gtk_inspector_window_get_for_display (GdkDisplay *display)
{
  return g_object_get_data (G_OBJECT (display), "-gtk-inspector");
}

GskRenderNode *
gtk_inspector_prepare_render (GtkWidget            *widget,
                              GskRenderer          *renderer,
                              GdkSurface           *surface,
                              const cairo_region_t *region,
                              GskRenderNode        *node)
{
  GtkInspectorWindow *iw;

  iw = gtk_inspector_window_get_for_display (gtk_widget_get_display (widget));
  if (iw == NULL)
    return node;

  /* sanity check for single-display GDK backends */
  if (GTK_WIDGET (iw) == widget)
    return node;

  gtk_inspector_recorder_record_render (GTK_INSPECTOR_RECORDER (iw->widget_recorder),
                                        widget,
                                        renderer,
                                        surface,
                                        region,
                                        node);

  if (iw->overlays)
    {
      GtkSnapshot *snapshot;
      GList *l;

      snapshot = gtk_snapshot_new ();
      gtk_snapshot_append_node (snapshot, node);

      for (l = iw->overlays; l; l = l->next)
        {
          gtk_inspector_overlay_snapshot (l->data, snapshot, node, widget);
        }

      gsk_render_node_unref (node);
      node = gtk_snapshot_free_to_node (snapshot);
    }

  return node;
}

gboolean
gtk_inspector_is_recording (GtkWidget *widget)
{
  GtkInspectorWindow *iw;

  iw = gtk_inspector_window_get_for_display (gtk_widget_get_display (widget));
  if (iw == NULL)
    return FALSE;

  /* sanity check for single-display GDK backends */
  if (GTK_WIDGET (iw) == widget)
    return FALSE;

  return gtk_inspector_recorder_is_recording (GTK_INSPECTOR_RECORDER (iw->widget_recorder));
}

gboolean
gtk_inspector_handle_event (GdkEvent *event)
{
  GtkInspectorWindow *iw;
  gboolean handled = FALSE;

  iw = gtk_inspector_window_get_for_display (gdk_event_get_display (event));
  if (iw == NULL)
    return FALSE;

  g_signal_emit_by_name (iw, "event", event, &handled);

  return handled;
}

// vim: set et sw=2 ts=2:
