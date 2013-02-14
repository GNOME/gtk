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

#include <glib/gi18n-lib.h>
#include <string.h>
#include <gtk/gtk.h>
#include "gtkmenuitemaccessible.h"
#include "gtkwidgetaccessibleprivate.h"
#include "gtk/gtkmenuitemprivate.h"

struct _GtkMenuItemAccessiblePrivate
{
  gchar *text;
  gboolean selected;
};

#define KEYBINDING_SEPARATOR ";"

static void menu_item_select   (GtkMenuItem *item);
static void menu_item_deselect (GtkMenuItem *item);

static GtkWidget *get_label_from_container   (GtkWidget *container);
static gchar     *get_text_from_label_widget (GtkWidget *widget);

static gint menu_item_add_gtk    (GtkContainer   *container,
                                  GtkWidget      *widget);
static gint menu_item_remove_gtk (GtkContainer   *container,
                                  GtkWidget      *widget);

static void atk_action_interface_init    (AtkActionIface *iface);
static void atk_selection_interface_init (AtkSelectionIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkMenuItemAccessible, gtk_menu_item_accessible, GTK_TYPE_CONTAINER_ACCESSIBLE,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, atk_action_interface_init);
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_SELECTION, atk_selection_interface_init))

static void
gtk_menu_item_accessible_initialize (AtkObject *obj,
                                     gpointer   data)
{
  GtkWidget *widget;
  GtkWidget *parent;
  GtkWidget *menu;

  ATK_OBJECT_CLASS (gtk_menu_item_accessible_parent_class)->initialize (obj, data);

  g_signal_connect (data, "select", G_CALLBACK (menu_item_select), NULL);
  g_signal_connect (data, "deselect", G_CALLBACK (menu_item_deselect), NULL);

  widget = GTK_WIDGET (data);
  parent = gtk_widget_get_parent (widget);
  if (GTK_IS_MENU (parent))
    {
      GtkWidget *parent_widget;

      parent_widget =  gtk_menu_get_attach_widget (GTK_MENU (parent));

      if (!GTK_IS_MENU_ITEM (parent_widget))
        parent_widget = gtk_widget_get_parent (widget);
      if (parent_widget)
        atk_object_set_parent (obj, gtk_widget_get_accessible (parent_widget));
    }

  _gtk_widget_accessible_set_layer (GTK_WIDGET_ACCESSIBLE (obj), ATK_LAYER_POPUP);

  obj->role = ATK_ROLE_MENU_ITEM;

  menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (data));
  if (menu)
    {
      g_signal_connect (menu, "add", G_CALLBACK (menu_item_add_gtk), NULL);
      g_signal_connect (menu, "remove", G_CALLBACK (menu_item_remove_gtk), NULL);
    }
}

static gint
gtk_menu_item_accessible_get_n_children (AtkObject *obj)
{
  GtkWidget *widget;
  GtkWidget *submenu;
  gint count = 0;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return count;

  submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));
  if (submenu)
    {
      GList *children;

      children = gtk_container_get_children (GTK_CONTAINER (submenu));
      count = g_list_length (children);
      g_list_free (children);
    }
  return count;
}

static AtkObject *
gtk_menu_item_accessible_ref_child (AtkObject *obj,
                                    gint       i)
{
  AtkObject  *accessible;
  GtkWidget *widget;
  GtkWidget *submenu;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return NULL;

  accessible = NULL;
  submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));
  if (submenu)
    {
      GList *children;
      GList *tmp_list;

      children = gtk_container_get_children (GTK_CONTAINER (submenu));
      tmp_list = g_list_nth (children, i);
      if (tmp_list)
        {
          accessible = gtk_widget_get_accessible (GTK_WIDGET (tmp_list->data));
          g_object_ref (accessible);
        }
      g_list_free (children);
    }

  return accessible;
}

