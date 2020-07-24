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

#include "gtkexpanderaccessibleprivate.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

static void atk_action_interface_init (AtkActionIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkExpanderAccessible, gtk_expander_accessible, GTK_TYPE_WIDGET_ACCESSIBLE,
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

static int
gtk_expander_accessible_get_n_children (AtkObject *obj)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return 0;

  if (gtk_expander_get_child (GTK_EXPANDER (widget)))
    return 1;

  return 0;
}

static AtkObject *
gtk_expander_accessible_ref_child (AtkObject *obj,
                                   int        i)
{
  AtkObject *accessible;
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return NULL;

  widget = gtk_expander_get_child (GTK_EXPANDER (widget));
  if (widget == NULL)
    return NULL;

  accessible = gtk_widget_get_accessible (widget);
  g_object_ref (accessible);
  return accessible;
}

void
gtk_expander_accessible_update_label (GtkExpanderAccessible *self)
{
  AtkObject *atk_obj = ATK_OBJECT (self);

  if (atk_obj->name == NULL)
    g_object_notify (G_OBJECT (atk_obj), "accessible-name");

  g_signal_emit_by_name (atk_obj, "visible-data-changed");
}

void
gtk_expander_accessible_update_state (GtkExpanderAccessible *self,
                                      gboolean               expanded)
{
  AtkObject *atk_obj = ATK_OBJECT (self);

  atk_object_notify_state_change (atk_obj, ATK_STATE_CHECKED, expanded);
  atk_object_notify_state_change (atk_obj, ATK_STATE_EXPANDED, expanded);

  g_signal_emit_by_name (atk_obj, "visible-data-changed");
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

  class->get_name = gtk_expander_accessible_get_name;
  class->get_n_children = gtk_expander_accessible_get_n_children;
  class->ref_child = gtk_expander_accessible_ref_child;
  class->ref_state_set = gtk_expander_accessible_ref_state_set;
}

static void
gtk_expander_accessible_init (GtkExpanderAccessible *self)
{
  ATK_OBJECT (self)->role = ATK_ROLE_TOGGLE_BUTTON;
}

static gboolean
gtk_expander_accessible_do_action (AtkAction *action,
                                   int        i)
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

static int
gtk_expander_accessible_get_n_actions (AtkAction *action)
{
  return 1;
}

static const gchar *
gtk_expander_accessible_get_keybinding (AtkAction *action,
                                        int        i)
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
        return_value = gtk_accelerator_name (key_val, GDK_ALT_MASK);
    }

  return return_value;
}

static const gchar *
gtk_expander_accessible_action_get_name (AtkAction *action,
                                         int        i)
{
  if (i == 0)
    return "activate";
  return NULL;
}

static const gchar *
gtk_expander_accessible_action_get_localized_name (AtkAction *action,
                                                   int        i)
{
  if (i == 0)
    return C_("Action name", "Activate");
  return NULL;
}

static const gchar *
gtk_expander_accessible_action_get_description (AtkAction *action,
                                                int        i)
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
