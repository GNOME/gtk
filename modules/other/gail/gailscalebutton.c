/* GAIL - The GNOME Accessibility Implementation Library
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <gtk/gtk.h>
#include "gailscalebutton.h"
#include "gailadjustment.h"
#include "gail-private-macros.h"

#include <string.h>

static void gail_scale_button_class_init (GailScaleButtonClass *klass);
static void gail_scale_button_init       (GailScaleButton      *button);

/* GailWidget */
static void gail_scale_button_notify_gtk (GObject    *obj,
                                          GParamSpec *pspec);

/* AtkObject */
static void gail_scale_button_initialize (AtkObject *obj,
                                          gpointer   data);

/* AtkAction */
static void                  atk_action_interface_init        (AtkActionIface *iface);
static gboolean              gail_scale_button_do_action      (AtkAction      *action,
                                                               gint           i);
static gint                  gail_scale_button_get_n_actions  (AtkAction      *action);
static const gchar*          gail_scale_button_get_description(AtkAction      *action,
                                                               gint           i);
static const gchar*          gail_scale_button_action_get_name(AtkAction      *action,
                                                               gint           i);
static const gchar*          gail_scale_button_get_keybinding (AtkAction      *action,
                                                               gint           i);
static gboolean              gail_scale_button_set_description(AtkAction      *action,
                                                               gint           i,
                                                               const gchar    *desc);

/* AtkValue */
static void	atk_value_interface_init	        (AtkValueIface  *iface);
static void	gail_scale_button_get_current_value     (AtkValue       *obj,
                                                         GValue         *value);
static void	gail_scale_button_get_maximum_value     (AtkValue       *obj,
                                                         GValue         *value);
static void	gail_scale_button_get_minimum_value     (AtkValue       *obj,
                                                         GValue         *value);
static void	gail_scale_button_get_minimum_increment (AtkValue       *obj,
                                                         GValue         *value);
static gboolean	gail_scale_button_set_current_value     (AtkValue       *obj,
                                                         const GValue   *value);

G_DEFINE_TYPE_WITH_CODE (GailScaleButton, gail_scale_button, GAIL_TYPE_BUTTON,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, atk_action_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_VALUE, atk_value_interface_init));

static void
gail_scale_button_class_init (GailScaleButtonClass *klass)
{
  AtkObjectClass *atk_object_class = ATK_OBJECT_CLASS (klass);
  GailWidgetClass *widget_class = GAIL_WIDGET_CLASS (klass);

  atk_object_class->initialize = gail_scale_button_initialize;

  widget_class->notify_gtk = gail_scale_button_notify_gtk;
}

static void
gail_scale_button_init (GailScaleButton *button)
{
}

static void
gail_scale_button_initialize (AtkObject *obj,
                              gpointer   data)
{
  ATK_OBJECT_CLASS (gail_scale_button_parent_class)->initialize (obj, data);

  obj->role = ATK_ROLE_SLIDER;
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
  iface->do_action = gail_scale_button_do_action;
  iface->get_n_actions = gail_scale_button_get_n_actions;
  iface->get_description = gail_scale_button_get_description;
  iface->get_keybinding = gail_scale_button_get_keybinding;
  iface->get_name = gail_scale_button_action_get_name;
  iface->set_description = gail_scale_button_set_description;
}

static gboolean
gail_scale_button_do_action(AtkAction *action,
                            gint       i)
{
  GtkWidget *widget;

  widget = GTK_ACCESSIBLE (action)->widget;
  if (widget == NULL)
    return FALSE;

  if (!gtk_widget_is_sensitive (widget) || !gtk_widget_get_visible (widget))
    return FALSE;

  switch (i) {
    case 0:
      g_signal_emit_by_name (widget, "popup");
      return TRUE;
    case 1:
      g_signal_emit_by_name (widget, "podown");
      return TRUE;
    default:
      return FALSE;
  }
}

static gint
gail_scale_button_get_n_actions (AtkAction *action)
{
  return 2;
}

static const gchar*
gail_scale_button_get_description (AtkAction *action,
                                   gint       i)
{
  return NULL;
}

static const gchar*
gail_scale_button_action_get_name (AtkAction *action,
                                   gint       i)
{
  switch (i) {
    case 0:
      return "popup";
    case 1:
      return "popdown";
    default:
      return NULL;
  }
}

static const gchar*
gail_scale_button_get_keybinding (AtkAction *action,
                                  gint       i)
{
  return NULL;
}

