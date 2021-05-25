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
#include <glib/gi18n-lib.h>
#include "gtkfilechooserwidgetaccessible.h"


static void atk_action_interface_init (AtkActionIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkFileChooserWidgetAccessible, gtk_file_chooser_widget_accessible, GTK_TYPE_CONTAINER_ACCESSIBLE,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, atk_action_interface_init))

static void
gtk_file_chooser_widget_accessible_initialize (AtkObject *obj,
                                  gpointer   data)
{
  ATK_OBJECT_CLASS (gtk_file_chooser_widget_accessible_parent_class)->initialize (obj, data);
  obj->role = ATK_ROLE_FILE_CHOOSER;
}

static void
gtk_file_chooser_widget_accessible_class_init (GtkFileChooserWidgetAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GtkContainerAccessibleClass *container_class = (GtkContainerAccessibleClass*)klass;

  class->initialize = gtk_file_chooser_widget_accessible_initialize;

  container_class->add_gtk = NULL;
  container_class->remove_gtk = NULL;
}

static void
gtk_file_chooser_widget_accessible_init (GtkFileChooserWidgetAccessible *file_chooser_widget)
{
}

static gboolean
gtk_file_chooser_widget_accessible_do_action (AtkAction *action,
                                   gint       i)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (action));
  if (widget == NULL)
    return FALSE;

  if (!gtk_widget_is_sensitive (widget) || !gtk_widget_get_visible (widget))
    return FALSE;

  switch (i)
    {
    case 0:
      g_signal_emit_by_name (GTK_FILE_CHOOSER_WIDGET (widget), "location-popup", "");
      return TRUE;
    default:
      break;
    }

  return FALSE;
}

static gint
gtk_file_chooser_widget_accessible_get_n_actions (AtkAction *action)
{
  return 1;
}

static const gchar *
gtk_file_chooser_widget_accessible_action_get_name (AtkAction *action,
                                       gint       i)
{
  if (i == 0)
    return "show_location";
  return NULL;
}

static const gchar *
gtk_file_chooser_widget_accessible_action_get_localized_name (AtkAction *action,
                                                 gint       i)
{
  if (i == 0)
    return C_("Action name", "Show location");
  return NULL;
}

static const gchar *
gtk_file_chooser_widget_accessible_action_get_description (AtkAction *action,
                                              gint       i)
{
  if (i == 0)
    return C_("Action description", "Show the File Chooser's Location text field");
  return NULL;
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
  iface->do_action = gtk_file_chooser_widget_accessible_do_action;
  iface->get_n_actions = gtk_file_chooser_widget_accessible_get_n_actions;
  iface->get_name = gtk_file_chooser_widget_accessible_action_get_name;
  iface->get_localized_name = gtk_file_chooser_widget_accessible_action_get_localized_name;
  iface->get_description = gtk_file_chooser_widget_accessible_action_get_description;
}
