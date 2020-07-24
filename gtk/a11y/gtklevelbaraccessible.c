/* GAIL - The GNOME Accessibility Implementation Library
 * Copyright 2001, 2002, 2003 Sun Microsystems Inc.
 * Copyright 2013 SUSE LLC.
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

#include "gtklevelbaraccessible.h"

#include "gtklevelbar.h"

#include <string.h>

static void atk_value_interface_init (AtkValueIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkLevelBarAccessible, gtk_level_bar_accessible, GTK_TYPE_WIDGET_ACCESSIBLE,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_VALUE, atk_value_interface_init))

static void
on_value_changed (GObject    *gobject,
                  GParamSpec *pspec,
                  gpointer    user_data)
{
  GtkLevelBarAccessible *self = user_data;

  g_object_notify (G_OBJECT (self), "accessible-value");
}

static void
gtk_level_bar_accessible_initialize (AtkObject *object,
                                     gpointer   data)
{
  GtkLevelBar *level_bar = data;

  g_signal_connect (level_bar, "notify::value", G_CALLBACK (on_value_changed), object);
}

static void
gtk_level_bar_accessible_class_init (GtkLevelBarAccessibleClass *klass)
{
  AtkObjectClass *object_class = ATK_OBJECT_CLASS (klass);

  object_class->initialize = gtk_level_bar_accessible_initialize;
}

static void
gtk_level_bar_accessible_init (GtkLevelBarAccessible *self)
{
  ATK_OBJECT (self)->role = ATK_ROLE_LEVEL_BAR;
}

static void
gtk_level_bar_accessible_get_current_value (AtkValue *obj,
                                            GValue   *value)
{
  GtkWidget *widget;
  GtkLevelBar *level_bar;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  level_bar = GTK_LEVEL_BAR (widget);

  memset (value,  0, sizeof (GValue));
  g_value_init (value, G_TYPE_DOUBLE);
  g_value_set_double (value, gtk_level_bar_get_value (level_bar));
}

static void
gtk_level_bar_accessible_get_maximum_value (AtkValue *obj,
                                            GValue   *value)
{
  GtkWidget *widget;
  GtkLevelBar *level_bar;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  level_bar = GTK_LEVEL_BAR (widget);

  memset (value,  0, sizeof (GValue));
  g_value_init (value, G_TYPE_DOUBLE);
  g_value_set_double (value, gtk_level_bar_get_max_value (level_bar));
}

static void
gtk_level_bar_accessible_get_minimum_value (AtkValue *obj,
                                            GValue   *value)
{
  GtkWidget *widget;
  GtkLevelBar *level_bar;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  level_bar = GTK_LEVEL_BAR (widget);

  memset (value,  0, sizeof (GValue));
  g_value_init (value, G_TYPE_DOUBLE);
  g_value_set_double (value, gtk_level_bar_get_min_value (level_bar));
}

static gboolean
gtk_level_bar_accessible_set_current_value (AtkValue     *obj,
                                            const GValue *value)
{
  GtkWidget *widget;
  GtkLevelBar *level_bar;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  level_bar = GTK_LEVEL_BAR (widget);

  gtk_level_bar_set_value (level_bar, g_value_get_double (value));

  return TRUE;
}

static void
gtk_level_bar_accessible_get_value_and_text (AtkValue  *obj,
                                             gdouble   *value,
                                             char     **text)
{
  GtkWidget *widget;
  GtkLevelBar *level_bar;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  level_bar = GTK_LEVEL_BAR (widget);

  *value = gtk_level_bar_get_value (level_bar);
  *text = NULL;
}

static AtkRange *
gtk_level_bar_accessible_get_range (AtkValue *obj)
{
  GtkWidget *widget;
  GtkLevelBar *level_bar;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  level_bar = GTK_LEVEL_BAR (widget);

  return atk_range_new (gtk_level_bar_get_min_value (level_bar),
                        gtk_level_bar_get_max_value (level_bar),
                        NULL);
}

static void
gtk_level_bar_accessible_set_value (AtkValue      *obj,
                                    const gdouble  value)
{
  GtkWidget *widget;
  GtkLevelBar *level_bar;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  level_bar = GTK_LEVEL_BAR (widget);

  gtk_level_bar_set_value (level_bar, value);
}

static void
atk_value_interface_init (AtkValueIface *iface)
{
  iface->get_current_value = gtk_level_bar_accessible_get_current_value;
  iface->get_maximum_value = gtk_level_bar_accessible_get_maximum_value;
  iface->get_minimum_value = gtk_level_bar_accessible_get_minimum_value;
  iface->set_current_value = gtk_level_bar_accessible_set_current_value;

  iface->get_value_and_text = gtk_level_bar_accessible_get_value_and_text;
  iface->get_range = gtk_level_bar_accessible_get_range;
  iface->set_value = gtk_level_bar_accessible_set_value;
}