static gboolean
gail_scale_button_set_description (AtkAction   *action,
                                   gint         i,
                                   const gchar *desc)
{
  return FALSE;
}


static void
atk_value_interface_init (AtkValueIface *iface)
{
  iface->get_current_value = gail_scale_button_get_current_value;
  iface->get_maximum_value = gail_scale_button_get_maximum_value;
  iface->get_minimum_value = gail_scale_button_get_minimum_value;
  iface->get_minimum_increment = gail_scale_button_get_minimum_increment;
  iface->set_current_value = gail_scale_button_set_current_value;
}

static void
gail_scale_button_get_current_value (AtkValue *obj,
                                     GValue   *value)
{
  GtkScaleButton *gtk_scale_button;

  g_return_if_fail (GAIL_IS_SCALE_BUTTON (obj));

  gtk_scale_button = GTK_SCALE_BUTTON (GTK_ACCESSIBLE (obj)->widget);

  g_value_set_double (g_value_init (value, G_TYPE_DOUBLE),
                      gtk_scale_button_get_value (gtk_scale_button));
}

static void
gail_scale_button_get_maximum_value (AtkValue *obj,
                                     GValue   *value)
{
  GtkWidget *gtk_widget;
  GtkAdjustment *adj;

  g_return_if_fail (GAIL_IS_SCALE_BUTTON (obj));

  gtk_widget = GTK_ACCESSIBLE (obj)->widget;
  if (gtk_widget == NULL)
    return;

  adj = gtk_scale_button_get_adjustment (GTK_SCALE_BUTTON (gtk_widget));
  if (adj != NULL)
    g_value_set_double (g_value_init (value, G_TYPE_DOUBLE),
                        adj->upper);
}

static void
gail_scale_button_get_minimum_value (AtkValue *obj,
                                     GValue   *value)
{
  GtkWidget *gtk_widget;
  GtkAdjustment *adj;

  g_return_if_fail (GAIL_IS_SCALE_BUTTON (obj));

  gtk_widget = GTK_ACCESSIBLE (obj)->widget;
  if (gtk_widget == NULL)
    return;

  adj = gtk_scale_button_get_adjustment (GTK_SCALE_BUTTON (gtk_widget));
  if (adj != NULL)
    g_value_set_double (g_value_init (value, G_TYPE_DOUBLE),
                        adj->lower);
}

static void
gail_scale_button_get_minimum_increment (AtkValue *obj,
                                         GValue   *value)
{
  GtkWidget *gtk_widget;
  GtkAdjustment *adj;

  g_return_if_fail (GAIL_IS_SCALE_BUTTON (obj));

  gtk_widget = GTK_ACCESSIBLE (obj)->widget;
  if (gtk_widget == NULL)
    return;

  adj = gtk_scale_button_get_adjustment (GTK_SCALE_BUTTON (gtk_widget));
  if (adj != NULL)
    g_value_set_double (g_value_init (value, G_TYPE_DOUBLE),
                        adj->step_increment);
}

static gboolean
gail_scale_button_set_current_value (AtkValue     *obj,
                                     const GValue *value)
{
  GtkWidget *gtk_widget;

  g_return_val_if_fail (GAIL_IS_SCALE_BUTTON (obj), FALSE);

  gtk_widget = GTK_ACCESSIBLE (obj)->widget;
  if (gtk_widget == NULL)
    return FALSE;

  if (G_VALUE_HOLDS_DOUBLE (value))
    {
      gtk_scale_button_set_value (GTK_SCALE_BUTTON (gtk_widget), g_value_get_double (value));
      return TRUE;
    }
  return FALSE;
}

static void
gail_scale_button_notify_gtk (GObject    *obj,
                              GParamSpec *pspec)
{
  GtkScaleButton *gtk_scale_button;
  GailScaleButton *scale_button;

  g_return_if_fail (GTK_IS_SCALE_BUTTON (obj));

  gtk_scale_button = GTK_SCALE_BUTTON (obj);
  scale_button = GAIL_SCALE_BUTTON (gtk_widget_get_accessible (GTK_WIDGET (gtk_scale_button)));

  if (strcmp (pspec->name, "value") == 0)
    {
      g_object_notify (G_OBJECT (scale_button), "accessible-value");
    }
  else
    {
      GAIL_WIDGET_CLASS (gail_scale_button_parent_class)->notify_gtk (obj, pspec);
    }
}


