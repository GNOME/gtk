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
#include "gailsubmenuitem.h"

static void         gail_sub_menu_item_class_init       (GailSubMenuItemClass *klass);
static void         gail_sub_menu_item_init             (GailSubMenuItem *item);
static void         gail_sub_menu_item_real_initialize  (AtkObject      *obj,
                                                         gpointer       data);

static void         atk_selection_interface_init        (AtkSelectionIface  *iface);
static gboolean     gail_sub_menu_item_add_selection    (AtkSelection   *selection,
                                                         gint           i);
static gboolean     gail_sub_menu_item_clear_selection  (AtkSelection   *selection);
static AtkObject*   gail_sub_menu_item_ref_selection    (AtkSelection   *selection,
                                                         gint           i);
static gint         gail_sub_menu_item_get_selection_count
                                                        (AtkSelection   *selection);
static gboolean     gail_sub_menu_item_is_child_selected
                                                        (AtkSelection   *selection,
                                                         gint           i);
static gboolean     gail_sub_menu_item_remove_selection (AtkSelection   *selection,
                                                         gint           i);
static gint         menu_item_add_gtk                   (GtkContainer   *container,
                                                         GtkWidget      *widget);
static gint         menu_item_remove_gtk                (GtkContainer   *container,
                                                         GtkWidget      *widget);

G_DEFINE_TYPE_WITH_CODE (GailSubMenuItem, gail_sub_menu_item, GAIL_TYPE_MENU_ITEM,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_SELECTION, atk_selection_interface_init))

static void
gail_sub_menu_item_class_init (GailSubMenuItemClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  class->initialize = gail_sub_menu_item_real_initialize;
}

static void
gail_sub_menu_item_init (GailSubMenuItem *item)
{
}

static void
gail_sub_menu_item_real_initialize (AtkObject *obj,
                                   gpointer   data)
{
  GtkWidget *submenu;

  ATK_OBJECT_CLASS (gail_sub_menu_item_parent_class)->initialize (obj, data);

  submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (data));
  g_return_if_fail (submenu);

  g_signal_connect (submenu,
                    "add",
                    G_CALLBACK (menu_item_add_gtk),
                    NULL);
  g_signal_connect (submenu,
                    "remove",
                    G_CALLBACK (menu_item_remove_gtk),
                    NULL);

  obj->role = ATK_ROLE_MENU;
}

AtkObject*
gail_sub_menu_item_new (GtkWidget *widget)
{
  GObject *object;
  AtkObject *accessible;

  g_return_val_if_fail (GTK_IS_MENU_ITEM (widget), NULL);

  object = g_object_new (GAIL_TYPE_SUB_MENU_ITEM, NULL);

  accessible = ATK_OBJECT (object);
  atk_object_initialize (accessible, widget);

  return accessible;
}

static void
atk_selection_interface_init (AtkSelectionIface *iface)
{
  iface->add_selection = gail_sub_menu_item_add_selection;
  iface->clear_selection = gail_sub_menu_item_clear_selection;
  iface->ref_selection = gail_sub_menu_item_ref_selection;
  iface->get_selection_count = gail_sub_menu_item_get_selection_count;
  iface->is_child_selected = gail_sub_menu_item_is_child_selected;
  iface->remove_selection = gail_sub_menu_item_remove_selection;
  /*
   * select_all_selection does not make sense for a submenu of a menu item
   * so no implementation is provided.
   */
}

static gboolean
gail_sub_menu_item_add_selection (AtkSelection *selection,
                                  gint          i)
{
  GtkMenuShell *shell;
  GList *item;
  guint length;
  GtkWidget *widget;
  GtkWidget *submenu;

  widget =  GTK_ACCESSIBLE (selection)->widget;
  if (widget == NULL)
    /* State is defunct */
    return FALSE;

  submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));
  g_return_val_if_fail (GTK_IS_MENU_SHELL (submenu), FALSE);
  shell = GTK_MENU_SHELL (submenu);
  length = g_list_length (shell->children);
  if (i < 0 || i > length)
    return FALSE;

  item = g_list_nth (shell->children, i);
  g_return_val_if_fail (item != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_MENU_ITEM(item->data), FALSE);
   
  gtk_menu_shell_select_item (shell, GTK_WIDGET (item->data));
  return TRUE;
}

static gboolean
gail_sub_menu_item_clear_selection (AtkSelection   *selection)
{
  GtkMenuShell *shell;
  GtkWidget *widget;
  GtkWidget *submenu;

  widget =  GTK_ACCESSIBLE (selection)->widget;
  if (widget == NULL)
    /* State is defunct */
    return FALSE;

  submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));
  g_return_val_if_fail (GTK_IS_MENU_SHELL (submenu), FALSE);
  shell = GTK_MENU_SHELL (submenu);

  gtk_menu_shell_deselect (shell);
  return TRUE;
}

