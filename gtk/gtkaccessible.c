/* GTK - The GIMP Toolkit
 * Copyright 2001 Sun Microsystems Inc.
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
#include <gtk/gtkwidget.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkaccessible.h>

static void gtk_accessible_class_init (GtkAccessibleClass *klass);

static void gtk_accessible_real_connect_widget_destroyed (GtkAccessible *accessible);

GtkType
gtk_accessible_get_type (void)
{
  static GtkType accessible_type = 0;

  if (!accessible_type)
    {
      static const GTypeInfo accessible_info =
      {
	sizeof (GtkAccessibleClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
	(GClassInitFunc) gtk_accessible_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	sizeof (GtkAccessible),
	16,		/* n_preallocs */
	(GInstanceInitFunc) NULL,
      };

      accessible_type = g_type_register_static (ATK_TYPE_OBJECT, "GtkAccessible", &accessible_info, 0);
    }

  return accessible_type;
}

static void
gtk_accessible_class_init (GtkAccessibleClass *klass)
{
  klass->connect_widget_destroyed = gtk_accessible_real_connect_widget_destroyed;

}

/**
 * gtk_accessible_connect_widget_destroyed
 * @accessible: a #GtkAccessible
 *
 * This function specifies the callback function to be called when the widget
 * corresponding to a GtkAccessible is destroyed.
 */
void
gtk_accessible_connect_widget_destroyed (GtkAccessible *accessible)
{
  GtkAccessibleClass *class;

  g_return_if_fail (GTK_IS_ACCESSIBLE (accessible));

  class = GTK_ACCESSIBLE_GET_CLASS (accessible);

  if (class->connect_widget_destroyed)
    class->connect_widget_destroyed (accessible);
}

static void
gtk_accessible_real_connect_widget_destroyed (GtkAccessible *accessible)
{
  if (accessible->widget)
  {
    gtk_signal_connect (GTK_OBJECT (accessible->widget),
                        "destroy",
                        GTK_SIGNAL_FUNC (gtk_widget_destroyed),
                        &accessible->widget);
  }
}
