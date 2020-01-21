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
#include "gtkpanedaccessible.h"

static void atk_value_interface_init (AtkValueIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkPanedAccessible, gtk_paned_accessible, GTK_TYPE_CONTAINER_ACCESSIBLE,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_VALUE, atk_value_interface_init))

static void
gtk_paned_accessible_size_allocate_gtk (GtkWidget     *widget,
                                        GtkAllocation *allocation)
{
  AtkObject *obj = gtk_widget_get_accessible (widget);

  g_object_notify (G_OBJECT (obj), "accessible-value");
}

static void
gtk_paned_accessible_initialize (AtkObject *obj,
                                 gpointer   data)
{
  ATK_OBJECT_CLASS (gtk_paned_accessible_parent_class)->initialize (obj, data);

  g_signal_connect (data, "size-allocate",
                    G_CALLBACK (gtk_paned_accessible_size_allocate_gtk), NULL);

  obj->role = ATK_ROLE_SPLIT_PANE;
}

static void
gtk_paned_accessible_class_init (GtkPanedAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  class->initialize = gtk_paned_accessible_initialize;
}

static void
gtk_paned_accessible_init (GtkPanedAccessible *paned)
{
}

static void
gtk_paned_accessible_get_value_and_text (AtkValue  *obj,
                                         gdouble   *value,
                                         gchar    **text)
{
  GtkWidget *widget;
  GtkPaned *paned;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  paned = GTK_PANED (widget);

  *value = gtk_paned_get_position (paned);
  *text = NULL;
}

static AtkRange *
gtk_paned_accessible_get_range (AtkValue *obj)
{
  GtkWidget *widget;
  gint minimum_value;
  gint maximum_value;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));

  g_object_get (widget,
                "min-position", &minimum_value,
                "max-position", &maximum_value,
                NULL);

  return atk_range_new (minimum_value, maximum_value, NULL);
}

static void
gtk_paned_accessible_set_value (AtkValue      *obj,
                                const gdouble  value)
{
  GtkWidget *widget;
  GtkPaned *paned;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  paned = GTK_PANED (widget);

  gtk_paned_set_position (paned, (gint)(value + 0.5));
}

static void
atk_value_interface_init (AtkValueIface *iface)
{
  iface->get_value_and_text = gtk_paned_accessible_get_value_and_text;
  iface->get_range = gtk_paned_accessible_get_range;
  iface->set_value = gtk_paned_accessible_set_value;
}
