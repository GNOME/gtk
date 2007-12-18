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

#include <string.h>
#include <gtk/gtk.h>
#include "gailprogressbar.h"
#include "gailadjustment.h"

static void	 gail_progress_bar_class_init        (GailProgressBarClass *klass);
static void      gail_progress_bar_real_initialize   (AtkObject      *obj,
                                                      gpointer       data);
static void      gail_progress_bar_finalize          (GObject        *object);


static void	 atk_value_interface_init            (AtkValueIface  *iface);


static void      gail_progress_bar_real_notify_gtk   (GObject        *obj,
                                                      GParamSpec     *pspec);

static void	 gail_progress_bar_get_current_value (AtkValue       *obj,
                                           	      GValue         *value);
static void	 gail_progress_bar_get_maximum_value (AtkValue       *obj,
                                            	      GValue         *value);
static void	 gail_progress_bar_get_minimum_value (AtkValue       *obj,
                                           	      GValue         *value);
static void      gail_progress_bar_value_changed     (GtkAdjustment  *adjustment,
                                                      gpointer       data);

static GailWidgetClass *parent_class = NULL;

GType
gail_progress_bar_get_type (void)
{
  static GType type = 0;

  if (!type)
    {
      static const GTypeInfo tinfo =
      {
        sizeof (GailProgressBarClass),
        (GBaseInitFunc) NULL, /* base init */
        (GBaseFinalizeFunc) NULL, /* base finalize */
        (GClassInitFunc) gail_progress_bar_class_init, /* class init */
        (GClassFinalizeFunc) NULL, /* class finalize */
        NULL, /* class data */
        sizeof (GailProgressBar), /* instance size */
        0, /* nb preallocs */
        (GInstanceInitFunc) NULL, /* instance init */
        NULL /* value table */
      };

      static const GInterfaceInfo atk_value_info =
      {
        (GInterfaceInitFunc) atk_value_interface_init,
        (GInterfaceFinalizeFunc) NULL,
        NULL
      };

      type = g_type_register_static (GAIL_TYPE_WIDGET,
                                     "GailProgressBar", &tinfo, 0);

      g_type_add_interface_static (type, ATK_TYPE_VALUE,
                                   &atk_value_info);
    }
  return type;
}

static void	 
gail_progress_bar_class_init		(GailProgressBarClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GailWidgetClass *widget_class;

  widget_class = (GailWidgetClass*)klass;

  widget_class->notify_gtk = gail_progress_bar_real_notify_gtk;

  class->initialize = gail_progress_bar_real_initialize;

  gobject_class->finalize = gail_progress_bar_finalize;

  parent_class = g_type_class_peek_parent (klass);
}

AtkObject* 
gail_progress_bar_new (GtkWidget *widget)
{
  GObject *object;
  AtkObject *accessible;

  g_return_val_if_fail (GTK_IS_PROGRESS_BAR (widget), NULL);

  object = g_object_new (GAIL_TYPE_PROGRESS_BAR, NULL);

  accessible = ATK_OBJECT (object);
  atk_object_initialize (accessible, widget);

  return accessible;
}

static void
gail_progress_bar_real_initialize (AtkObject *obj,
                                   gpointer  data)
{
  GailProgressBar *progress_bar = GAIL_PROGRESS_BAR (obj);
  GtkProgress *gtk_progress;

  ATK_OBJECT_CLASS (parent_class)->initialize (obj, data);

  gtk_progress = GTK_PROGRESS (data);
  /*
   * If a GtkAdjustment already exists for the spin_button,
   * create the GailAdjustment
   */
  if (gtk_progress->adjustment)
    {
      progress_bar->adjustment = gail_adjustment_new (gtk_progress->adjustment);
      g_signal_connect (gtk_progress->adjustment,
                        "value-changed",
                        G_CALLBACK (gail_progress_bar_value_changed),
                        obj);
    }
  else
    progress_bar->adjustment = NULL;

  obj->role = ATK_ROLE_PROGRESS_BAR;
}

static void	 
atk_value_interface_init (AtkValueIface *iface)
{

  g_return_if_fail (iface != NULL);

  iface->get_current_value = gail_progress_bar_get_current_value;
  iface->get_maximum_value = gail_progress_bar_get_maximum_value;
  iface->get_minimum_value = gail_progress_bar_get_minimum_value;
}

static void	 
gail_progress_bar_get_current_value (AtkValue   *obj,
                                     GValue     *value)
{
  GailProgressBar *progress_bar;

  g_return_if_fail (GAIL_IS_PROGRESS_BAR (obj));

  progress_bar = GAIL_PROGRESS_BAR (obj);
  if (progress_bar->adjustment == NULL)
    /*
     * Adjustment has not been specified
     */
    return;

  atk_value_get_current_value (ATK_VALUE (progress_bar->adjustment), value);
}

static void	 
gail_progress_bar_get_maximum_value (AtkValue   *obj,
                                     GValue     *value)
{
  GailProgressBar *progress_bar;

  g_return_if_fail (GAIL_IS_PROGRESS_BAR (obj));

  progress_bar = GAIL_PROGRESS_BAR (obj);
  if (progress_bar->adjustment == NULL)
    /*
     * Adjustment has not been specified
     */
    return;

  atk_value_get_maximum_value (ATK_VALUE (progress_bar->adjustment), value);
}

static void	 
gail_progress_bar_get_minimum_value (AtkValue    *obj,
                              	     GValue      *value)
{
  GailProgressBar *progress_bar;

  g_return_if_fail (GAIL_IS_PROGRESS_BAR (obj));

  progress_bar = GAIL_PROGRESS_BAR (obj);
  if (progress_bar->adjustment == NULL)
    /*
     * Adjustment has not been specified
     */
    return;

  atk_value_get_minimum_value (ATK_VALUE (progress_bar->adjustment), value);
}

static void
gail_progress_bar_finalize (GObject            *object)
{
  GailProgressBar *progress_bar = GAIL_PROGRESS_BAR (object);

  if (progress_bar->adjustment)
    {
      g_object_unref (progress_bar->adjustment);
      progress_bar->adjustment = NULL;
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
gail_progress_bar_real_notify_gtk (GObject           *obj,
                                   GParamSpec        *pspec)
{
  GtkWidget *widget = GTK_WIDGET (obj);
  GailProgressBar *progress_bar = GAIL_PROGRESS_BAR (gtk_widget_get_accessible (widget));

  if (strcmp (pspec->name, "adjustment") == 0)
    {
      /*
       * Get rid of the GailAdjustment for the GtkAdjustment
       * which was associated with the progress_bar.
       */
      if (progress_bar->adjustment)
        {
          g_object_unref (progress_bar->adjustment);
          progress_bar->adjustment = NULL;
        }
      /*
       * Create the GailAdjustment when notify for "adjustment" property
       * is received
       */
      progress_bar->adjustment = gail_adjustment_new (GTK_PROGRESS (widget)->adjustment);
      g_signal_connect (GTK_PROGRESS (widget)->adjustment,
                        "value-changed",
                        G_CALLBACK (gail_progress_bar_value_changed),
                        progress_bar);
    }
  else
    parent_class->notify_gtk (obj, pspec);
}

static void
gail_progress_bar_value_changed (GtkAdjustment    *adjustment,
                                 gpointer         data)
{
  GailProgressBar *progress_bar;

  g_return_if_fail (adjustment != NULL);
  g_return_if_fail (data != NULL);

  progress_bar = GAIL_PROGRESS_BAR (data);

  g_object_notify (G_OBJECT (progress_bar), "accessible-value");
}
