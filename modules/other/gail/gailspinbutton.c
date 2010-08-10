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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <string.h>
#include <gtk/gtk.h>
#include "gailspinbutton.h"
#include "gailadjustment.h"
#include "gail-private-macros.h"

static void      gail_spin_button_class_init        (GailSpinButtonClass *klass);
static void      gail_spin_button_init              (GailSpinButton *button);
static void      gail_spin_button_real_initialize   (AtkObject      *obj,
                                                     gpointer       data);
static void      gail_spin_button_finalize          (GObject        *object);

static void      atk_value_interface_init           (AtkValueIface  *iface);

static void      gail_spin_button_real_notify_gtk   (GObject        *obj,
                                                     GParamSpec     *pspec);

static void      gail_spin_button_get_current_value (AtkValue       *obj,
                                                     GValue         *value);
static void      gail_spin_button_get_maximum_value (AtkValue       *obj,
                                                     GValue         *value);
static void      gail_spin_button_get_minimum_value (AtkValue       *obj,
                                                     GValue         *value);
static void      gail_spin_button_get_minimum_increment (AtkValue       *obj,
                                                         GValue         *value);
static gboolean  gail_spin_button_set_current_value (AtkValue       *obj,
                                                     const GValue   *value);
static void      gail_spin_button_value_changed     (GtkAdjustment  *adjustment,
                                                     gpointer       data);
        
G_DEFINE_TYPE_WITH_CODE (GailSpinButton, gail_spin_button, GAIL_TYPE_ENTRY,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_VALUE, atk_value_interface_init))

static void
gail_spin_button_class_init (GailSpinButtonClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GailWidgetClass *widget_class;

  widget_class = (GailWidgetClass*)klass;

  widget_class->notify_gtk = gail_spin_button_real_notify_gtk;

  class->initialize = gail_spin_button_real_initialize;

  gobject_class->finalize = gail_spin_button_finalize;
}

static void
gail_spin_button_init (GailSpinButton *button)
{
}

static void
gail_spin_button_real_initialize (AtkObject *obj,
                                  gpointer  data)
{
  GailSpinButton *spin_button = GAIL_SPIN_BUTTON (obj);
  GtkSpinButton *gtk_spin_button;

  ATK_OBJECT_CLASS (gail_spin_button_parent_class)->initialize (obj, data);

  gtk_spin_button = GTK_SPIN_BUTTON (data); 
  /*
   * If a GtkAdjustment already exists for the spin_button, 
   * create the GailAdjustment
   */
  if (gtk_spin_button->adjustment)
    {
      spin_button->adjustment = gail_adjustment_new (gtk_spin_button->adjustment);
      g_signal_connect (gtk_spin_button->adjustment,
                        "value-changed",
                        G_CALLBACK (gail_spin_button_value_changed),
                        obj);
    }
  else
    spin_button->adjustment = NULL;

  obj->role = ATK_ROLE_SPIN_BUTTON;

}

static void
atk_value_interface_init (AtkValueIface *iface)
{
  iface->get_current_value = gail_spin_button_get_current_value;
  iface->get_maximum_value = gail_spin_button_get_maximum_value;
  iface->get_minimum_value = gail_spin_button_get_minimum_value;
  iface->get_minimum_increment = gail_spin_button_get_minimum_increment;
  iface->set_current_value = gail_spin_button_set_current_value;
}

static void
gail_spin_button_get_current_value (AtkValue       *obj,
                                    GValue         *value)
{
  GailSpinButton *spin_button;

  g_return_if_fail (GAIL_IS_SPIN_BUTTON (obj));

  spin_button = GAIL_SPIN_BUTTON (obj);
  if (spin_button->adjustment == NULL)
    /*
     * Adjustment has not been specified
     */
    return;

  atk_value_get_current_value (ATK_VALUE (spin_button->adjustment), value);
}

