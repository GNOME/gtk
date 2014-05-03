/* GTK+ - accessibility implementations
 * Copyright 2008 Jan Arne Petersen
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

#include <config.h>

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include "gtkscalebuttonaccessible.h"

#include <string.h>


static void atk_action_interface_init (AtkActionIface *iface);
static void atk_value_interface_init  (AtkValueIface  *iface);

G_DEFINE_TYPE_WITH_CODE (GtkScaleButtonAccessible, gtk_scale_button_accessible, GTK_TYPE_BUTTON_ACCESSIBLE,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, atk_action_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_VALUE, atk_value_interface_init));

static void
gtk_scale_button_accessible_value_changed (GtkAdjustment *adjustment,
                                           gpointer       data)
{
  g_object_notify (G_OBJECT (data), "accessible-value");
}

static void
gtk_scale_button_accessible_initialize (AtkObject *obj,
                                        gpointer   data)
{
  GtkAdjustment *adjustment;

  ATK_OBJECT_CLASS (gtk_scale_button_accessible_parent_class)->initialize (obj, data);

  adjustment = gtk_scale_button_get_adjustment (GTK_SCALE_BUTTON (data));
  if (adjustment)
    g_signal_connect (adjustment,
                      "value-changed",
                      G_CALLBACK (gtk_scale_button_accessible_value_changed),
                      obj);

  obj->role = ATK_ROLE_SLIDER;
}

static void
gtk_scale_button_accessible_notify_gtk (GObject    *obj,
                                        GParamSpec *pspec)
{
  GtkScaleButton *scale_button;
  GtkScaleButtonAccessible *accessible;

  scale_button = GTK_SCALE_BUTTON (obj);
  accessible = GTK_SCALE_BUTTON_ACCESSIBLE (gtk_widget_get_accessible (GTK_WIDGET (scale_button)));

  if (strcmp (pspec->name, "adjustment") == 0)
    {
      GtkAdjustment* adjustment;

      adjustment = gtk_scale_button_get_adjustment (scale_button);
      g_signal_connect (adjustment,
                        "value-changed",
                        G_CALLBACK (gtk_scale_button_accessible_value_changed),
                        accessible);
    }
  else
    {
      GTK_WIDGET_ACCESSIBLE_CLASS (gtk_scale_button_accessible_parent_class)->notify_gtk (obj, pspec);
    }
}

static void
gtk_scale_button_accessible_class_init (GtkScaleButtonAccessibleClass *klass)
{
  AtkObjectClass *atk_object_class = ATK_OBJECT_CLASS (klass);
  GtkWidgetAccessibleClass *widget_class = GTK_WIDGET_ACCESSIBLE_CLASS (klass);

  atk_object_class->initialize = gtk_scale_button_accessible_initialize;

  widget_class->notify_gtk = gtk_scale_button_accessible_notify_gtk;
}

static void
gtk_scale_button_accessible_init (GtkScaleButtonAccessible *button)
{
}

static gboolean
gtk_scale_button_accessible_do_action (AtkAction *action,
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
      g_signal_emit_by_name (widget, "popup");
      return TRUE;
    case 1:
      g_signal_emit_by_name (widget, "popdown");
      return TRUE;
    default:
      return FALSE;
    }
}

static gint
gtk_scale_button_accessible_get_n_actions (AtkAction *action)
{
  return 2;
}

static const gchar *
gtk_scale_button_accessible_get_description (AtkAction *action,
                                             gint       i)
{
  switch (i)
    {
    case 0:
      return C_("Action description", "Pops up the slider");
    case 1:
      return C_("Action description", "Dismisses the slider");
    default:
      return NULL;
    }
}

static const gchar *
gtk_scale_button_accessible_action_get_name (AtkAction *action,
                                             gint       i)
{
  switch (i)
    {
    case 0:
      return "popup";
    case 1:
      return "popdown";
    default:
      return NULL;
    }
}

static const gchar *
gtk_scale_button_accessible_action_get_localized_name (AtkAction *action,
                                                       gint       i)
{
  switch (i)
    {
    case 0:
      return C_("Action name", "Popup");
    case 1:
      return C_("Action name", "Dismiss");
    default:
      return NULL;
    }
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
  iface->do_action = gtk_scale_button_accessible_do_action;
  iface->get_n_actions = gtk_scale_button_accessible_get_n_actions;
  iface->get_description = gtk_scale_button_accessible_get_description;
  iface->get_name = gtk_scale_button_accessible_action_get_name;
  iface->get_localized_name = gtk_scale_button_accessible_action_get_localized_name;
}

static void
gtk_scale_button_accessible_get_current_value (AtkValue *obj,
                                               GValue   *value)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_scale_button_get_adjustment (GTK_SCALE_BUTTON (widget));
  if (adjustment == NULL)
    return;

  memset (value,  0, sizeof (GValue));
  g_value_init (value, G_TYPE_DOUBLE);
  g_value_set_double (value, gtk_adjustment_get_value (adjustment));
}

static void
gtk_scale_button_accessible_get_maximum_value (AtkValue *obj,
                                               GValue   *value)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_scale_button_get_adjustment (GTK_SCALE_BUTTON (widget));
  if (adjustment == NULL)
    return;

  memset (value,  0, sizeof (GValue));
  g_value_init (value, G_TYPE_DOUBLE);
  g_value_set_double (value, gtk_adjustment_get_upper (adjustment));
}

static void
gtk_scale_button_accessible_get_minimum_value (AtkValue *obj,
                                               GValue   *value)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_scale_button_get_adjustment (GTK_SCALE_BUTTON (widget));
  if (adjustment == NULL)
    return;

  memset (value,  0, sizeof (GValue));
  g_value_init (value, G_TYPE_DOUBLE);
  g_value_set_double (value, gtk_adjustment_get_lower (adjustment));
}

static void
gtk_scale_button_accessible_get_minimum_increment (AtkValue *obj,
                                                   GValue   *value)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_scale_button_get_adjustment (GTK_SCALE_BUTTON (widget));
  if (adjustment == NULL)
    return;

  memset (value,  0, sizeof (GValue));
  g_value_init (value, G_TYPE_DOUBLE);
  g_value_set_double (value, gtk_adjustment_get_minimum_increment (adjustment));
}

static gboolean
gtk_scale_button_accessible_set_current_value (AtkValue     *obj,
                                               const GValue *value)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_scale_button_get_adjustment (GTK_SCALE_BUTTON (widget));
  if (adjustment == NULL)
    return FALSE;

  gtk_adjustment_set_value (adjustment, g_value_get_double (value));

  return TRUE;
}

static void
gtk_scale_button_accessible_get_value_and_text (AtkValue  *obj,
                                                gdouble   *value,
                                                gchar    **text)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_scale_button_get_adjustment (GTK_SCALE_BUTTON (widget));
  if (adjustment == NULL)
    return;

  *value = gtk_adjustment_get_value (adjustment);
  *text = NULL;
}

static AtkRange *
gtk_scale_button_accessible_get_range (AtkValue *obj)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_scale_button_get_adjustment (GTK_SCALE_BUTTON (widget));
  if (adjustment == NULL)
    return NULL;

  return atk_range_new (gtk_adjustment_get_lower (adjustment),
                        gtk_adjustment_get_upper (adjustment),
                        NULL);
}

static void
gtk_scale_button_accessible_set_value (AtkValue      *obj,
                                       const gdouble  value)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_scale_button_get_adjustment (GTK_SCALE_BUTTON (widget));
  if (adjustment == NULL)
    return;

  gtk_adjustment_set_value (adjustment, value);
}

static gdouble
gtk_scale_button_accessible_get_increment (AtkValue *obj)
{
  GtkWidget *widget;
  GtkAdjustment *adjustment;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));
  adjustment = gtk_scale_button_get_adjustment (GTK_SCALE_BUTTON (widget));
  if (adjustment == NULL)
    return 0;

  return gtk_adjustment_get_minimum_increment (adjustment);
}

static void
atk_value_interface_init (AtkValueIface *iface)
{
  iface->get_current_value = gtk_scale_button_accessible_get_current_value;
  iface->get_maximum_value = gtk_scale_button_accessible_get_maximum_value;
  iface->get_minimum_value = gtk_scale_button_accessible_get_minimum_value;
  iface->get_minimum_increment = gtk_scale_button_accessible_get_minimum_increment;
  iface->set_current_value = gtk_scale_button_accessible_set_current_value;

  iface->get_value_and_text = gtk_scale_button_accessible_get_value_and_text;
  iface->get_range = gtk_scale_button_accessible_get_range;
  iface->set_value = gtk_scale_button_accessible_set_value;
  iface->get_increment = gtk_scale_button_accessible_get_increment;
}
