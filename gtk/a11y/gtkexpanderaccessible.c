/* GTK+ - accessibility implementations
 * Copyright 2003 Sun Microsystems Inc.
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
#include "gtkexpanderaccessible.h"

static void atk_action_interface_init (AtkActionIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkExpanderAccessible, gtk_expander_accessible, GTK_TYPE_CONTAINER_ACCESSIBLE,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, atk_action_interface_init))

static const gchar *
gtk_expander_accessible_get_full_text (GtkExpander *widget)
{
  GtkWidget *label_widget;

  label_widget = gtk_expander_get_label_widget (widget);

  if (!GTK_IS_LABEL (label_widget))
    return NULL;

  return gtk_label_get_text (GTK_LABEL (label_widget));
}

static const gchar *
gtk_expander_accessible_get_name (AtkObject *obj)
{
  GtkWidget *widget;
  const gchar *name;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return NULL;

  name = ATK_OBJECT_CLASS (gtk_expander_accessible_parent_class)->get_name (obj);
  if (name != NULL)
    return name;

  return gtk_expander_accessible_get_full_text (GTK_EXPANDER (widget));
}

static gint
gtk_expander_accessible_get_n_children (AtkObject *obj)
{
  GtkWidget *widget;
  GList *children;
  gint count = 0;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return 0;

  children = gtk_container_get_children (GTK_CONTAINER(widget));
  count = g_list_length (children);
  g_list_free (children);

  /* See if there is a label - if there is, reduce our count by 1
   * since we don't want the label included with the children.
   */
  if (gtk_expander_get_label_widget (GTK_EXPANDER (widget)))
    count -= 1;

  return count;
}

static AtkObject *
gtk_expander_accessible_ref_child (AtkObject *obj,
                                   gint       i)
{
  GList *children, *tmp_list;
  AtkObject *accessible;
  GtkWidget *widget;
  GtkWidget *label;
  gint index;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return NULL;

  children = gtk_container_get_children (GTK_CONTAINER (widget));

  /* See if there is a label - if there is, we need to skip it
   * since we don't want the label included with the children.
   */
  label = gtk_expander_get_label_widget (GTK_EXPANDER (widget));
  if (label)
    {
      for (index = 0; index <= i; index++)
        {
          tmp_list = g_list_nth (children, index);
          if (label == GTK_WIDGET (tmp_list->data))
            {
              i += 1;
              break;
            }
        }
    }

  tmp_list = g_list_nth (children, i);
  if (!tmp_list)
    {
      g_list_free (children);
      return NULL;
    }
  accessible = gtk_widget_get_accessible (GTK_WIDGET (tmp_list->data));

  g_list_free (children);
  g_object_ref (accessible);
  return accessible;
}

static void
gtk_expander_accessible_initialize (AtkObject *obj,
                                    gpointer   data)
{
  ATK_OBJECT_CLASS (gtk_expander_accessible_parent_class)->initialize (obj, data);

  obj->role = ATK_ROLE_TOGGLE_BUTTON;
}

static void
gtk_expander_accessible_notify_gtk (GObject    *obj,
                                    GParamSpec *pspec)
{
  AtkObject* atk_obj;
  GtkExpander *expander;

  expander = GTK_EXPANDER (obj);
  atk_obj = gtk_widget_get_accessible (GTK_WIDGET (expander));
;
  if (g_strcmp0 (pspec->name, "label") == 0)
    {
      if (atk_obj->name == NULL)
        g_object_notify (G_OBJECT (atk_obj), "accessible-name");
      g_signal_emit_by_name (atk_obj, "visible-data-changed");
    }
  else if (g_strcmp0 (pspec->name, "expanded") == 0)
    {
      atk_object_notify_state_change (atk_obj, ATK_STATE_CHECKED,
                                      gtk_expander_get_expanded (expander));
      atk_object_notify_state_change (atk_obj, ATK_STATE_EXPANDED,
                                      gtk_expander_get_expanded (expander));
      g_signal_emit_by_name (atk_obj, "visible-data-changed");
    }
  else
    GTK_WIDGET_ACCESSIBLE_CLASS (gtk_expander_accessible_parent_class)->notify_gtk (obj, pspec);
}

