/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2002, 2003 Sun Microsystems Inc.
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <gtk/gtk.h>
#include "gtksubmenuitemaccessible.h"

static gint menu_item_add_gtk    (GtkContainer   *container,
                                  GtkWidget      *widget);
static gint menu_item_remove_gtk (GtkContainer   *container,
                                  GtkWidget      *widget);

static void atk_selection_interface_init (AtkSelectionIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkSubmenuItemAccessible, gtk_submenu_item_accessible, GTK_TYPE_MENU_ITEM_ACCESSIBLE,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_SELECTION, atk_selection_interface_init))

static void
gtk_submenu_item_accessible_initialize (AtkObject *obj,
                                        gpointer   data)
{
  GtkWidget *submenu;

  ATK_OBJECT_CLASS (gtk_submenu_item_accessible_parent_class)->initialize (obj, data);

  submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (data));
  if (submenu)
    {
      g_signal_connect (submenu, "add", G_CALLBACK (menu_item_add_gtk), NULL);
      g_signal_connect (submenu, "remove", G_CALLBACK (menu_item_remove_gtk), NULL);
    }

  obj->role = ATK_ROLE_MENU;
}

static void
gtk_submenu_item_accessible_class_init (GtkSubmenuItemAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  class->initialize = gtk_submenu_item_accessible_initialize;
}

static void
gtk_submenu_item_accessible_init (GtkSubmenuItemAccessible *item)
{
}

static gboolean
gtk_submenu_item_accessible_add_selection (AtkSelection *selection,
                                           gint          i)
{
  GtkMenuShell *shell;
  GList *kids;
  guint length;
  GtkWidget *widget;
  GtkWidget *submenu;
  GtkWidget *child;

  widget =  gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return FALSE;

  submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));
  if (submenu == NULL)
    return FALSE;

  shell = GTK_MENU_SHELL (submenu);
  kids = gtk_container_get_children (GTK_CONTAINER (shell));
  length = g_list_length (kids);
  if (i < 0 || i > length)
    {
      g_list_free (kids);
      return FALSE;
    }

  child = g_list_nth_data (kids, i);
  g_list_free (kids);
  g_return_val_if_fail (GTK_IS_MENU_ITEM(child), FALSE);
  gtk_menu_shell_select_item (shell, GTK_WIDGET (child));
  return TRUE;
}

static gboolean
gtk_submenu_item_accessible_clear_selection (AtkSelection *selection)
{
  GtkWidget *widget;
  GtkWidget *submenu;

  widget =  gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return FALSE;

  submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));
  if (submenu == NULL)
    return FALSE;

  gtk_menu_shell_deselect (GTK_MENU_SHELL (submenu));

  return TRUE;
}

static AtkObject *
gtk_submenu_item_accessible_ref_selection (AtkSelection *selection,
                                           gint          i)
{
  GtkMenuShell *shell;
  AtkObject *obj;
  GtkWidget *widget;
  GtkWidget *submenu;
  GtkWidget *item;

  if (i != 0)
    return NULL;

  widget =  gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return NULL;

  submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));
  if (submenu == NULL)
    return NULL;

  shell = GTK_MENU_SHELL (submenu);

  item = gtk_menu_shell_get_selected_item (shell);
  if (item != NULL)
    {
      obj = gtk_widget_get_accessible (item);
      g_object_ref (obj);
      return obj;
    }

  return NULL;
}

static gint
gtk_submenu_item_accessible_get_selection_count (AtkSelection *selection)
{
  GtkMenuShell *shell;
  GtkWidget *widget;
  GtkWidget *submenu;

  widget =  gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return 0;

  submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));
  if (submenu == NULL)
    return 0;

  shell = GTK_MENU_SHELL (submenu);

  if (gtk_menu_shell_get_selected_item (shell) != NULL)
    return 1;

  return 0;
}

