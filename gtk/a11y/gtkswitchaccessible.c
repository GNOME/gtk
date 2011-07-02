/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2010  Intel Corporation
 * Copyright (C) 2010  RedHat, Inc.
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
 * Boston, MA  02111-1307, USA.
 *
 * Author:
 *      Emmanuele Bassi <ebassi@linux.intel.com>
 *      Matthias Clasen <mclasen@redhat.com>
 *
 * Based on similar code from Mx.
 */

#include "config.h"

#include <gtk/gtk.h>
#include "gtkintl.h"
#include "gtkswitchaccessible.h"


static void atk_action_interface_init (AtkActionIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkSwitchAccessible, gtk_switch_accessible, GTK_TYPE_WIDGET_ACCESSIBLE,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, atk_action_interface_init))

static AtkStateSet *
gtk_switch_accessible_ref_state_set (AtkObject *accessible)
{
  AtkStateSet *state_set;
  GtkWidget *widget;

  state_set = ATK_OBJECT_CLASS (gtk_switch_accessible_parent_class)->ref_state_set (accessible);

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (widget == NULL)
    return state_set;

  if (gtk_switch_get_active (GTK_SWITCH (widget)))
    atk_state_set_add_state (state_set, ATK_STATE_CHECKED);

  return state_set;
}

static void
gtk_switch_accessible_finalize (GObject *obj)
{
  GtkSwitchAccessible *accessible = (GtkSwitchAccessible *)obj;

  g_free (accessible->action_description);

  if (accessible->action_idle)
    g_source_remove (accessible->action_idle);

  G_OBJECT_CLASS (gtk_switch_accessible_parent_class)->finalize (obj);
}

static void
gtk_switch_accessible_initialize (AtkObject *accessible,
                                  gpointer   widget)
{
  ATK_OBJECT_CLASS (gtk_switch_accessible_parent_class)->initialize (accessible, widget);

  atk_object_set_role (accessible, ATK_ROLE_TOGGLE_BUTTON);
  atk_object_set_name (accessible, C_("light switch widget", "Switch"));
  atk_object_set_description (accessible, _("Switches between on and off states"));
}

static void
gtk_switch_accessible_class_init (GtkSwitchAccessibleClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  AtkObjectClass *atk_class = ATK_OBJECT_CLASS (klass);

  object_class->finalize = gtk_switch_accessible_finalize;

  atk_class->initialize = gtk_switch_accessible_initialize;
  atk_class->ref_state_set = gtk_switch_accessible_ref_state_set;
}

static void
gtk_switch_accessible_init (GtkSwitchAccessible *self)
{
  self->action_description = NULL;
  self->action_idle = 0;
}

static gint
gtk_switch_action_get_n_actions (AtkAction *action)
{
  return 1;
}

static const gchar *
gtk_switch_action_get_name (AtkAction *action,
                            gint       i)
{
  if (i != 0)
    return NULL;

  return "toggle";
}

static const gchar *
gtk_switch_action_get_description (AtkAction *action,
                                   gint       i)
{
  GtkSwitchAccessible *accessible = (GtkSwitchAccessible *)action;

  if (i != 0)
    return NULL;

  return accessible->action_description;
}

static gboolean
gtk_switch_action_set_description (AtkAction   *action,
                                   gint         i,
                                   const gchar *description)
{
  GtkSwitchAccessible *accessible = (GtkSwitchAccessible*)action;

  if (i != 0)
    return FALSE;

  g_free (accessible->action_description);
  accessible->action_description = g_strdup (description);

  return TRUE;
}

static gboolean
idle_do_action (gpointer data)
{
  GtkSwitchAccessible *accessible = data;
  GtkWidget *widget;
  GtkSwitch *sw;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (data));
  sw = GTK_SWITCH (widget);

  accessible->action_idle = 0;

  if (widget == NULL ||
      !gtk_widget_is_sensitive (widget) || !gtk_widget_get_visible (widget))
    return FALSE;

  gtk_switch_set_active (sw, !gtk_switch_get_active (sw));

  return FALSE;
}

static gboolean
gtk_switch_action_do_action (AtkAction *action,
                             gint       i)
{
  GtkSwitchAccessible *accessible;
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (action));
  if (widget == NULL)
    return FALSE;

  if (i != 0)
    return FALSE;

  if (!gtk_widget_is_sensitive (widget) || !gtk_widget_get_visible (widget))
    return FALSE;

  accessible = (GtkSwitchAccessible *)action;

  if (!accessible->action_idle)
    accessible->action_idle = gdk_threads_add_idle (idle_do_action, accessible);

  return TRUE;
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
  iface->do_action = gtk_switch_action_do_action;
  iface->get_n_actions = gtk_switch_action_get_n_actions;
  iface->get_name = gtk_switch_action_get_name;
  iface->get_description = gtk_switch_action_get_description;
  iface->set_description = gtk_switch_action_set_description;
}
