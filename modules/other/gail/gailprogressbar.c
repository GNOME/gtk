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

#undef GTK_DISABLE_DEPRECATED

#include <gtk/gtk.h>

#include "gailprogressbar.h"
#include "gailadjustment.h"

static void	 gail_progress_bar_class_init        (GailProgressBarClass *klass);
static void	 gail_progress_bar_init              (GailProgressBar *bar);
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

G_DEFINE_TYPE_WITH_CODE (GailProgressBar, gail_progress_bar, GAIL_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_VALUE, atk_value_interface_init))

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
}

static void
gail_progress_bar_init (GailProgressBar *bar)
{
}

static void
gail_progress_bar_real_initialize (AtkObject *obj,
                                   gpointer  data)
{
  GailProgressBar *progress_bar = GAIL_PROGRESS_BAR (obj);
  GtkProgress *gtk_progress;

  ATK_OBJECT_CLASS (gail_progress_bar_parent_class)->initialize (obj, data);

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

  G_OBJECT_CLASS (gail_progress_bar_parent_class)->finalize (object);
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
    GAIL_WIDGET_CLASS (gail_progress_bar_parent_class)->notify_gtk (obj, pspec);
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
