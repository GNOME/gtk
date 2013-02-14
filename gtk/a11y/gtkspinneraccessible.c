/* GTK+ - accessibility implementations
 * Copyright (C) 2007 John Stowers, Neil Jagdish Patel.
 * Copyright (C) 2009 Bastien Nocera, David Zeuthen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Code adapted from egg-spinner
 * by Christian Hergert <christian.hergert@gmail.com>
 */

#include "config.h"

#include <gtk/gtk.h>
#include "gtkintl.h"
#include "gtkspinneraccessible.h"

static void atk_image_interface_init (AtkImageIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkSpinnerAccessible, gtk_spinner_accessible, GTK_TYPE_WIDGET_ACCESSIBLE,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_IMAGE, atk_image_interface_init));

static void
gtk_spinner_accessible_initialize (AtkObject *accessible,
                                   gpointer   widget)
{
  ATK_OBJECT_CLASS (gtk_spinner_accessible_parent_class)->initialize (accessible, widget);

  atk_object_set_name (accessible, C_("throbbing progress animation widget", "Spinner"));
  atk_object_set_description (accessible, _("Provides visual indication of progress"));
  atk_object_set_role (accessible, ATK_ROLE_ANIMATION);
}

static void
gtk_spinner_accessible_class_init (GtkSpinnerAccessibleClass *klass)
{
  AtkObjectClass *atk_class = ATK_OBJECT_CLASS (klass);

  atk_class->initialize = gtk_spinner_accessible_initialize;
}

static void
gtk_spinner_accessible_init (GtkSpinnerAccessible *self)
{
}

static void
gtk_spinner_accessible_image_get_size (AtkImage *image,
                                       gint     *width,
                                       gint     *height)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (image));
  if (widget == NULL)
    {
      *width = 0;
      *height = 0;
      return;
    }

  *width = gtk_widget_get_allocated_width (widget);
  *height = gtk_widget_get_allocated_height (widget);
}

static void
atk_image_interface_init (AtkImageIface *iface)
{
  iface->get_image_size = gtk_spinner_accessible_image_get_size;
}
