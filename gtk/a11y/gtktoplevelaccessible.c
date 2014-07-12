/* GTK+ - accessibility implementations
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
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

#include <stdlib.h>
#include <string.h>

#include <gtk/gtkeventbox.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkplug.h>
#include <gtk/gtkwindow.h>

#include "gtktoplevelaccessible.h"

struct _GtkToplevelAccessiblePrivate
{
  GList *window_list;
};

G_DEFINE_TYPE_WITH_PRIVATE (GtkToplevelAccessible, gtk_toplevel_accessible, ATK_TYPE_OBJECT)

static void
gtk_toplevel_accessible_initialize (AtkObject *accessible,
                                    gpointer   data)
{
  ATK_OBJECT_CLASS (gtk_toplevel_accessible_parent_class)->initialize (accessible, data);

  accessible->role = ATK_ROLE_APPLICATION;
  accessible->accessible_parent = NULL;
}

static void
gtk_toplevel_accessible_object_finalize (GObject *obj)
{
  GtkToplevelAccessible *toplevel = GTK_TOPLEVEL_ACCESSIBLE (obj);

  if (toplevel->priv->window_list)
    g_list_free (toplevel->priv->window_list);

  G_OBJECT_CLASS (gtk_toplevel_accessible_parent_class)->finalize (obj);
}

static gint
gtk_toplevel_accessible_get_n_children (AtkObject *obj)
{
  GtkToplevelAccessible *toplevel = GTK_TOPLEVEL_ACCESSIBLE (obj);

  return g_list_length (toplevel->priv->window_list);
}

static AtkObject *
gtk_toplevel_accessible_ref_child (AtkObject *obj,
                                   gint       i)
{
  GtkToplevelAccessible *toplevel;
  GtkWidget *widget;
  AtkObject *atk_obj;

  toplevel = GTK_TOPLEVEL_ACCESSIBLE (obj);
  widget = g_list_nth_data (toplevel->priv->window_list, i);
  if (!widget)
    return NULL;

  atk_obj = gtk_widget_get_accessible (widget);

  g_object_ref (atk_obj);

  return atk_obj;
}

static const char *
gtk_toplevel_accessible_get_name (AtkObject *obj)
{
  return g_get_prgname ();
}

static gboolean
is_combo_window (GtkWidget *widget)
{
  GtkWidget *child;
  AtkObject *obj;

  child = gtk_bin_get_child (GTK_BIN (widget));

  if (!GTK_IS_EVENT_BOX (child))
    return FALSE;

  child = gtk_bin_get_child (GTK_BIN (child));

  if (!GTK_IS_FRAME (child))
    return FALSE;

  child = gtk_bin_get_child (GTK_BIN (child));

  if (!GTK_IS_SCROLLED_WINDOW (child))
    return FALSE;

  obj = gtk_widget_get_accessible (child);
  obj = atk_object_get_parent (obj);

  return FALSE;
}

static gboolean
is_attached_menu_window (GtkWidget *widget)
{
  GtkWidget *child;

  child = gtk_bin_get_child (GTK_BIN (widget));
  if (GTK_IS_MENU (child))
    {
      GtkWidget *attach;

      attach = gtk_menu_get_attach_widget (GTK_MENU (child));
      /* Allow for menu belonging to the Panel Menu, which is a GtkButton */
      if (GTK_IS_MENU_ITEM (attach) || GTK_IS_BUTTON (attach))
        return TRUE;
    }

  return FALSE;
}

static void
gtk_toplevel_accessible_class_init (GtkToplevelAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS(klass);
  GObjectClass *g_object_class = G_OBJECT_CLASS(klass);

  class->initialize = gtk_toplevel_accessible_initialize;
  class->get_n_children = gtk_toplevel_accessible_get_n_children;
  class->ref_child = gtk_toplevel_accessible_ref_child;
  class->get_parent = NULL;
  class->get_name = gtk_toplevel_accessible_get_name;

  g_object_class->finalize = gtk_toplevel_accessible_object_finalize;
}

static void
remove_child (GtkToplevelAccessible *toplevel,
              GtkWindow             *window)
{
  AtkObject *atk_obj = ATK_OBJECT (toplevel);
  GList *l;
  guint window_count = 0;
  AtkObject *child;

  if (toplevel->priv->window_list)
    {
      GtkWindow *tmp_window;

      for (l = toplevel->priv->window_list; l; l = l->next)
        {
          tmp_window = GTK_WINDOW (l->data);

          if (window == tmp_window)
            {
              /* Remove the window from the window_list & emit the signal */
              toplevel->priv->window_list = g_list_delete_link (toplevel->priv->window_list, l);
              child = gtk_widget_get_accessible (GTK_WIDGET (window));
              g_signal_emit_by_name (atk_obj, "children-changed::remove",
                                     window_count, child, NULL);
              atk_object_set_parent (child, NULL);
              break;
            }

          window_count++;
        }
    }
}

