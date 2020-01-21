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

struct _GtkRangeAccessiblePrivate
{
  GtkAdjustment *adjustment;
};

static void atk_value_interface_init  (AtkValueIface  *iface);

G_DEFINE_TYPE_WITH_CODE (GtkRangeAccessible, gtk_range_accessible, GTK_TYPE_WIDGET_ACCESSIBLE,
                         G_ADD_PRIVATE (GtkRangeAccessible)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_VALUE, atk_value_interface_init))

static void
gtk_range_accessible_value_changed (GtkAdjustment *adjustment,
                                    gpointer       data)
{
  g_object_notify (G_OBJECT (data), "accessible-value");
}

static void
gtk_range_accessible_widget_set (GtkAccessible *accessible)
{
  GtkRangeAccessiblePrivate *priv = GTK_RANGE_ACCESSIBLE (accessible)->priv;
  GtkWidget *range;
  GtkAdjustment *adj;

  range = gtk_accessible_get_widget (accessible);
  adj = gtk_range_get_adjustment (GTK_RANGE (range));
  if (adj)
    {
      priv->adjustment = adj;
      g_object_ref (priv->adjustment);
      g_signal_connect (priv->adjustment, "value-changed",
                        G_CALLBACK (gtk_range_accessible_value_changed),
                        accessible);
    }
}

static void
gtk_range_accessible_widget_unset (GtkAccessible *accessible)
{
  GtkRangeAccessiblePrivate *priv = GTK_RANGE_ACCESSIBLE (accessible)->priv;

  if (priv->adjustment)
    {
      g_signal_handlers_disconnect_by_func (priv->adjustment,
                                            G_CALLBACK (gtk_range_accessible_value_changed),
                                            accessible);
      g_object_unref (priv->adjustment);
      priv->adjustment = NULL;
    }
}

static void
gtk_range_accessible_initialize (AtkObject *obj,
                                 gpointer   data)
{
  ATK_OBJECT_CLASS (gtk_range_accessible_parent_class)->initialize (obj, data);
  obj->role = ATK_ROLE_SLIDER;
}

static void
gtk_range_accessible_notify_gtk (GObject    *obj,
                                 GParamSpec *pspec)
{
  GtkWidget *widget = GTK_WIDGET (obj);
  AtkObject *range;

  if (strcmp (pspec->name, "adjustment") == 0)
    {
      range = gtk_widget_get_accessible (widget);
      gtk_range_accessible_widget_unset (GTK_ACCESSIBLE (range));
      gtk_range_accessible_widget_set (GTK_ACCESSIBLE (range));
    }
  else
    GTK_WIDGET_ACCESSIBLE_CLASS (gtk_range_accessible_parent_class)->notify_gtk (obj, pspec);
}


static void
gtk_range_accessible_class_init (GtkRangeAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GtkAccessibleClass *accessible_class = (GtkAccessibleClass*)klass;
  GtkWidgetAccessibleClass *widget_class = (GtkWidgetAccessibleClass*)klass;

  class->initialize = gtk_range_accessible_initialize;

  accessible_class->widget_set = gtk_range_accessible_widget_set;
  accessible_class->widget_unset = gtk_range_accessible_widget_unset;

  widget_class->notify_gtk = gtk_range_accessible_notify_gtk;
}

static void
gtk_range_accessible_init (GtkRangeAccessible *range)
{
  range->priv = gtk_range_accessible_get_instance_private (range);
}

static void
gtk_range_accessible_get_value_and_text (AtkValue  *obj,
                                         gdouble   *value,
                                         gchar    **text)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_range_get_adjustment (GTK_RANGE (widget));
  if (adjustment == NULL)
    return;

  *value = gtk_adjustment_get_value (adjustment);
  *text = NULL;
}

static AtkRange *
gtk_range_accessible_get_range (AtkValue *obj)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;
  gdouble min, max;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_range_get_adjustment (GTK_RANGE (widget));
  if (adjustment == NULL)
    return NULL;

  min = gtk_adjustment_get_lower (adjustment);
  max = gtk_adjustment_get_upper (adjustment)
        - gtk_adjustment_get_page_size (adjustment);

  if (gtk_range_get_restrict_to_fill_level (GTK_RANGE (widget)))
    max = MIN (max, gtk_range_get_fill_level (GTK_RANGE (widget)));

  return atk_range_new (min, max, NULL);
}

static void
gtk_range_accessible_set_value (AtkValue      *obj,
                                const gdouble  value)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_range_get_adjustment (GTK_RANGE (widget));
  if (adjustment == NULL)
    return;

  gtk_adjustment_set_value (adjustment, value);
}

static gdouble
gtk_range_accessible_get_increment (AtkValue *obj)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_range_get_adjustment (GTK_RANGE (widget));
  if (adjustment == NULL)
    return 0;

  return gtk_adjustment_get_minimum_increment (adjustment);
}

static void
atk_value_interface_init (AtkValueIface *iface)
{
  iface->get_value_and_text = gtk_range_accessible_get_value_and_text;
  iface->get_range = gtk_range_accessible_get_range;
  iface->set_value = gtk_range_accessible_set_value;
  iface->get_increment = gtk_range_accessible_get_increment;
}
