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

#include "gtkprogressbaraccessibleprivate.h"

static void atk_value_interface_init (AtkValueIface  *iface);

G_DEFINE_TYPE_WITH_CODE (GtkProgressBarAccessible, gtk_progress_bar_accessible, GTK_TYPE_WIDGET_ACCESSIBLE,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_VALUE, atk_value_interface_init))

void
gtk_progress_bar_accessible_update_value (GtkProgressBarAccessible *self)
{
  g_object_notify (G_OBJECT (self), "accessible-value");
}

static void
gtk_progress_bar_accessible_class_init (GtkProgressBarAccessibleClass *klass)
{
}

static void
gtk_progress_bar_accessible_init (GtkProgressBarAccessible *self)
{
  ATK_OBJECT (self)->role = ATK_ROLE_PROGRESS_BAR;
}

static void
gtk_progress_bar_accessible_get_current_value (AtkValue *obj,
                                               GValue   *value)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));

  memset (value, 0, sizeof (GValue));
  g_value_init (value, G_TYPE_DOUBLE);
  g_value_set_double (value, gtk_progress_bar_get_fraction (GTK_PROGRESS_BAR (widget)));
}

static void
gtk_progress_bar_accessible_get_maximum_value (AtkValue *obj,
                                               GValue   *value)
{
  memset (value, 0, sizeof (GValue));
  g_value_init (value, G_TYPE_DOUBLE);
  g_value_set_double (value, 1.0);
}

static void
gtk_progress_bar_accessible_get_minimum_value (AtkValue *obj,
                                               GValue   *value)
{
  memset (value, 0, sizeof (GValue));
  g_value_init (value, G_TYPE_DOUBLE);
  g_value_set_double (value, 0.0);
}

static void
gtk_progress_bar_accessible_get_value_and_text (AtkValue  *obj,
                                                gdouble   *value,
                                                char     **text)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));

  *value = gtk_progress_bar_get_fraction (GTK_PROGRESS_BAR (widget));
  *text = NULL;
}

static AtkRange *
gtk_progress_bar_accessible_get_range (AtkValue *obj)
{
  return atk_range_new (0.0, 1.0, NULL);
}

static void
atk_value_interface_init (AtkValueIface *iface)
{
  iface->get_current_value = gtk_progress_bar_accessible_get_current_value;
  iface->get_maximum_value = gtk_progress_bar_accessible_get_maximum_value;
  iface->get_minimum_value = gtk_progress_bar_accessible_get_minimum_value;

  iface->get_value_and_text = gtk_progress_bar_accessible_get_value_and_text;
  iface->get_range = gtk_progress_bar_accessible_get_range;
}