static AtkStateSet *
gtk_menu_item_accessible_ref_state_set (AtkObject *obj)
{
  AtkObject *menu_item;
  AtkStateSet *state_set, *parent_state_set;

  state_set = ATK_OBJECT_CLASS (gtk_menu_item_accessible_parent_class)->ref_state_set (obj);

  atk_state_set_add_state (state_set, ATK_STATE_SELECTABLE);
  if (GTK_MENU_ITEM_ACCESSIBLE (obj)->priv->selected)
    atk_state_set_add_state (state_set, ATK_STATE_SELECTED);

  menu_item = atk_object_get_parent (obj);

  if (menu_item)
    {
      if (!GTK_IS_MENU_ITEM (gtk_accessible_get_widget (GTK_ACCESSIBLE (menu_item))))
        return state_set;

      parent_state_set = atk_object_ref_state_set (menu_item);
      if (!atk_state_set_contains_state (parent_state_set, ATK_STATE_SELECTED))
        {
          atk_state_set_remove_state (state_set, ATK_STATE_FOCUSED);
          atk_state_set_remove_state (state_set, ATK_STATE_SHOWING);
        }
      g_object_unref (parent_state_set);
    }

  return state_set;
}

static AtkRole
gtk_menu_item_accessible_get_role (AtkObject *obj)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget != NULL &&
      gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget)))
    return ATK_ROLE_MENU;

  return ATK_OBJECT_CLASS (gtk_menu_item_accessible_parent_class)->get_role (obj);
}

static const gchar *
gtk_menu_item_accessible_get_name (AtkObject *obj)
{
  const gchar *name;
  GtkWidget *widget;
  GtkWidget *label;
  GtkMenuItemAccessible *accessible;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return NULL;

  name = ATK_OBJECT_CLASS (gtk_menu_item_accessible_parent_class)->get_name (obj);
  if (name)
    return name;

  accessible = GTK_MENU_ITEM_ACCESSIBLE (obj);
  label = get_label_from_container (widget);

  g_free (accessible->priv->text);
  accessible->priv->text = get_text_from_label_widget (label);

  return accessible->priv->text;
}

static void
gtk_menu_item_accessible_finalize (GObject *object)
{
  GtkMenuItemAccessible *accessible = GTK_MENU_ITEM_ACCESSIBLE (object);

  g_free (accessible->priv->text);

  G_OBJECT_CLASS (gtk_menu_item_accessible_parent_class)->finalize (object);
}

static void
gtk_menu_item_accessible_notify_gtk (GObject    *obj,
                                     GParamSpec *pspec)
{
  AtkObject* atk_obj;

  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (obj));

  if (strcmp (pspec->name, "label") == 0)
    {
      if (atk_obj->name == NULL)
        g_object_notify (G_OBJECT (atk_obj), "accessible-name");
      g_signal_emit_by_name (atk_obj, "visible-data-changed");
    }
  else
    GTK_WIDGET_ACCESSIBLE_CLASS (gtk_menu_item_accessible_parent_class)->notify_gtk (obj, pspec);
}

static void
gtk_menu_item_accessible_class_init (GtkMenuItemAccessibleClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GtkWidgetAccessibleClass *widget_class = (GtkWidgetAccessibleClass*)klass;

  widget_class->notify_gtk = gtk_menu_item_accessible_notify_gtk;

  gobject_class->finalize = gtk_menu_item_accessible_finalize;

  class->get_n_children = gtk_menu_item_accessible_get_n_children;
  class->ref_child = gtk_menu_item_accessible_ref_child;
  class->ref_state_set = gtk_menu_item_accessible_ref_state_set;
  class->initialize = gtk_menu_item_accessible_initialize;
  class->get_name = gtk_menu_item_accessible_get_name;
  class->get_role = gtk_menu_item_accessible_get_role;

  g_type_class_add_private (klass, sizeof (GtkMenuItemAccessiblePrivate));
}

static void
gtk_menu_item_accessible_init (GtkMenuItemAccessible *menu_item)
{
  menu_item->priv = G_TYPE_INSTANCE_GET_PRIVATE (menu_item,
                                                 GTK_TYPE_MENU_ITEM_ACCESSIBLE,
                                                 GtkMenuItemAccessiblePrivate);
}

