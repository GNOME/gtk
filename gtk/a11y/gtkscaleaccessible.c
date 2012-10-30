/* GTK+ - accessibility implementations
 * Copyright 2004 Sun Microsystems Inc.
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

#include <gtk/gtk.h>
#include "gtkscaleaccessible.h"

G_DEFINE_TYPE (GtkScaleAccessible, gtk_scale_accessible, GTK_TYPE_RANGE_ACCESSIBLE)

static const gchar *
gtk_scale_accessible_get_description (AtkObject *object)
{
  GtkWidget *widget;
  PangoLayout *layout;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (object));
  if (widget == NULL)
    return NULL;

  layout = gtk_scale_get_layout (GTK_SCALE (widget));
  if (layout)
    return pango_layout_get_text (layout);

  return ATK_OBJECT_CLASS (gtk_scale_accessible_parent_class)->get_description (object);
}

static void
gtk_scale_accessible_class_init (GtkScaleAccessibleClass *klass)
{
  AtkObjectClass *class = ATK_OBJECT_CLASS (klass);

  class->get_description = gtk_scale_accessible_get_description;
}

static void
gtk_scale_accessible_init (GtkScaleAccessible *scale)
{
}