static gboolean
gtk_submenu_item_accessible_is_child_selected (AtkSelection *selection,
                                               gint          i)
{
  GtkMenuShell *shell;
  gint j;
  GtkWidget *widget;
  GtkWidget *submenu;
  GtkWidget *item;
  GList *kids;

  widget =  gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return FALSE;

  submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));
  if (submenu == NULL)
    return FALSE;

  shell = GTK_MENU_SHELL (submenu);

  item = gtk_menu_shell_get_selected_item (shell);
  if (item == NULL)
    return FALSE;

  kids = gtk_container_get_children (GTK_CONTAINER (shell));
  j = g_list_index (kids, item);
  g_list_free (kids);

  return j==i;
}

static gboolean
gtk_submenu_item_accessible_remove_selection (AtkSelection *selection,
                                              gint          i)
{
  GtkMenuShell *shell;
  GtkWidget *widget;
  GtkWidget *submenu;
  GtkWidget *item;

  if (i != 0)
    return FALSE;

  widget =  gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return FALSE;

  submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));
  if (submenu == NULL)
    return FALSE;

  shell = GTK_MENU_SHELL (submenu);

  item = gtk_menu_shell_get_selected_item (shell);
  if (item && gtk_menu_item_get_submenu (GTK_MENU_ITEM (item)))
    gtk_menu_shell_deselect (shell);

  return TRUE;
}

static void
atk_selection_interface_init (AtkSelectionIface *iface)
{
  iface->add_selection = gtk_submenu_item_accessible_add_selection;
  iface->clear_selection = gtk_submenu_item_accessible_clear_selection;
  iface->ref_selection = gtk_submenu_item_accessible_ref_selection;
  iface->get_selection_count = gtk_submenu_item_accessible_get_selection_count;
  iface->is_child_selected = gtk_submenu_item_accessible_is_child_selected;
  iface->remove_selection = gtk_submenu_item_accessible_remove_selection;
}

static gint
menu_item_add_gtk (GtkContainer *container,
                   GtkWidget    *widget)
{
  GtkWidget *parent_widget;
  AtkObject *atk_parent;
  AtkObject *atk_child;
  GtkContainerAccessible *container_accessible;
  gint index;

  g_return_val_if_fail (GTK_IS_MENU (container), 1);

  parent_widget = gtk_menu_get_attach_widget (GTK_MENU (container));
  if (GTK_IS_MENU_ITEM (parent_widget))
    {
      atk_parent = gtk_widget_get_accessible (parent_widget);
      atk_child = gtk_widget_get_accessible (widget);

      g_object_notify (G_OBJECT (atk_child), "accessible-parent");
      container_accessible = GTK_CONTAINER_ACCESSIBLE (atk_parent);
      g_list_free (container_accessible->children);
      container_accessible->children = gtk_container_get_children (container);
      index = g_list_index (container_accessible->children, widget);
      g_signal_emit_by_name (atk_parent, "children_changed::add",
                             index, atk_child, NULL);
    }
  return 1;
}

static gint
menu_item_remove_gtk (GtkContainer *container,
                      GtkWidget    *widget)
{
  GtkWidget *parent_widget;
  AtkObject *atk_parent;
  AtkObject *atk_child;
  GtkContainerAccessible *container_accessible;
  gint index;
  gint list_length;

  g_return_val_if_fail (GTK_IS_MENU (container), 1);

  parent_widget = gtk_menu_get_attach_widget (GTK_MENU (container));
  if (GTK_IS_MENU_ITEM (parent_widget))
    {
      atk_parent = gtk_widget_get_accessible (parent_widget);
      atk_child = gtk_widget_get_accessible (widget);

      g_object_notify (G_OBJECT (atk_child), "accessible-parent");

      container_accessible = GTK_CONTAINER_ACCESSIBLE (atk_parent);
      index = g_list_index (container_accessible->children, widget);
      list_length = g_list_length (container_accessible->children);
      g_list_free (container_accessible->children);
      container_accessible->children = gtk_container_get_children (container);
      if (index >= 0 && index <= list_length)
        g_signal_emit_by_name (atk_parent, "children_changed::remove",
                               index, atk_child, NULL);
    }
  return 1;
}