static GtkWidget *
get_label_from_container (GtkWidget *container)
{
  GtkWidget *label;
  GList *children, *tmp_list;

  if (!GTK_IS_CONTAINER (container))
    return NULL;

  children = gtk_container_get_children (GTK_CONTAINER (container));
  label = NULL;

  for (tmp_list = children; tmp_list != NULL; tmp_list = tmp_list->next)
    {
      if (GTK_IS_LABEL (tmp_list->data))
        {
          label = tmp_list->data;
          break;
        }
      else if (GTK_IS_CELL_VIEW (tmp_list->data))
        {
          label = tmp_list->data;
          break;
        }
      else if (GTK_IS_BOX (tmp_list->data))
        {
          label = get_label_from_container (GTK_WIDGET (tmp_list->data));
          if (label)
            break;
        }
    }
  g_list_free (children);

  return label;
}

static gchar *
get_text_from_label_widget (GtkWidget *label)
{
  if (GTK_IS_LABEL (label))
    return g_strdup (gtk_label_get_text (GTK_LABEL (label)));
  else if (GTK_IS_CELL_VIEW (label))
    {
      GList *cells, *l;
      GtkTreeModel *model;
      GtkTreeIter iter;
      GtkTreePath *path;
      GtkCellArea *area;
      gchar *text;

      model = gtk_cell_view_get_model (GTK_CELL_VIEW (label));
      path = gtk_cell_view_get_displayed_row (GTK_CELL_VIEW (label));
      gtk_tree_model_get_iter (model, &iter, path);
      gtk_tree_path_free (path);

      area = gtk_cell_layout_get_area (GTK_CELL_LAYOUT (label));
      gtk_cell_area_apply_attributes (area, model, &iter, FALSE, FALSE);
      cells = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (label));

      text = NULL;
      for (l = cells; l; l = l->next)
        {
          GtkCellRenderer *cell = l->data;

          if (GTK_IS_CELL_RENDERER_TEXT (cell))
            {
              g_object_get (cell, "text", &text, NULL);
              break;
            }
        }

      g_list_free (cells);

      return text;
    }

  return NULL;
}

static void
ensure_menus_unposted (GtkMenuItemAccessible *menu_item)
{
  AtkObject *parent;
  GtkWidget *widget;

  parent = atk_object_get_parent (ATK_OBJECT (menu_item));
  while (parent)
    {
      widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (parent));
      if (GTK_IS_MENU (widget))
        {
          if (gtk_widget_get_mapped (widget))
            gtk_menu_shell_cancel (GTK_MENU_SHELL (widget));

          return;
        }
      parent = atk_object_get_parent (parent);
    }
}

static gboolean
gtk_menu_item_accessible_do_action (AtkAction *action,
                                    gint       i)
{
  GtkWidget *item, *item_parent;
  gboolean item_mapped;

  item = gtk_accessible_get_widget (GTK_ACCESSIBLE (action));
  if (item == NULL)
    return FALSE;

  if (i != 0)
    return FALSE;

  if (!gtk_widget_get_sensitive (item) || !gtk_widget_get_visible (item))
    return FALSE;

  item_parent = gtk_widget_get_parent (item);
  if (!GTK_IS_MENU_SHELL (item_parent))
    return FALSE;

  gtk_menu_shell_select_item (GTK_MENU_SHELL (item_parent), item);
  item_mapped = gtk_widget_get_mapped (item);

  /* This is what is called when <Return> is pressed for a menu item.
   * The last argument means 'force hide'.
   */
  g_signal_emit_by_name (item_parent, "activate-current", 1);
  if (!item_mapped)
    ensure_menus_unposted (GTK_MENU_ITEM_ACCESSIBLE (action));

  return TRUE;
}

static gint
gtk_menu_item_accessible_get_n_actions (AtkAction *action)
{
  GtkWidget *item;

  item = gtk_accessible_get_widget (GTK_ACCESSIBLE (action));
  if (item == NULL)
    return 0;

  if (!_gtk_menu_item_is_selectable (item))
    return 0;

  return 1;
}

static const gchar *
gtk_menu_item_accessible_action_get_name (AtkAction *action,
                                          gint       i)
{
  if (i == 0 && gtk_menu_item_accessible_get_n_actions (action) > 0)
    return "click";
  return NULL;
}

