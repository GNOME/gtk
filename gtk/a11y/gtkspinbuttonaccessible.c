/* GAIL - The GNOME Accessibility Implementation Library
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
#include "gtkspinbuttonaccessible.h"


static void atk_value_interface_init (AtkValueIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkSpinButtonAccessible, _gtk_spin_button_accessible, GTK_TYPE_ENTRY_ACCESSIBLE,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_VALUE, atk_value_interface_init))

static void
gtk_spin_button_accessible_value_changed (GtkAdjustment *adjustment,
                                          gpointer       data)
{
  GtkSpinButtonAccessible *spin_button;

  if (adjustment == NULL || data == NULL)
    return;

  spin_button = GTK_SPIN_BUTTON_ACCESSIBLE (data);

  g_object_notify (G_OBJECT (spin_button), "accessible-value");
}

static void
gtk_spin_button_accessible_initialize (AtkObject *obj,
                                       gpointer  data)
{
  GtkAdjustment *adjustment;

  ATK_OBJECT_CLASS (_gtk_spin_button_accessible_parent_class)->initialize (obj, data);

  adjustment = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (data));
  if (adjustment)
    g_signal_connect (adjustment,
                      "value-changed",
                      G_CALLBACK (gtk_spin_button_accessible_value_changed),
                      obj);

  obj->role = ATK_ROLE_SPIN_BUTTON;
}

static void
gtk_spin_button_accessible_notify_gtk (GObject    *obj,
                                       GParamSpec *pspec)
{
  GtkWidget *widget = GTK_WIDGET (obj);
  GtkSpinButtonAccessible *spin_button = GTK_SPIN_BUTTON_ACCESSIBLE (gtk_widget_get_accessible (widget));

  if (strcmp (pspec->name, "adjustment") == 0)
    {
      GtkAdjustment* adjustment;

      adjustment = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (widget));
      g_signal_connect (adjustment, "value-changed",
                        G_CALLBACK (gtk_spin_button_accessible_value_changed),
                        spin_button);
    }
  else
    GTK_WIDGET_ACCESSIBLE_CLASS (_gtk_spin_button_accessible_parent_class)->notify_gtk (obj, pspec);
}



static void
_gtk_spin_button_accessible_class_init (GtkSpinButtonAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GtkWidgetAccessibleClass *widget_class = (GtkWidgetAccessibleClass*)klass;

  widget_class->notify_gtk = gtk_spin_button_accessible_notify_gtk;

  class->initialize = gtk_spin_button_accessible_initialize;
}

static void
_gtk_spin_button_accessible_init (GtkSpinButtonAccessible *button)
{
}

static void
gtk_spin_button_accessible_get_current_value (AtkValue *obj,
                                              GValue   *value)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (widget));
  if (adjustment == NULL)
    return;

  memset (value,  0, sizeof (GValue));
  g_value_init (value, G_TYPE_DOUBLE);
  g_value_set_double (value, gtk_adjustment_get_value (adjustment));
}

static void
gtk_spin_button_accessible_get_maximum_value (AtkValue *obj,
                                              GValue   *value)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (widget));
  if (adjustment == NULL)
    return;

  memset (value,  0, sizeof (GValue));
  g_value_init (value, G_TYPE_DOUBLE);
  g_value_set_double (value, gtk_adjustment_get_upper (adjustment));
}

static void
gtk_spin_button_accessible_get_minimum_value (AtkValue *obj,
                                              GValue   *value)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (widget));
  if (adjustment == NULL)
    return;

  memset (value,  0, sizeof (GValue));
  g_value_init (value, G_TYPE_DOUBLE);
  g_value_set_double (value, gtk_adjustment_get_lower (adjustment));
}

static void
gtk_spin_button_accessible_get_minimum_increment (AtkValue *obj,
                                                  GValue   *value)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (widget));
  if (adjustment == NULL)
    return;

  memset (value,  0, sizeof (GValue));
  g_value_init (value, G_TYPE_DOUBLE);
  g_value_set_double (value, gtk_adjustment_get_minimum_increment (adjustment));
}

static gboolean
gtk_spin_button_accessible_set_current_value (AtkValue     *obj,
                                              const GValue *value)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_spin_button_get_adjustment (GTK_SPIN_BUTTON (widget));
  if (adjustment == NULL)
    return FALSE;

  gtk_adjustment_set_value (adjustment, g_value_get_double (value));

  return TRUE;
}

static void
atk_value_interface_init (AtkValueIface *iface)
{
  iface->get_current_value = gtk_spin_button_accessible_get_current_value;
  iface->get_maximum_value = gtk_spin_button_accessible_get_maximum_value;
  iface->get_minimum_value = gtk_spin_button_accessible_get_minimum_value;
  iface->get_minimum_increment = gtk_spin_button_accessible_get_minimum_increment;
  iface->set_current_value = gtk_spin_button_accessible_set_current_value;
}
