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

typedef struct
{
  GtkAdjustment *adjustment;
  gulong value_changed_id;
} GtkScrollbarAccessiblePrivate;

static void atk_value_interface_init  (AtkValueIface  *iface);

G_DEFINE_TYPE_WITH_CODE (GtkScrollbarAccessible, gtk_scrollbar_accessible, GTK_TYPE_WIDGET_ACCESSIBLE,
                         G_ADD_PRIVATE (GtkScrollbarAccessible)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_VALUE, atk_value_interface_init))

static void
on_value_changed (GtkAdjustment *adjustment,
                  gpointer       data)
{
  GtkScrollbarAccessible *self = data;

  g_object_notify (G_OBJECT (self), "accessible-value");
}

static void
on_adjustment_changed (GObject    *gobject,
                       GParamSpec *pspec,
                       gpointer    data)
{
  GtkScrollbar *scrollbar = GTK_SCROLLBAR (gobject);
  GtkScrollbarAccessible *self = data;
  GtkScrollbarAccessiblePrivate *priv =
    gtk_scrollbar_accessible_get_instance_private (self);
  GtkAdjustment *adjustment =
    gtk_scrollbar_get_adjustment (scrollbar);

  if (priv->adjustment == adjustment)
    return;

  if (priv->adjustment != NULL && priv->value_changed_id != 0)
    {
      g_signal_handler_disconnect (priv->adjustment, priv->value_changed_id);
      priv->value_changed_id = 0;
    }

  g_clear_object (&priv->adjustment);

  if (adjustment != NULL)
    {
      priv->adjustment = g_object_ref (adjustment);
      priv->value_changed_id =
        g_signal_connect (priv->adjustment, "value-changed",
                          G_CALLBACK (on_value_changed),
                          self);
    }
}

static void
gtk_scrollbar_accessible_dispose (GObject *gobject)
{
  GtkScrollbarAccessible *self = GTK_SCROLLBAR_ACCESSIBLE (gobject);
  GtkScrollbarAccessiblePrivate *priv =
    gtk_scrollbar_accessible_get_instance_private (self);

  if (priv->adjustment != NULL && priv->value_changed_id != 0)
    {
      g_signal_handler_disconnect (priv->adjustment, priv->value_changed_id);
      priv->value_changed_id = 0;
    }

  g_clear_object (&priv->adjustment);

  G_OBJECT_CLASS (gtk_scrollbar_accessible_parent_class)->dispose (gobject);
}

static void
gtk_scrollbar_accessible_initialize (AtkObject *obj,
                                     gpointer   data)
{
  GtkScrollbar *scrollbar = data;

  ATK_OBJECT_CLASS (gtk_scrollbar_accessible_parent_class)->initialize (obj, data);

  g_signal_connect (scrollbar, "notify::adjustment",
                    G_CALLBACK (on_adjustment_changed),
                    obj);
}

static void
gtk_scrollbar_accessible_class_init (GtkScrollbarAccessibleClass *klass)
{
  AtkObjectClass *atk_object_class = ATK_OBJECT_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  atk_object_class->initialize = gtk_scrollbar_accessible_initialize;

  gobject_class->dispose = gtk_scrollbar_accessible_dispose;
}

static void
gtk_scrollbar_accessible_init (GtkScrollbarAccessible *self)
{
  ATK_OBJECT (self)->role = ATK_ROLE_SCROLL_BAR;
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