static const gchar *
gtk_menu_item_accessible_action_get_localized_name (AtkAction *action,
                                                    gint       i)
{
  if (i == 0 && gtk_menu_item_accessible_get_n_actions (action) > 0)
    return C_("Action name", "Click");
  return NULL;
}

static const gchar *
gtk_menu_item_accessible_action_get_description (AtkAction *action,
                                                 gint       i)
{
  if (i == 0 && gtk_menu_item_accessible_get_n_actions (action) > 0)
    return C_("Action description", "Clicks the menuitem");
  return NULL;
}

static gboolean
find_accel_by_widget (GtkAccelKey *key,
                      GClosure    *closure,
                      gpointer     data)
{
  /* We assume that closure->data points to the widget
   * pending gtk_widget_get_accel_closures being made public
   */
  return data == (gpointer) closure->data;
}

static gboolean
find_accel_by_closure (GtkAccelKey *key,
                       GClosure    *closure,
                       gpointer     data)
{
  return data == (gpointer) closure;
}

/* This function returns a string of the form A;B;C where A is
 * the keybinding for the widget; B is the keybinding to traverse
 * from the menubar and C is the accelerator. The items in the
 * keybinding to traverse from the menubar are separated by ":".
 */
static const gchar *
gtk_menu_item_accessible_get_keybinding (AtkAction *action,
                                         gint       i)
{
  gchar *keybinding = NULL;
  gchar *item_keybinding = NULL;
  gchar *full_keybinding = NULL;
  gchar *accelerator = NULL;
  GtkWidget *item;
  GtkWidget *temp_item;
  GtkWidget *child;
  GtkWidget *parent;

  item = gtk_accessible_get_widget (GTK_ACCESSIBLE (action));
  if (item == NULL)
    return NULL;

  if (i != 0)
    return NULL;

  temp_item = item;
  while (TRUE)
    {
      GdkModifierType mnemonic_modifier = 0;
      guint key_val;
      gchar *key, *temp_keybinding;

      child = gtk_bin_get_child (GTK_BIN (temp_item));
      if (child == NULL)
        return NULL;

      parent = gtk_widget_get_parent (temp_item);
      if (!parent)
        /* parent can be NULL when activating a window from the panel */
        return NULL;

      if (GTK_IS_MENU_BAR (parent))
        {
          GtkWidget *toplevel;

          toplevel = gtk_widget_get_toplevel (parent);
          if (toplevel && GTK_IS_WINDOW (toplevel))
            mnemonic_modifier =
              gtk_window_get_mnemonic_modifier (GTK_WINDOW (toplevel));
        }

      if (GTK_IS_LABEL (child))
        {
          key_val = gtk_label_get_mnemonic_keyval (GTK_LABEL (child));
          if (key_val != GDK_KEY_VoidSymbol)
            {
              key = gtk_accelerator_name (key_val, mnemonic_modifier);
              if (full_keybinding)
                temp_keybinding = g_strconcat (key, ":", full_keybinding, NULL);
              else
                temp_keybinding = g_strdup (key);

              if (temp_item == item)
                item_keybinding = g_strdup (key);

              g_free (key);
              g_free (full_keybinding);
              full_keybinding = temp_keybinding;
            }
          else
            {
              /* No keybinding */
              g_free (full_keybinding);
              full_keybinding = NULL;
              break;
            }
        }

      /* We have reached the menu bar so we are finished */
      if (GTK_IS_MENU_BAR (parent))
        break;

      g_return_val_if_fail (GTK_IS_MENU (parent), NULL);
      temp_item = gtk_menu_get_attach_widget (GTK_MENU (parent));
      if (!GTK_IS_MENU_ITEM (temp_item))
        {
          /* Menu is attached to something other than a menu item;
           * probably an option menu
           */
          g_free (full_keybinding);
          full_keybinding = NULL;
          break;
        }
    }

  parent = gtk_widget_get_parent (item);
  if (GTK_IS_MENU (parent))
    {
      GtkAccelGroup *group;
      GtkAccelKey *key;

      group = gtk_menu_get_accel_group (GTK_MENU (parent));
      if (group)
        key = gtk_accel_group_find (group, find_accel_by_widget, item);
      else
        {
          key = NULL;
          child = gtk_bin_get_child (GTK_BIN (item));
          if (GTK_IS_ACCEL_LABEL (child))
            {
              GtkAccelLabel *accel_label;
              GClosure      *accel_closure;

              accel_label = GTK_ACCEL_LABEL (child);
              g_object_get (accel_label, "accel-closure", &accel_closure, NULL);
              if (accel_closure)
                {
                  key = gtk_accel_group_find (gtk_accel_group_from_accel_closure (accel_closure),
                                              find_accel_by_closure,
                                              accel_closure);
                  g_closure_unref (accel_closure);
                }
            }
        }

     if (key)
       accelerator = gtk_accelerator_name (key->accel_key, key->accel_mods);
   }

  /* Concatenate the bindings */
  if (item_keybinding || full_keybinding || accelerator)
    {
      gchar *temp;
      if (item_keybinding)
        {
          keybinding = g_strconcat (item_keybinding, KEYBINDING_SEPARATOR, NULL);
          g_free (item_keybinding);
        }
      else
        keybinding = g_strdup (KEYBINDING_SEPARATOR);

      if (full_keybinding)
        {
          temp = g_strconcat (keybinding, full_keybinding,
                              KEYBINDING_SEPARATOR, NULL);
          g_free (full_keybinding);
        }
      else
        temp = g_strconcat (keybinding, KEYBINDING_SEPARATOR, NULL);

      g_free (keybinding);
      keybinding = temp;
      if (accelerator)
        {
          temp = g_strconcat (keybinding, accelerator, NULL);
          g_free (accelerator);
          g_free (keybinding);
          keybinding = temp;
      }
    }

  return keybinding;
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
  iface->do_action = gtk_menu_item_accessible_do_action;
  iface->get_n_actions = gtk_menu_item_accessible_get_n_actions;
  iface->get_name = gtk_menu_item_accessible_action_get_name;
  iface->get_localized_name = gtk_menu_item_accessible_action_get_localized_name;
  iface->get_description = gtk_menu_item_accessible_action_get_description;
  iface->get_keybinding = gtk_menu_item_accessible_get_keybinding;
}

