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

#include <string.h>
#include <gtk/gtk.h>
#include "gtktogglebuttonaccessible.h"


G_DEFINE_TYPE (GtkToggleButtonAccessible, gtk_toggle_button_accessible, GTK_TYPE_BUTTON_ACCESSIBLE)

static void
gtk_toggle_button_accessible_toggled (GtkWidget *widget)
{
  AtkObject *accessible;
  GtkToggleButton *toggle_button;

  toggle_button = GTK_TOGGLE_BUTTON (widget);

  accessible = gtk_widget_get_accessible (widget);
  atk_object_notify_state_change (accessible, ATK_STATE_CHECKED,
                                  gtk_toggle_button_get_active (toggle_button));
}

static void
gtk_toggle_button_accessible_initialize (AtkObject *obj,
                                         gpointer   data)
{
  ATK_OBJECT_CLASS (gtk_toggle_button_accessible_parent_class)->initialize (obj, data);

  g_signal_connect (data, "toggled",
                    G_CALLBACK (gtk_toggle_button_accessible_toggled), NULL);

  obj->role = ATK_ROLE_TOGGLE_BUTTON;
}

static AtkStateSet*
gtk_toggle_button_accessible_ref_state_set (AtkObject *accessible)
{
  AtkStateSet *state_set;
  GtkToggleButton *toggle_button;
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (widget == NULL)
    return NULL;

  state_set = ATK_OBJECT_CLASS (gtk_toggle_button_accessible_parent_class)->ref_state_set (accessible);
  toggle_button = GTK_TOGGLE_BUTTON (widget);

  if (gtk_toggle_button_get_active (toggle_button))
    atk_state_set_add_state (state_set, ATK_STATE_CHECKED);

  return state_set;
}

static void
gtk_toggle_button_accessible_class_init (GtkToggleButtonAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  class->ref_state_set = gtk_toggle_button_accessible_ref_state_set;
  class->initialize = gtk_toggle_button_accessible_initialize;
}

static void
gtk_toggle_button_accessible_init (GtkToggleButtonAccessible *button)
{
}