static AtkStateSet *
gtk_expander_accessible_ref_state_set (AtkObject *obj)
{
  AtkStateSet *state_set;
  GtkWidget *widget;
  GtkExpander *expander;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return NULL;

  state_set = ATK_OBJECT_CLASS (gtk_expander_accessible_parent_class)->ref_state_set (obj);

  expander = GTK_EXPANDER (widget);

  atk_state_set_add_state (state_set, ATK_STATE_EXPANDABLE);

  if (gtk_expander_get_expanded (expander))
    {
      atk_state_set_add_state (state_set, ATK_STATE_CHECKED);
      atk_state_set_add_state (state_set, ATK_STATE_EXPANDED);
    }

  return state_set;
}

static void
gtk_expander_accessible_class_init (GtkExpanderAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GtkWidgetAccessibleClass *widget_class = (GtkWidgetAccessibleClass*)klass;

  widget_class->notify_gtk = gtk_expander_accessible_notify_gtk;

  class->get_name = gtk_expander_accessible_get_name;
  class->get_n_children = gtk_expander_accessible_get_n_children;
  class->ref_child = gtk_expander_accessible_ref_child;
  class->ref_state_set = gtk_expander_accessible_ref_state_set;

  class->initialize = gtk_expander_accessible_initialize;
}

static void
gtk_expander_accessible_init (GtkExpanderAccessible *expander)
{
}

static gboolean
gtk_expander_accessible_do_action (AtkAction *action,
                                   gint       i)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (action));
  if (widget == NULL)
    return FALSE;

  if (!gtk_widget_is_sensitive (widget) || !gtk_widget_get_visible (widget))
    return FALSE;

  if (i != 0)
    return FALSE;

  gtk_widget_activate (widget);
  return TRUE;
}

static gint
gtk_expander_accessible_get_n_actions (AtkAction *action)
{
  return 1;
}

static const gchar *
gtk_expander_accessible_get_keybinding (AtkAction *action,
                                        gint       i)
{
  gchar *return_value = NULL;
  GtkWidget *widget;
  GtkWidget *label;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (action));
  if (widget == NULL)
    return NULL;

  if (i != 0)
    return NULL;

  label = gtk_expander_get_label_widget (GTK_EXPANDER (widget));
  if (GTK_IS_LABEL (label))
    {
      guint key_val;

      key_val = gtk_label_get_mnemonic_keyval (GTK_LABEL (label));
      if (key_val != GDK_KEY_VoidSymbol)
        return_value = gtk_accelerator_name (key_val, GDK_MOD1_MASK);
    }

  return return_value;
}

static const gchar *
gtk_expander_accessible_action_get_name (AtkAction *action,
                                         gint       i)
{
  if (i == 0)
    return "activate";
  return NULL;
}

static const gchar *
gtk_expander_accessible_action_get_localized_name (AtkAction *action,
                                                   gint       i)
{
  if (i == 0)
    return C_("Action name", "Activate");
  return NULL;
}

static const gchar *
gtk_expander_accessible_action_get_description (AtkAction *action,
                                                gint       i)
{
  if (i == 0)
    return C_("Action description", "Activates the expander");
  return NULL;
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
  iface->do_action = gtk_expander_accessible_do_action;
  iface->get_n_actions = gtk_expander_accessible_get_n_actions;
  iface->get_keybinding = gtk_expander_accessible_get_keybinding;
  iface->get_name = gtk_expander_accessible_action_get_name;
  iface->get_localized_name = gtk_expander_accessible_action_get_localized_name;
  iface->get_description = gtk_expander_accessible_action_get_description;
}
