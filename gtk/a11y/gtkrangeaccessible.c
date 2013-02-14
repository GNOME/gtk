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
#include "gtkrangeaccessible.h"


static void atk_value_interface_init  (AtkValueIface  *iface);

G_DEFINE_TYPE_WITH_CODE (GtkRangeAccessible, gtk_range_accessible, GTK_TYPE_WIDGET_ACCESSIBLE,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_VALUE, atk_value_interface_init))

static void
gtk_range_accessible_value_changed (GtkAdjustment *adjustment,
                                    gpointer       data)
{
  g_object_notify (G_OBJECT (data), "accessible-value");
}

static void
gtk_range_accessible_initialize (AtkObject *obj,
                                 gpointer   data)
{
  GtkRangeAccessible *range = GTK_RANGE_ACCESSIBLE (obj);
  GtkAdjustment *adj;
  GtkRange *gtk_range;

  ATK_OBJECT_CLASS (gtk_range_accessible_parent_class)->initialize (obj, data);

  gtk_range = GTK_RANGE (data);
  /*
   * If a GtkAdjustment already exists for the GtkRange,
   * create the GailAdjustment
   */
  adj = gtk_range_get_adjustment (gtk_range);
  if (adj)
    g_signal_connect (adj, "value-changed",
                      G_CALLBACK (gtk_range_accessible_value_changed),
                      range);

  obj->role = ATK_ROLE_SLIDER;
}

static void
gtk_range_accessible_finalize (GObject *object)
{
  GtkRangeAccessible *range = GTK_RANGE_ACCESSIBLE (object);
  GtkWidget *widget;
  GtkAdjustment *adj;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (object));
  if (widget)
    {
      adj = gtk_range_get_adjustment (GTK_RANGE (widget));
      if (adj)
        g_signal_handlers_disconnect_by_func (adj,
                                              gtk_range_accessible_value_changed,
                                              range);
    }

  G_OBJECT_CLASS (gtk_range_accessible_parent_class)->finalize (object);
}

static void
gtk_range_accessible_notify_gtk (GObject    *obj,
                                 GParamSpec *pspec)
{
  GtkWidget *widget = GTK_WIDGET (obj);
  GtkAdjustment *adj;
  AtkObject *range;

  if (strcmp (pspec->name, "adjustment") == 0)
    {
      range = gtk_widget_get_accessible (widget);
      adj = gtk_range_get_adjustment (GTK_RANGE (widget));
      g_signal_connect (adj, "value-changed",
                        G_CALLBACK (gtk_range_accessible_value_changed),
                        range);
    }
  else
    GTK_WIDGET_ACCESSIBLE_CLASS (gtk_range_accessible_parent_class)->notify_gtk (obj, pspec);
}


static void
gtk_range_accessible_class_init (GtkRangeAccessibleClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GtkWidgetAccessibleClass *widget_class = (GtkWidgetAccessibleClass*)klass;

  widget_class->notify_gtk = gtk_range_accessible_notify_gtk;

  class->initialize = gtk_range_accessible_initialize;

  gobject_class->finalize = gtk_range_accessible_finalize;
}

static void
gtk_range_accessible_init (GtkRangeAccessible *range)
{
}

static void
gtk_range_accessible_get_current_value (AtkValue *obj,
                                        GValue   *value)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_range_get_adjustment (GTK_RANGE (widget));
  if (adjustment == NULL)
    return;

  memset (value,  0, sizeof (GValue));
  g_value_init (value, G_TYPE_DOUBLE);
  g_value_set_double (value, gtk_adjustment_get_value (adjustment));
}

static void
gtk_range_accessible_get_maximum_value (AtkValue *obj,
                                        GValue   *value)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;
  gdouble max;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_range_get_adjustment (GTK_RANGE (widget));
  if (adjustment == NULL)
    return;

  max = gtk_adjustment_get_upper (adjustment)
        - gtk_adjustment_get_page_size (adjustment);

  if (gtk_range_get_restrict_to_fill_level (GTK_RANGE (widget)))
    max = MIN (max, gtk_range_get_fill_level (GTK_RANGE (widget)));

  memset (value,  0, sizeof (GValue));
  g_value_init (value, G_TYPE_DOUBLE);
  g_value_set_double (value, max);
}

static void
gtk_range_accessible_get_minimum_value (AtkValue *obj,
                                        GValue   *value)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_range_get_adjustment (GTK_RANGE (widget));
  if (adjustment == NULL)
    return;

  memset (value,  0, sizeof (GValue));
  g_value_init (value, G_TYPE_DOUBLE);
  g_value_set_double (value, gtk_adjustment_get_lower (adjustment));
}

static void
gtk_range_accessible_get_minimum_increment (AtkValue *obj,
                                            GValue   *value)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_range_get_adjustment (GTK_RANGE (widget));
  if (adjustment == NULL)
    return;

  memset (value,  0, sizeof (GValue));
  g_value_init (value, G_TYPE_DOUBLE);
  g_value_set_double (value, gtk_adjustment_get_minimum_increment (adjustment));
}

static gboolean
gtk_range_accessible_set_current_value (AtkValue     *obj,
                                        const GValue *value)
{
 GtkWidget *widget;
  GtkAdjustment *adjustment;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_range_get_adjustment (GTK_RANGE (widget));
  if (adjustment == NULL)
    return FALSE;

  gtk_adjustment_set_value (adjustment, g_value_get_double (value));

  return TRUE;
}

static void
atk_value_interface_init (AtkValueIface *iface)
{
  iface->get_current_value = gtk_range_accessible_get_current_value;
  iface->get_maximum_value = gtk_range_accessible_get_maximum_value;
  iface->get_minimum_value = gtk_range_accessible_get_minimum_value;
  iface->get_minimum_increment = gtk_range_accessible_get_minimum_increment;
  iface->set_current_value = gtk_range_accessible_set_current_value;
}
