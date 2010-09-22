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

#include "gailprogressbar.h"
#include "gailadjustment.h"

static void	 gail_progress_bar_class_init        (GailProgressBarClass *klass);
static void	 gail_progress_bar_init              (GailProgressBar *bar);
static void      gail_progress_bar_real_initialize   (AtkObject      *obj,
                                                      gpointer       data);

static void	 atk_value_interface_init            (AtkValueIface  *iface);


static void      gail_progress_bar_real_notify_gtk   (GObject        *obj,
                                                      GParamSpec     *pspec);

static void	 gail_progress_bar_get_current_value (AtkValue       *obj,
                                           	      GValue         *value);
static void	 gail_progress_bar_get_maximum_value (AtkValue       *obj,
                                            	      GValue         *value);
static void	 gail_progress_bar_get_minimum_value (AtkValue       *obj,
                                           	      GValue         *value);

G_DEFINE_TYPE_WITH_CODE (GailProgressBar, gail_progress_bar, GAIL_TYPE_WIDGET,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_VALUE, atk_value_interface_init))

static void
gail_progress_bar_class_init		(GailProgressBarClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);
  GailWidgetClass *widget_class;

  widget_class = (GailWidgetClass*)klass;

  widget_class->notify_gtk = gail_progress_bar_real_notify_gtk;

  class->initialize = gail_progress_bar_real_initialize;
}

static void
gail_progress_bar_init (GailProgressBar *bar)
{
}

static void
gail_progress_bar_real_initialize (AtkObject *obj,
                                   gpointer  data)
{
  ATK_OBJECT_CLASS (gail_progress_bar_parent_class)->initialize (obj, data);

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
  GtkWidget *widget;

  g_return_if_fail (GAIL_IS_PROGRESS_BAR (obj));

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (obj));

  g_value_set_double (value, gtk_progress_bar_get_fraction (GTK_PROGRESS_BAR (widget)));
}

static void
gail_progress_bar_get_maximum_value (AtkValue   *obj,
                                     GValue     *value)
{
  g_return_if_fail (GAIL_IS_PROGRESS_BAR (obj));

  g_value_set_double (value, 1.0);
}

static void	 
gail_progress_bar_get_minimum_value (AtkValue    *obj,
                              	     GValue      *value)
{
  g_return_if_fail (GAIL_IS_PROGRESS_BAR (obj));

  g_value_set_double (value, 0.0);
}

static void
gail_progress_bar_real_notify_gtk (GObject           *obj,
                                   GParamSpec        *pspec)
{
  GtkWidget *widget = GTK_WIDGET (obj);
  GailProgressBar *progress_bar = GAIL_PROGRESS_BAR (gtk_widget_get_accessible (widget));

  if (strcmp (pspec->name, "fraction") == 0)
    g_object_notify (G_OBJECT (progress_bar), "accessible-value");
  else
    GAIL_WIDGET_CLASS (gail_progress_bar_parent_class)->notify_gtk (obj, pspec);
}
