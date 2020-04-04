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
#include "gtkscrollbaraccessible.h"

struct _GtkScrollbarAccessiblePrivate
{
  GtkAdjustment *adjustment;
};

static void atk_value_interface_init  (AtkValueIface  *iface);

G_DEFINE_TYPE_WITH_CODE (GtkScrollbarAccessible, gtk_scrollbar_accessible, GTK_TYPE_WIDGET_ACCESSIBLE,
                         G_ADD_PRIVATE (GtkScrollbarAccessible)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_VALUE, atk_value_interface_init))

static void
gtk_scrollbar_accessible_value_changed (GtkAdjustment *adjustment,
                                        gpointer       data)
{
  g_object_notify (G_OBJECT (data), "accessible-value");
}

static void
gtk_scrollbar_accessible_widget_set (GtkAccessible *accessible)
{
  GtkScrollbarAccessiblePrivate *priv = GTK_SCROLLBAR_ACCESSIBLE (accessible)->priv;
  GtkWidget *scrollbar;
  GtkAdjustment *adj;

  scrollbar = gtk_accessible_get_widget (accessible);
  adj = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (scrollbar));
  if (adj)
    {
      priv->adjustment = adj;
      g_object_ref (priv->adjustment);
      g_signal_connect (priv->adjustment, "value-changed",
                        G_CALLBACK (gtk_scrollbar_accessible_value_changed),
                        accessible);
    }
}

static void
gtk_scrollbar_accessible_widget_unset (GtkAccessible *accessible)
{
  GtkScrollbarAccessiblePrivate *priv = GTK_SCROLLBAR_ACCESSIBLE (accessible)->priv;

  if (priv->adjustment)
    {
      g_signal_handlers_disconnect_by_func (priv->adjustment,
                                            G_CALLBACK (gtk_scrollbar_accessible_value_changed),
                                            accessible);
      g_object_unref (priv->adjustment);
      priv->adjustment = NULL;
    }
}

static void
gtk_scrollbar_accessible_initialize (AtkObject *obj,
                                     gpointer   data)
{
  ATK_OBJECT_CLASS (gtk_scrollbar_accessible_parent_class)->initialize (obj, data);
  obj->role = ATK_ROLE_SCROLL_BAR;
}

static void
gtk_scrollbar_accessible_notify_gtk (GObject    *obj,
                                     GParamSpec *pspec)
{
  GtkWidget *widget = GTK_WIDGET (obj);
  AtkObject *scrollbar;

  if (strcmp (pspec->name, "adjustment") == 0)
    {
      scrollbar = gtk_widget_get_accessible (widget);
      gtk_scrollbar_accessible_widget_unset (GTK_ACCESSIBLE (scrollbar));
      gtk_scrollbar_accessible_widget_set (GTK_ACCESSIBLE (scrollbar));
    }
  else
    GTK_WIDGET_ACCESSIBLE_CLASS (gtk_scrollbar_accessible_parent_class)->notify_gtk (obj, pspec);
}


static void
gtk_scrollbar_accessible_class_init (GtkScrollbarAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GtkAccessibleClass *accessible_class = (GtkAccessibleClass*)klass;
  GtkWidgetAccessibleClass *widget_class = (GtkWidgetAccessibleClass*)klass;

  class->initialize = gtk_scrollbar_accessible_initialize;

  accessible_class->widget_set = gtk_scrollbar_accessible_widget_set;
  accessible_class->widget_unset = gtk_scrollbar_accessible_widget_unset;

  widget_class->notify_gtk = gtk_scrollbar_accessible_notify_gtk;
}

static void
gtk_scrollbar_accessible_init (GtkScrollbarAccessible *scrollbar)
{
  scrollbar->priv = gtk_scrollbar_accessible_get_instance_private (scrollbar);
}

static void
gtk_scrollbar_accessible_get_current_value (AtkValue *obj,
                                            GValue   *value)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (widget));
  if (adjustment == NULL)
    return;

  memset (value,  0, sizeof (GValue));
  g_value_init (value, G_TYPE_DOUBLE);
  g_value_set_double (value, gtk_adjustment_get_value (adjustment));
}

static void
gtk_scrollbar_accessible_get_maximum_value (AtkValue *obj,
                                            GValue   *value)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;
  double max;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (widget));
  if (adjustment == NULL)
    return;

  max = gtk_adjustment_get_upper (adjustment)
        - gtk_adjustment_get_page_size (adjustment);

  memset (value,  0, sizeof (GValue));
  g_value_init (value, G_TYPE_DOUBLE);
  g_value_set_double (value, max);
}

static void
gtk_scrollbar_accessible_get_minimum_value (AtkValue *obj,
                                            GValue   *value)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (widget));
  if (adjustment == NULL)
    return;

  memset (value,  0, sizeof (GValue));
  g_value_init (value, G_TYPE_DOUBLE);
  g_value_set_double (value, gtk_adjustment_get_lower (adjustment));
}

static void
gtk_scrollbar_accessible_get_minimum_increment (AtkValue *obj,
                                                GValue   *value)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (widget));
  if (adjustment == NULL)
    return;

  memset (value,  0, sizeof (GValue));
  g_value_init (value, G_TYPE_DOUBLE);
  g_value_set_double (value, gtk_adjustment_get_minimum_increment (adjustment));
}

static gboolean
gtk_scrollbar_accessible_set_current_value (AtkValue     *obj,
                                            const GValue *value)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (widget));
  if (adjustment == NULL)
    return FALSE;

  gtk_adjustment_set_value (adjustment, g_value_get_double (value));

  return TRUE;
}

static void
gtk_scrollbar_accessible_get_value_and_text (AtkValue  *obj,
                                             double    *value,
                                             char     **text)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (widget));
  if (adjustment == NULL)
    return;

  *value = gtk_adjustment_get_value (adjustment);
  *text = NULL;
}

static AtkRange *
gtk_scrollbar_accessible_get_range (AtkValue *obj)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;
  double min, max;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (widget));
  if (adjustment == NULL)
    return NULL;

  min = gtk_adjustment_get_lower (adjustment);
  max = gtk_adjustment_get_upper (adjustment)
        - gtk_adjustment_get_page_size (adjustment);

  return atk_range_new (min, max, NULL);
}

static void
gtk_scrollbar_accessible_set_value (AtkValue     *obj,
                                    const double  value)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (widget));
  if (adjustment == NULL)
    return;

  gtk_adjustment_set_value (adjustment, value);
}

static double
gtk_scrollbar_accessible_get_increment (AtkValue *obj)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_scrollbar_get_adjustment (GTK_SCROLLBAR (widget));
  if (adjustment == NULL)
    return 0;

  return gtk_adjustment_get_minimum_increment (adjustment);
}

static void
atk_value_interface_init (AtkValueIface *iface)
{
  iface->get_current_value = gtk_scrollbar_accessible_get_current_value;
  iface->get_maximum_value = gtk_scrollbar_accessible_get_maximum_value;
  iface->get_minimum_value = gtk_scrollbar_accessible_get_minimum_value;
  iface->get_minimum_increment = gtk_scrollbar_accessible_get_minimum_increment;
  iface->set_current_value = gtk_scrollbar_accessible_set_current_value;

  iface->get_value_and_text = gtk_scrollbar_accessible_get_value_and_text;
  iface->get_range = gtk_scrollbar_accessible_get_range;
  iface->set_value = gtk_scrollbar_accessible_set_value;
  iface->get_increment = gtk_scrollbar_accessible_get_increment;
}
