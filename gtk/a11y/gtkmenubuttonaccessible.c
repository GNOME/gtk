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
#include <glib/gi18n-lib.h>
#include "gtkmenubuttonaccessible.h"


G_DEFINE_TYPE (GtkMenuButtonAccessible, gtk_menu_button_accessible, GTK_TYPE_WIDGET_ACCESSIBLE)

static void
gtk_menu_button_accessible_initialize (AtkObject *accessible,
                                        gpointer   data)
{
  ATK_OBJECT_CLASS (gtk_menu_button_accessible_parent_class)->initialize (accessible, data);

  accessible->role = ATK_ROLE_TOGGLE_BUTTON;
}

static gint
gtk_menu_button_accessible_get_n_children (AtkObject* obj)
{
  GtkWidget *widget;
  gint count = 0;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return count;

  return count;
}

static AtkObject *
gtk_menu_button_accessible_ref_child (AtkObject *obj,
                                      gint       i)
{
  AtkObject *accessible = NULL;
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return NULL;

  return accessible;
}

static const gchar *
gtk_menu_button_accessible_get_name (AtkObject *obj)
{
  const gchar *name = NULL;
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return NULL;

  name = ATK_OBJECT_CLASS (gtk_menu_button_accessible_parent_class)->get_name (obj);
  if (name != NULL)
    return name;

  return _("Menu");
}

static AtkStateSet *
gtk_menu_button_accessible_ref_state_set (AtkObject *obj)
{
  AtkStateSet *state_set;
  GtkWidget *widget;
  GtkWidget *button;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  if (widget == NULL)
    return NULL;

  button = gtk_widget_get_first_child (widget);

  state_set = ATK_OBJECT_CLASS (gtk_menu_button_accessible_parent_class)->ref_state_set (obj);

  atk_state_set_add_state (state_set, ATK_STATE_FOCUSABLE);
  if (gtk_widget_has_focus (button))
    atk_state_set_add_state (state_set, ATK_STATE_FOCUSED);

  return state_set;
}

static void
gtk_menu_button_accessible_class_init (GtkMenuButtonAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  class->get_name = gtk_menu_button_accessible_get_name;
  class->initialize = gtk_menu_button_accessible_initialize;
  class->get_n_children = gtk_menu_button_accessible_get_n_children;
  class->ref_child = gtk_menu_button_accessible_ref_child;
  class->ref_state_set = gtk_menu_button_accessible_ref_state_set;
}

static void
gtk_menu_button_accessible_init (GtkMenuButtonAccessible *menu_button)
{
}