static void      
gail_spin_button_get_maximum_value (AtkValue       *obj,
                                    GValue         *value)
{
  GailSpinButton *spin_button;

  g_return_if_fail (GAIL_IS_SPIN_BUTTON (obj));

  spin_button = GAIL_SPIN_BUTTON (obj);
  if (spin_button->adjustment == NULL)
    /*
     * Adjustment has not been specified
     */
    return;

  atk_value_get_maximum_value (ATK_VALUE (spin_button->adjustment), value);
}

static void 
gail_spin_button_get_minimum_value (AtkValue       *obj,
                                    GValue         *value)
{
 GailSpinButton *spin_button;

  g_return_if_fail (GAIL_IS_SPIN_BUTTON (obj));

  spin_button = GAIL_SPIN_BUTTON (obj);
  if (spin_button->adjustment == NULL)
    /*
     * Adjustment has not been specified
     */
    return;

  atk_value_get_minimum_value (ATK_VALUE (spin_button->adjustment), value);
}

static void
gail_spin_button_get_minimum_increment (AtkValue *obj, GValue *value)
{
 GailSpinButton *spin_button;

  g_return_if_fail (GAIL_IS_SPIN_BUTTON (obj));

  spin_button = GAIL_SPIN_BUTTON (obj);
  if (spin_button->adjustment == NULL)
    /*
     * Adjustment has not been specified
     */
    return;

  atk_value_get_minimum_increment (ATK_VALUE (spin_button->adjustment), value);
}

static gboolean  
gail_spin_button_set_current_value (AtkValue       *obj,
                                    const GValue   *value)
{
 GailSpinButton *spin_button;

  g_return_val_if_fail (GAIL_IS_SPIN_BUTTON (obj), FALSE);

  spin_button = GAIL_SPIN_BUTTON (obj);
  if (spin_button->adjustment == NULL)
    /*
     * Adjustment has not been specified
     */
    return FALSE;

  return atk_value_set_current_value (ATK_VALUE (spin_button->adjustment), value);
}

static void
gail_spin_button_finalize (GObject            *object)
{
  GailSpinButton *spin_button = GAIL_SPIN_BUTTON (object);

  if (spin_button->adjustment)
    {
      g_object_unref (spin_button->adjustment);
      spin_button->adjustment = NULL;
    }
  G_OBJECT_CLASS (gail_spin_button_parent_class)->finalize (object);
}


static void
gail_spin_button_real_notify_gtk (GObject    *obj,
                                  GParamSpec *pspec)
{
  GtkWidget *widget = GTK_WIDGET (obj);
  GailSpinButton *spin_button = GAIL_SPIN_BUTTON (gtk_widget_get_accessible (widget));

  if (strcmp (pspec->name, "adjustment") == 0)
    {
      /*
       * Get rid of the GailAdjustment for the GtkAdjustment
       * which was associated with the spin_button.
       */
      GtkSpinButton* gtk_spin_button;

      if (spin_button->adjustment)
        {
          g_object_unref (spin_button->adjustment);
          spin_button->adjustment = NULL;
        }
      /*
       * Create the GailAdjustment when notify for "adjustment" property
       * is received
       */
      gtk_spin_button = GTK_SPIN_BUTTON (widget);
      spin_button->adjustment = gail_adjustment_new (gtk_spin_button->adjustment);
      g_signal_connect (gtk_spin_button->adjustment,
                        "value-changed",
                        G_CALLBACK (gail_spin_button_value_changed),
                        spin_button);
    }
  else
    GAIL_WIDGET_CLASS (gail_spin_button_parent_class)->notify_gtk (obj, pspec);
}


static void
gail_spin_button_value_changed (GtkAdjustment    *adjustment,
                                gpointer         data)
{
  GailSpinButton *spin_button;

  gail_return_if_fail (adjustment != NULL);
  gail_return_if_fail (data != NULL);

  spin_button = GAIL_SPIN_BUTTON (data);

  g_object_notify (G_OBJECT (spin_button), "accessible-value");
}

