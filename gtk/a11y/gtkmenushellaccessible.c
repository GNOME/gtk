/* GTK+ - accessibility implementations
 * Copyright 2001 Sun Microsystems Inc.
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

#include <gtk/gtk.h>
#include "gtkmenushellaccessible.h"


static void atk_selection_interface_init (AtkSelectionIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkMenuShellAccessible, gtk_menu_shell_accessible, GTK_TYPE_CONTAINER_ACCESSIBLE,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_SELECTION, atk_selection_interface_init))

static void
gtk_menu_shell_accessible_initialize (AtkObject *accessible,
                                      gpointer   data)
{
  ATK_OBJECT_CLASS (gtk_menu_shell_accessible_parent_class)->initialize (accessible, data);

  accessible->role = ATK_ROLE_UNKNOWN;
}

static void
gtk_menu_shell_accessible_class_init (GtkMenuShellAccessibleClass *klass)
{
  AtkObjectClass *atk_object_class = ATK_OBJECT_CLASS (klass);

  atk_object_class->initialize = gtk_menu_shell_accessible_initialize;
}

static void
gtk_menu_shell_accessible_init (GtkMenuShellAccessible *menu_shell)
{
}

static gboolean
gtk_menu_shell_accessible_add_selection (AtkSelection *selection,
                                         gint          i)
{
  GList *kids;
  GtkWidget *item;
  guint length;
  GtkWidget *widget;

  widget =  gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return FALSE;

  kids = gtk_container_get_children (GTK_CONTAINER (widget));
  length = g_list_length (kids);
  if (i < 0 || i > length)
    {
      g_list_free (kids);
      return FALSE;
    }

  item = g_list_nth_data (kids, i);
  g_list_free (kids);
  g_return_val_if_fail (GTK_IS_MENU_ITEM (item), FALSE);
  gtk_menu_shell_select_item (GTK_MENU_SHELL (widget), item);
  return TRUE;
}

static gboolean
gtk_menu_shell_accessible_clear_selection (AtkSelection *selection)
{
  GtkMenuShell *shell;
  GtkWidget *widget;

  widget =  gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return FALSE;

  shell = GTK_MENU_SHELL (widget);

  gtk_menu_shell_deselect (shell);
  return TRUE;
}

static AtkObject *
gtk_menu_shell_accessible_ref_selection (AtkSelection *selection,
                                         gint          i)
{
  GtkMenuShell *shell;
  AtkObject *obj;
  GtkWidget *widget;
  GtkWidget *item;

  widget =  gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return NULL;

  if (i != 0)
    return NULL;

  shell = GTK_MENU_SHELL (widget);

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
gtk_menu_shell_accessible_get_selection_count (AtkSelection *selection)
{
  GtkMenuShell *shell;
  GtkWidget *widget;

  widget =  gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return 0;

  shell = GTK_MENU_SHELL (widget);

  if (gtk_menu_shell_get_selected_item (shell) != NULL)
    return 1;

  return 0;
}

static gboolean
gtk_menu_shell_accessible_is_child_selected (AtkSelection *selection,
                                             gint          i)
{
  GtkMenuShell *shell;
  GList *kids;
  gint j;
  GtkWidget *widget;
  GtkWidget *item;

  widget =  gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return FALSE;

  shell = GTK_MENU_SHELL (widget);
  item = gtk_menu_shell_get_selected_item (shell);
  if (item == NULL)
    return FALSE;

  kids = gtk_container_get_children (GTK_CONTAINER (shell));
  j = g_list_index (kids, item);
  g_list_free (kids);

  return j==i;
}

static gboolean
gtk_menu_shell_accessible_remove_selection (AtkSelection *selection,
                                            gint          i)
{
  GtkMenuShell *shell;
  GtkWidget *widget;
  GtkWidget *item;

  widget =  gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return FALSE;

  if (i != 0)
    return FALSE;

  shell = GTK_MENU_SHELL (widget);

  item = gtk_menu_shell_get_selected_item (shell);
  if (item && gtk_menu_item_get_submenu (GTK_MENU_ITEM (item)))
    gtk_menu_shell_deselect (shell);
  return TRUE;
}

static void
atk_selection_interface_init (AtkSelectionIface *iface)
{
  iface->add_selection = gtk_menu_shell_accessible_add_selection;
  iface->clear_selection = gtk_menu_shell_accessible_clear_selection;
  iface->ref_selection = gtk_menu_shell_accessible_ref_selection;
  iface->get_selection_count = gtk_menu_shell_accessible_get_selection_count;
  iface->is_child_selected = gtk_menu_shell_accessible_is_child_selected;
  iface->remove_selection = gtk_menu_shell_accessible_remove_selection;
}