static AtkObject*
gail_sub_menu_item_ref_selection (AtkSelection   *selection,
                                  gint           i)
{
  GtkMenuShell *shell;
  AtkObject *obj;
  GtkWidget *widget;
  GtkWidget *submenu;

  if (i != 0)
    return NULL;

  widget =  GTK_ACCESSIBLE (selection)->widget;
  if (widget == NULL)
    /* State is defunct */
    return NULL;

  submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));
  g_return_val_if_fail (GTK_IS_MENU_SHELL (submenu), NULL);
  shell = GTK_MENU_SHELL (submenu);
  
  if (shell->active_menu_item != NULL)
    {
      obj = gtk_widget_get_accessible (shell->active_menu_item);
      g_object_ref (obj);
      return obj;
    }
  else
    {
      return NULL;
    }
}

static gint
gail_sub_menu_item_get_selection_count (AtkSelection   *selection)
{
  GtkMenuShell *shell;
  GtkWidget *widget;
  GtkWidget *submenu;

  widget =  GTK_ACCESSIBLE (selection)->widget;
  if (widget == NULL)
    /* State is defunct */
    return 0;

  submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));
  g_return_val_if_fail (GTK_IS_MENU_SHELL (submenu), FALSE);
  shell = GTK_MENU_SHELL (submenu);

  /*
   * Identifies the currently selected menu item
   */
  if (shell->active_menu_item == NULL)
    return 0;
  else
    return 1;
}

static gboolean
gail_sub_menu_item_is_child_selected (AtkSelection   *selection,
                                       gint           i)
{
  GtkMenuShell *shell;
  gint j;
  GtkWidget *widget;
  GtkWidget *submenu;

  widget =  GTK_ACCESSIBLE (selection)->widget;
  if (widget == NULL)
    /* State is defunct */
    return FALSE;

  submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));
  g_return_val_if_fail (GTK_IS_MENU_SHELL (submenu), FALSE);
  shell = GTK_MENU_SHELL (submenu);

  if (shell->active_menu_item == NULL)
    return FALSE;
  
  j = g_list_index (shell->children, shell->active_menu_item);

  return (j==i);   
}

static gboolean
gail_sub_menu_item_remove_selection (AtkSelection   *selection,
                                  gint           i)
{
  GtkMenuShell *shell;
  GtkWidget *widget;
  GtkWidget *submenu;

  if (i != 0)
    return FALSE;

  widget =  GTK_ACCESSIBLE (selection)->widget;
  if (widget == NULL)
    /* State is defunct */
    return FALSE;

  submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));
  g_return_val_if_fail (GTK_IS_MENU_SHELL (submenu), FALSE);
  shell = GTK_MENU_SHELL (submenu);

  if (shell->active_menu_item && 
      GTK_MENU_ITEM (shell->active_menu_item)->submenu)
    {
    /*
     * Menu item contains a menu and it is the selected menu item
     * so deselect it.
     */
      gtk_menu_shell_deselect (shell);
    }
  return TRUE;
}

static gint
menu_item_add_gtk (GtkContainer *container,
                   GtkWidget	*widget)
{
  GtkWidget *parent_widget;
  AtkObject *atk_parent;
  AtkObject *atk_child;
  GailContainer *gail_container;
  gint index;

  g_return_val_if_fail (GTK_IS_MENU (container), 1);

  parent_widget = gtk_menu_get_attach_widget (GTK_MENU (container));
  if (GTK_IS_MENU_ITEM (parent_widget))
    {
      atk_parent = gtk_widget_get_accessible (parent_widget);
      atk_child = gtk_widget_get_accessible (widget);

      gail_container = GAIL_CONTAINER (atk_parent);
      g_object_notify (G_OBJECT (atk_child), "accessible_parent");

      g_list_free (gail_container->children);
      gail_container->children = gtk_container_get_children (container);
      index = g_list_index (gail_container->children, widget);
      g_signal_emit_by_name (atk_parent, "children_changed::add",
                             index, atk_child, NULL);
    }
  return 1;
}

static gint
menu_item_remove_gtk (GtkContainer *container,
                      GtkWidget	   *widget)
{
  GtkWidget *parent_widget;
  AtkObject *atk_parent;
  AtkObject *atk_child;
  GailContainer *gail_container;
  AtkPropertyValues values = { NULL };
  gint index;
  gint list_length;

  g_return_val_if_fail (GTK_IS_MENU (container), 1);

  parent_widget = gtk_menu_get_attach_widget (GTK_MENU (container));
  if (GTK_IS_MENU_ITEM (parent_widget))
    {
      atk_parent = gtk_widget_get_accessible (parent_widget);
      atk_child = gtk_widget_get_accessible (widget);

      gail_container = GAIL_CONTAINER (atk_parent);
      g_value_init (&values.old_value, G_TYPE_POINTER);
      g_value_set_pointer (&values.old_value, atk_parent);
      values.property_name = "accessible-parent";
      g_signal_emit_by_name (atk_child,
                             "property_change::accessible-parent", &values, NULL);

      index = g_list_index (gail_container->children, widget);
      list_length = g_list_length (gail_container->children);
      g_list_free (gail_container->children);
      gail_container->children = gtk_container_get_children (container);
      if (index >= 0 && index <= list_length)
        g_signal_emit_by_name (atk_parent, "children_changed::remove",
                               index, atk_child, NULL);
    }
  return 1;
}