static gboolean
show_event_watcher (GSignalInvocationHint *ihint,
                    guint                  n_param_values,
                    const GValue          *param_values,
                    gpointer               data)
{
  GtkToplevelAccessible *toplevel = GTK_TOPLEVEL_ACCESSIBLE (data);
  AtkObject *atk_obj = ATK_OBJECT (toplevel);
  GObject *object;
  GtkWidget *widget;
  gint n_children;
  AtkObject *child;

  object = g_value_get_object (param_values + 0);

  if (!GTK_IS_WINDOW (object))
    return TRUE;

  widget = GTK_WIDGET (object);
  if (gtk_widget_get_parent (widget) ||
      is_attached_menu_window (widget) ||
#ifdef GDK_WINDOWING_X11
      GTK_IS_PLUG (widget) ||
#endif
      is_combo_window (widget))
    return TRUE;

  child = gtk_widget_get_accessible (widget);
  if (atk_object_get_role (child) == ATK_ROLE_REDUNDANT_OBJECT ||
      atk_object_get_role (child) == ATK_ROLE_TOOL_TIP)
    return TRUE;

  /* Add the window to the list & emit the signal */
  toplevel->priv->window_list = g_list_append (toplevel->priv->window_list, widget);
  n_children = g_list_length (toplevel->priv->window_list);

  atk_object_set_parent (child, atk_obj);
  g_signal_emit_by_name (atk_obj, "children-changed::add",
                         n_children - 1, child, NULL);

  g_signal_connect_swapped (G_OBJECT(object), "destroy",
                            G_CALLBACK (remove_child), toplevel);

  return TRUE;
}

static gboolean
hide_event_watcher (GSignalInvocationHint *ihint,
                    guint                  n_param_values,
                    const GValue          *param_values,
                    gpointer               data)
{
  GtkToplevelAccessible *toplevel = GTK_TOPLEVEL_ACCESSIBLE (data);
  GObject *object;

  object = g_value_get_object (param_values + 0);

  if (!GTK_IS_WINDOW (object))
    return TRUE;

  remove_child (toplevel, GTK_WINDOW (object));
  return TRUE;
}

static void
gtk_toplevel_accessible_init (GtkToplevelAccessible *toplevel)
{
  GtkWindow *window;
  GtkWidget *widget;
  GList *l;
  guint signal_id;

  toplevel->priv = gtk_toplevel_accessible_get_instance_private (toplevel);

  l = toplevel->priv->window_list = gtk_window_list_toplevels ();

  while (l)
    {
      window = GTK_WINDOW (l->data);
      widget = GTK_WIDGET (window);
      if (!window ||
          !gtk_widget_get_visible (widget) ||
          is_attached_menu_window (widget) ||
#ifdef GDK_WINDOWING_X11
          GTK_IS_PLUG (window) ||
#endif
          gtk_widget_get_parent (GTK_WIDGET (window)))
        {
          GList *temp_l  = l->next;

          toplevel->priv->window_list = g_list_delete_link (toplevel->priv->window_list, l);
          l = temp_l;
        }
      else
        {
          g_signal_connect_swapped (G_OBJECT (window), "destroy",
                                    G_CALLBACK (remove_child), toplevel);
          l = l->next;
        }
    }

  g_type_class_ref (GTK_TYPE_WINDOW);

  signal_id  = g_signal_lookup ("show", GTK_TYPE_WINDOW);
  g_signal_add_emission_hook (signal_id, 0,
                              show_event_watcher, toplevel, (GDestroyNotify) NULL);

  signal_id  = g_signal_lookup ("hide", GTK_TYPE_WINDOW);
  g_signal_add_emission_hook (signal_id, 0,
                              hide_event_watcher, toplevel, (GDestroyNotify) NULL);
}

/**
 * gtk_toplevel_accessible_get_children:
 *
 * Returns: (transfer none) (element-type Gtk.Window): List of
 *   children.
 */
GList *
gtk_toplevel_accessible_get_children (GtkToplevelAccessible *accessible)
{
  return accessible->priv->window_list;
}
