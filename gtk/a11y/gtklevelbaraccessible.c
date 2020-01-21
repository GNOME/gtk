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

#include <string.h>
#include <gtk/gtk.h>
#include "gtklevelbaraccessible.h"


static void atk_value_interface_init (AtkValueIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkLevelBarAccessible, gtk_level_bar_accessible, GTK_TYPE_WIDGET_ACCESSIBLE,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_VALUE, atk_value_interface_init))

static void
gtk_level_bar_accessible_initialize (AtkObject *obj,
                                       gpointer  data)
{
  ATK_OBJECT_CLASS (gtk_level_bar_accessible_parent_class)->initialize (obj, data);

  obj->role = ATK_ROLE_LEVEL_BAR;
}

static void
gtk_level_bar_accessible_notify_gtk (GObject    *obj,
                                       GParamSpec *pspec)
{
  GtkWidget *widget = GTK_WIDGET (obj);
  GtkLevelBarAccessible *level_bar = GTK_LEVEL_BAR_ACCESSIBLE (gtk_widget_get_accessible (widget));

  if (strcmp (pspec->name, "value") == 0)
    {
      g_object_notify (G_OBJECT (level_bar), "accessible-value");
    }
  else
    GTK_WIDGET_ACCESSIBLE_CLASS (gtk_level_bar_accessible_parent_class)->notify_gtk (obj, pspec);
}



static void
gtk_level_bar_accessible_class_init (GtkLevelBarAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GtkWidgetAccessibleClass *widget_class = (GtkWidgetAccessibleClass*)klass;

  widget_class->notify_gtk = gtk_level_bar_accessible_notify_gtk;

  class->initialize = gtk_level_bar_accessible_initialize;
}

static void
gtk_level_bar_accessible_init (GtkLevelBarAccessible *button)
{
}

static void
gtk_level_bar_accessible_get_value_and_text (AtkValue  *obj,
                                             gdouble   *value,
                                             gchar    **text)
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
  iface->get_value_and_text = gtk_level_bar_accessible_get_value_and_text;
  iface->get_range = gtk_level_bar_accessible_get_range;
  iface->set_value = gtk_level_bar_accessible_set_value;
}