static void
menu_item_selection (GtkMenuItem  *item,
                     gboolean      selected)
{
  AtkObject *obj, *parent;
  gint i;

  obj = gtk_widget_get_accessible (GTK_WIDGET (item));
  GTK_MENU_ITEM_ACCESSIBLE (obj)->priv->selected = selected;
  atk_object_notify_state_change (obj, ATK_STATE_SELECTED, selected);

  for (i = 0; i < atk_object_get_n_accessible_children (obj); i++)
    {
      AtkObject *child;
      child = atk_object_ref_accessible_child (obj, i);
      atk_object_notify_state_change (child, ATK_STATE_SHOWING, selected);
      g_object_unref (child);
    }
  parent = atk_object_get_parent (obj);
  g_signal_emit_by_name (parent, "selection-changed");
}

static gboolean
gtk_menu_item_accessible_add_selection (AtkSelection *selection,
                                           gint          i)
{
  GtkMenuShell *shell;
  GList *kids;
  guint length;
  GtkWidget *widget;
  GtkWidget *menu;
  GtkWidget *child;

  widget =  gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return FALSE;

  menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));
  if (menu == NULL)
    return FALSE;

  shell = GTK_MENU_SHELL (menu);
  kids = gtk_container_get_children (GTK_CONTAINER (shell));
  length = g_list_length (kids);
  if (i < 0 || i > length)
    {
      g_list_free (kids);
      return FALSE;
    }

  child = g_list_nth_data (kids, i);
  g_list_free (kids);
  g_return_val_if_fail (GTK_IS_MENU_ITEM (child), FALSE);
  gtk_menu_shell_select_item (shell, GTK_WIDGET (child));
  return TRUE;
}

static gboolean
gtk_menu_item_accessible_clear_selection (AtkSelection *selection)
{
  GtkWidget *widget;
  GtkWidget *menu;

  widget =  gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return FALSE;

  menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));
  if (menu == NULL)
    return FALSE;

  gtk_menu_shell_deselect (GTK_MENU_SHELL (menu));

  return TRUE;
}

