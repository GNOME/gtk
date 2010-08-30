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
static void gail_scale_button_real_notify_gtk (GObject    *obj,
                                               GParamSpec *pspec);

/* AtkObject */
static void gail_scale_button_real_initialize (AtkObject *obj,
                                               gpointer   data);

/* AtkAction */
static void                  atk_action_interface_init        (AtkActionIface *iface);
static gboolean              gail_scale_button_do_action      (AtkAction      *action,
                                                               gint           i);
static gint                  gail_scale_button_get_n_actions  (AtkAction      *action);
static G_CONST_RETURN gchar* gail_scale_button_get_description(AtkAction      *action,
                                                               gint           i);
static G_CONST_RETURN gchar* gail_scale_button_action_get_name(AtkAction      *action,
                                                               gint           i);
static G_CONST_RETURN gchar* gail_scale_button_get_keybinding (AtkAction      *action,
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
static void     gail_scale_button_value_changed         (GtkAdjustment  *adjustment,
                                                         gpointer       data);

G_DEFINE_TYPE_WITH_CODE (GailScaleButton, gail_scale_button, GAIL_TYPE_BUTTON,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, atk_action_interface_init)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_VALUE, atk_value_interface_init));

static void
gail_scale_button_class_init (GailScaleButtonClass *klass)
{
  AtkObjectClass *atk_object_class = ATK_OBJECT_CLASS (klass);
  GailWidgetClass *widget_class = GAIL_WIDGET_CLASS (klass);

  atk_object_class->initialize = gail_scale_button_real_initialize;

  widget_class->notify_gtk = gail_scale_button_real_notify_gtk;
}

static void
gail_scale_button_init (GailScaleButton *button)
{
}

static void
gail_scale_button_real_initialize (AtkObject *obj,
                                   gpointer  data)
{
  GailScaleButton *scale_button = GAIL_SCALE_BUTTON (obj);
  GtkScaleButton *gtk_scale_button;
  GtkAdjustment *gtk_adjustment;

  ATK_OBJECT_CLASS (gail_scale_button_parent_class)->initialize (obj, data);

  gtk_scale_button = GTK_SCALE_BUTTON (data);
  gtk_adjustment = gtk_scale_button_get_adjustment (gtk_scale_button);
  /*
   * If a GtkAdjustment already exists for the scale_button,
   * create the GailAdjustment
   */
  if (gtk_adjustment)
    {
      scale_button->adjustment = gail_adjustment_new (gtk_adjustment);
      g_signal_connect (gtk_adjustment,
                        "value-changed",
                        G_CALLBACK (gail_scale_button_value_changed),
                        obj);
    }
  else
    scale_button->adjustment = NULL;

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

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (action));
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

static G_CONST_RETURN gchar*
gail_scale_button_get_description (AtkAction *action,
                                   gint       i)
{
  return NULL;
}

static G_CONST_RETURN gchar*
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

static G_CONST_RETURN gchar*
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
  GailScaleButton *scale_button;

  g_return_if_fail (GAIL_IS_SCALE_BUTTON (obj));

  scale_button = GAIL_SCALE_BUTTON (obj);
  if (scale_button->adjustment == NULL)
    /*
     * Adjustment has not been specified
     */
    return;

  atk_value_get_current_value (ATK_VALUE (scale_button->adjustment), value);
}

static void
gail_scale_button_get_maximum_value (AtkValue *obj,
                                     GValue   *value)
{
  GailScaleButton *scale_button;

  g_return_if_fail (GAIL_IS_SCALE_BUTTON (obj));

  scale_button = GAIL_SCALE_BUTTON (obj);
  if (scale_button->adjustment == NULL)
    /*
     * Adjustment has not been specified
     */
    return;

  atk_value_get_maximum_value (ATK_VALUE (scale_button->adjustment), value);
}

static void
gail_scale_button_get_minimum_value (AtkValue *obj,
                                     GValue   *value)
{
  GailScaleButton *scale_button;

  g_return_if_fail (GAIL_IS_SCALE_BUTTON (obj));

  scale_button = GAIL_SCALE_BUTTON (obj);
  if (scale_button->adjustment == NULL)
    /*
     * Adjustment has not been specified
     */
    return;

  atk_value_get_minimum_value (ATK_VALUE (scale_button->adjustment), value);
}

static void
gail_scale_button_get_minimum_increment (AtkValue *obj,
                                         GValue   *value)
{
  GailScaleButton *scale_button;

  g_return_if_fail (GAIL_IS_SCALE_BUTTON (obj));

  scale_button = GAIL_SCALE_BUTTON (obj);
  if (scale_button->adjustment == NULL)
    /*
     * Adjustment has not been specified
     */
    return;

  atk_value_get_minimum_increment (ATK_VALUE (scale_button->adjustment), value);
}

static gboolean
gail_scale_button_set_current_value (AtkValue     *obj,
                                     const GValue *value)
{
  GailScaleButton *scale_button;

  g_return_val_if_fail (GAIL_IS_SCALE_BUTTON (obj), FALSE);

  scale_button = GAIL_SCALE_BUTTON (obj);
  if (scale_button->adjustment == NULL)
    /*
     * Adjustment has not been specified
     */
    return FALSE;

  return atk_value_set_current_value (ATK_VALUE (scale_button->adjustment), value);
}

static void
gail_scale_button_real_notify_gtk (GObject    *obj,
                                   GParamSpec *pspec)
{
  GtkScaleButton *gtk_scale_button;
  GailScaleButton *scale_button;

  g_return_if_fail (GTK_IS_SCALE_BUTTON (obj));

  gtk_scale_button = GTK_SCALE_BUTTON (obj);
  scale_button = GAIL_SCALE_BUTTON (gtk_widget_get_accessible (GTK_WIDGET (gtk_scale_button)));

  if (strcmp (pspec->name, "adjustment") == 0)
    {
      /*
       * Get rid of the GailAdjustment for the GtkAdjustment
       * which was associated with the scale_button.
       */
      GtkAdjustment* gtk_adjustment;

      if (scale_button->adjustment)
        {
          g_object_unref (scale_button->adjustment);
          scale_button->adjustment = NULL;
        }
      /*
       * Create the GailAdjustment when notify for "adjustment" property
       * is received
       */
      gtk_adjustment = gtk_scale_button_get_adjustment (gtk_scale_button);
      scale_button->adjustment = gail_adjustment_new (gtk_adjustment);
      g_signal_connect (gtk_adjustment,
                        "value-changed",
                        G_CALLBACK (gail_scale_button_value_changed),
                        scale_button);
    }
  else
    {
      GAIL_WIDGET_CLASS (gail_scale_button_parent_class)->notify_gtk (obj, pspec);
    }
}

static void
gail_scale_button_value_changed (GtkAdjustment    *adjustment,
                                 gpointer         data)
{
  GailScaleButton *scale_button;

  gail_return_if_fail (adjustment != NULL);
  gail_return_if_fail (data != NULL);

  scale_button = GAIL_SCALE_BUTTON (data);

  g_object_notify (G_OBJECT (scale_button), "accessible-value");
}

