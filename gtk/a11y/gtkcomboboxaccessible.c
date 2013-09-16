/* GTK+ - accessibility implementations
 * Copyright 2004 Sun Microsystems Inc.
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
#include <gtk/gtk.h>
#include "gtkcomboboxaccessible.h"

struct _GtkComboBoxAccessiblePrivate
{
  gchar         *name;
  gint           old_selection;
  gboolean       popup_set;
};

static void atk_action_interface_init    (AtkActionIface    *iface);
static void atk_selection_interface_init (AtkSelectionIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkComboBoxAccessible, gtk_combo_box_accessible, GTK_TYPE_CONTAINER_ACCESSIBLE,
                         G_ADD_PRIVATE (GtkComboBoxAccessible)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, atk_action_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_SELECTION, atk_selection_interface_init))

static void
changed_cb (GtkWidget *widget)
{
  GtkComboBox *combo_box;
  AtkObject *obj;
  GtkComboBoxAccessible *accessible;
  gint index;

  combo_box = GTK_COMBO_BOX (widget);

  index = gtk_combo_box_get_active (combo_box);
  obj = gtk_widget_get_accessible (widget);
  accessible = GTK_COMBO_BOX_ACCESSIBLE (obj);
  if (accessible->priv->old_selection != index)
    {
      accessible->priv->old_selection = index;
      g_object_notify (G_OBJECT (obj), "accessible-name");
      g_signal_emit_by_name (obj, "selection-changed");
    }
}

static void
gtk_combo_box_accessible_initialize (AtkObject *obj,
                                     gpointer   data)
{
  GtkComboBox *combo_box;
  GtkComboBoxAccessible *accessible;
  AtkObject *popup;

  ATK_OBJECT_CLASS (gtk_combo_box_accessible_parent_class)->initialize (obj, data);

  combo_box = GTK_COMBO_BOX (data);
  accessible = GTK_COMBO_BOX_ACCESSIBLE (obj);

  g_signal_connect (combo_box, "changed", G_CALLBACK (changed_cb), NULL);
  accessible->priv->old_selection = gtk_combo_box_get_active (combo_box);

  popup = gtk_combo_box_get_popup_accessible (combo_box);
  if (popup)
    {
      atk_object_set_parent (popup, obj);
      accessible->priv->popup_set = TRUE;
    }
  if (gtk_combo_box_get_has_entry (combo_box))
    atk_object_set_parent (gtk_widget_get_accessible (gtk_bin_get_child (GTK_BIN (combo_box))), obj);

  obj->role = ATK_ROLE_COMBO_BOX;
}

static void
gtk_combo_box_accessible_finalize (GObject *object)
{
  GtkComboBoxAccessible *combo_box = GTK_COMBO_BOX_ACCESSIBLE (object);

  g_free (combo_box->priv->name);

  G_OBJECT_CLASS (gtk_combo_box_accessible_parent_class)->finalize (object);
}

static const gchar *
gtk_combo_box_accessible_get_name (AtkObject *obj)
{
  GtkWidget *widget;
  GtkComboBox *combo_box;
  GtkComboBoxAccessible *accessible;
  GtkTreeIter iter;
  const gchar *name;
  GtkTreeModel *model;
  gint n_columns;
  gint i;

  name = ATK_OBJECT_CLASS (gtk_combo_box_accessible_parent_class)->get_name (obj);
  if (name)
    return name;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return NULL;

  combo_box = GTK_COMBO_BOX (widget);
  accessible = GTK_COMBO_BOX_ACCESSIBLE (obj);
  if (gtk_combo_box_get_active_iter (combo_box, &iter))
    {
      model = gtk_combo_box_get_model (combo_box);
      n_columns = gtk_tree_model_get_n_columns (model);
      for (i = 0; i < n_columns; i++)
        {
          GValue value = G_VALUE_INIT;

          gtk_tree_model_get_value (model, &iter, i, &value);
          if (G_VALUE_HOLDS_STRING (&value))
            {
              g_free (accessible->priv->name);
              accessible->priv->name =  g_strdup (g_value_get_string (&value));
              g_value_unset (&value);
              break;
            }
          else
            g_value_unset (&value);
        }
    }
  return accessible->priv->name;
}

static gint
gtk_combo_box_accessible_get_n_children (AtkObject* obj)
{
  gint n_children = 0;
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return 0;

  n_children++;
  if (gtk_combo_box_get_has_entry (GTK_COMBO_BOX (widget)))
    n_children++;

  return n_children;
}

static AtkObject *
gtk_combo_box_accessible_ref_child (AtkObject *obj,
                                    gint       i)
{
  GtkWidget *widget;
  AtkObject *child;
  GtkComboBoxAccessible *box;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return NULL;

  if (i == 0)
    {
      child = gtk_combo_box_get_popup_accessible (GTK_COMBO_BOX (widget));
      box = GTK_COMBO_BOX_ACCESSIBLE (obj);
      if (!box->priv->popup_set)
        {
          atk_object_set_parent (child, obj);
          box->priv->popup_set = TRUE;
        }
    }
  else if (i == 1 && gtk_combo_box_get_has_entry (GTK_COMBO_BOX (widget)))
    {
      child = gtk_widget_get_accessible (gtk_bin_get_child (GTK_BIN (widget)));
    }
  else
    {
      return NULL;
    }

  return g_object_ref (child);
}

static void
gtk_combo_box_accessible_class_init (GtkComboBoxAccessibleClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  gobject_class->finalize = gtk_combo_box_accessible_finalize;

  class->get_name = gtk_combo_box_accessible_get_name;
  class->get_n_children = gtk_combo_box_accessible_get_n_children;
  class->ref_child = gtk_combo_box_accessible_ref_child;
  class->initialize = gtk_combo_box_accessible_initialize;
}

static void
gtk_combo_box_accessible_init (GtkComboBoxAccessible *combo_box)
{
  combo_box->priv = gtk_combo_box_accessible_get_instance_private (combo_box);
  combo_box->priv->old_selection = -1;
  combo_box->priv->name = NULL;
  combo_box->priv->popup_set = FALSE;
}

static gboolean
gtk_combo_box_accessible_do_action (AtkAction *action,
                                    gint       i)
{
  GtkComboBox *combo_box;
  GtkWidget *widget;
  gboolean popup_shown;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (action));
  if (widget == NULL)
    return FALSE;

  if (!gtk_widget_get_sensitive (widget) || !gtk_widget_get_visible (widget))
    return FALSE;

  if (i != 0)
    return FALSE;

  combo_box = GTK_COMBO_BOX (widget);
  g_object_get (combo_box, "popup-shown", &popup_shown, NULL);
  if (popup_shown)
    gtk_combo_box_popdown (combo_box);
  else
    gtk_combo_box_popup (combo_box);

  return TRUE;
}

static gint
gtk_combo_box_accessible_get_n_actions (AtkAction *action)
{
  return 1;
}

static const gchar *
gtk_combo_box_accessible_get_keybinding (AtkAction *action,
                                         gint       i)
{
  GtkComboBoxAccessible *combo_box;
  GtkWidget *widget;
  GtkWidget *label;
  AtkRelationSet *set;
  AtkRelation *relation;
  GPtrArray *target;
  gpointer target_object;
  guint key_val;
  gchar *return_value = NULL;

  if (i != 0)
    return NULL;

  combo_box = GTK_COMBO_BOX_ACCESSIBLE (action);
  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (combo_box));
  if (widget == NULL)
    return NULL;

  set = atk_object_ref_relation_set (ATK_OBJECT (action));
  if (set == NULL)
    return NULL;

  label = NULL;
  relation = atk_relation_set_get_relation_by_type (set, ATK_RELATION_LABELLED_BY);
  if (relation)
    {
      target = atk_relation_get_target (relation);
      target_object = g_ptr_array_index (target, 0);
      label = gtk_accessible_get_widget (GTK_ACCESSIBLE (target_object));
    }
  g_object_unref (set);
  if (GTK_IS_LABEL (label))
    {
      key_val = gtk_label_get_mnemonic_keyval (GTK_LABEL (label));
      if (key_val != GDK_KEY_VoidSymbol)
        return_value = gtk_accelerator_name (key_val, GDK_MOD1_MASK);
    }

  return return_value;
}

static const gchar *
gtk_combo_box_accessible_action_get_name (AtkAction *action,
                                          gint       i)
{
  if (i == 0)
    return "press";
  return NULL;
}

static const gchar *
gtk_combo_box_accessible_action_get_localized_name (AtkAction *action,
                                                    gint       i)
{
  if (i == 0)
    return C_("Action name", "Press");
  return NULL;
}

static const gchar *
gtk_combo_box_accessible_action_get_description (AtkAction *action,
                                                 gint       i)
{
  if (i == 0)
    return C_("Action description", "Presses the combobox");
  return NULL;
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
  iface->do_action = gtk_combo_box_accessible_do_action;
  iface->get_n_actions = gtk_combo_box_accessible_get_n_actions;
  iface->get_keybinding = gtk_combo_box_accessible_get_keybinding;
  iface->get_name = gtk_combo_box_accessible_action_get_name;
  iface->get_localized_name = gtk_combo_box_accessible_action_get_localized_name;
  iface->get_description = gtk_combo_box_accessible_action_get_description;
}

static gboolean
gtk_combo_box_accessible_add_selection (AtkSelection *selection,
                                        gint          i)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return FALSE;

  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), i);

  return TRUE;
}

static gboolean
gtk_combo_box_accessible_clear_selection (AtkSelection *selection)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return FALSE;

  gtk_combo_box_set_active (GTK_COMBO_BOX (widget), -1);

  return TRUE;
}

static AtkObject *
gtk_combo_box_accessible_ref_selection (AtkSelection *selection,
                                        gint          i)
{
  GtkComboBox *combo_box;
  GtkWidget *widget;
  AtkObject *obj;
  gint index;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return NULL;

  if (i != 0)
    return NULL;

  combo_box = GTK_COMBO_BOX (widget);

  obj = gtk_combo_box_get_popup_accessible (combo_box);
  index = gtk_combo_box_get_active (combo_box);

  return atk_object_ref_accessible_child (obj, index);
}

static gint
gtk_combo_box_accessible_get_selection_count (AtkSelection *selection)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));
  if (widget == NULL)
    return 0;

  return (gtk_combo_box_get_active (GTK_COMBO_BOX (widget)) == -1) ? 0 : 1;
}

static gboolean
gtk_combo_box_accessible_is_child_selected (AtkSelection *selection,
                                            gint          i)
{
  GtkWidget *widget;
  gint j;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (selection));

  if (widget == NULL)
    return FALSE;

  j = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));

  return (j == i);
}

static gboolean
gtk_combo_box_accessible_remove_selection (AtkSelection *selection,
                                           gint          i)
{
  if (atk_selection_is_child_selected (selection, i))
    atk_selection_clear_selection (selection);

  return TRUE;
}

static void
atk_selection_interface_init (AtkSelectionIface *iface)
{
  iface->add_selection = gtk_combo_box_accessible_add_selection;
  iface->clear_selection = gtk_combo_box_accessible_clear_selection;
  iface->ref_selection = gtk_combo_box_accessible_ref_selection;
  iface->get_selection_count = gtk_combo_box_accessible_get_selection_count;
  iface->is_child_selected = gtk_combo_box_accessible_is_child_selected;
  iface->remove_selection = gtk_combo_box_accessible_remove_selection;
}