static AtkObject *
gtk_menu_item_accessible_ref_selection (AtkSelection *selection,
                                           gint          i)
{
  GtkMenuShell *shell;
  AtkObject *obj;
  GtkWidget *widget;
  GtkWidget *menu;
  GtkWidget *item;

  if (i != 0)
    return NULL;

  widget =  gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return NULL;

  menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));
  if (menu == NULL)
    return NULL;

  shell = GTK_MENU_SHELL (menu);

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
gtk_menu_item_accessible_get_selection_count (AtkSelection *selection)
{
  GtkMenuShell *shell;
  GtkWidget *widget;
  GtkWidget *menu;

  widget =  gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return 0;

  menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));
  if (menu == NULL)
    return 0;

  shell = GTK_MENU_SHELL (menu);

  if (gtk_menu_shell_get_selected_item (shell) != NULL)
    return 1;

  return 0;
}

static gboolean
gtk_menu_item_accessible_is_child_selected (AtkSelection *selection,
                                               gint          i)
{
  GtkMenuShell *shell;
  gint j;
  GtkWidget *widget;
  GtkWidget *menu;
  GtkWidget *item;
  GList *kids;

  widget =  gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return FALSE;

  menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));
  if (menu == NULL)
    return FALSE;

  shell = GTK_MENU_SHELL (menu);

  item = gtk_menu_shell_get_selected_item (shell);
  if (item == NULL)
    return FALSE;

  kids = gtk_container_get_children (GTK_CONTAINER (shell));
  j = g_list_index (kids, item);
  g_list_free (kids);

  return j==i;
}

static gboolean
gtk_menu_item_accessible_remove_selection (AtkSelection *selection,
                                              gint          i)
{
  GtkMenuShell *shell;
  GtkWidget *widget;
  GtkWidget *menu;
  GtkWidget *item;

  if (i != 0)
    return FALSE;

  widget =  gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return FALSE;

  menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));
  if (menu == NULL)
    return FALSE;

  shell = GTK_MENU_SHELL (menu);

  item = gtk_menu_shell_get_selected_item (shell);
  if (item && gtk_menu_item_get_submenu (GTK_MENU_ITEM (item)))
    gtk_menu_shell_deselect (shell);

  return TRUE;
}

static void
atk_selection_interface_init (AtkSelectionIface *iface)
{
  iface->add_selection = gtk_menu_item_accessible_add_selection;
  iface->clear_selection = gtk_menu_item_accessible_clear_selection;
  iface->ref_selection = gtk_menu_item_accessible_ref_selection;
  iface->get_selection_count = gtk_menu_item_accessible_get_selection_count;
  iface->is_child_selected = gtk_menu_item_accessible_is_child_selected;
  iface->remove_selection = gtk_menu_item_accessible_remove_selection;
}

static gint
menu_item_add_gtk (GtkContainer *container,
                   GtkWidget    *widget)
{
  GtkWidget *parent_widget;

  g_return_val_if_fail (GTK_IS_MENU (container), 1);

  parent_widget = gtk_menu_get_attach_widget (GTK_MENU (container));
  if (GTK_IS_MENU_ITEM (parent_widget))
    {
      GTK_CONTAINER_ACCESSIBLE_CLASS (gtk_menu_item_accessible_parent_class)->add_gtk (container, widget, gtk_widget_get_accessible (parent_widget));

    }
  return 1;
}

static gint
menu_item_remove_gtk (GtkContainer *container,
                      GtkWidget    *widget)
{
  GtkWidget *parent_widget;

  g_return_val_if_fail (GTK_IS_MENU (container), 1);

  parent_widget = gtk_menu_get_attach_widget (GTK_MENU (container));
  if (GTK_IS_MENU_ITEM (parent_widget))
    {
      GTK_CONTAINER_ACCESSIBLE_CLASS (gtk_menu_item_accessible_parent_class)->remove_gtk (container, widget, gtk_widget_get_accessible (parent_widget));
    }
  return 1;
}
static void
menu_item_select (GtkMenuItem *item)
{
  menu_item_selection (item, TRUE);
}

static void
menu_item_deselect (GtkMenuItem *item)
{
  menu_item_selection (item, FALSE);
}
