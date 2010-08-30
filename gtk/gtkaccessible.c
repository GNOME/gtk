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

#include "config.h"
#include <string.h>

#include "gtkwidget.h"
#include "gtkintl.h"
#include "gtkaccessible.h"

/**
 * SECTION:gtkaccessible
 * @Short_description: Accessibility support for widgets
 * @Title: GtkAccessible
 */

/*
 * GtkAccessiblePriv:
 * @widget: The GtkWidget whose properties and features are exported via this
 *   accessible instance
 */
struct _GtkAccessiblePrivate
{
  GtkWidget *widget;
};

static void gtk_accessible_real_connect_widget_destroyed (GtkAccessible *accessible);

G_DEFINE_TYPE (GtkAccessible, gtk_accessible, ATK_TYPE_OBJECT)

static void
gtk_accessible_init (GtkAccessible *accesible)
{
  accesible->priv = G_TYPE_INSTANCE_GET_PRIVATE (accesible,
                                                 GTK_TYPE_ACCESSIBLE,
                                                 GtkAccessiblePrivate);
}

static void
gtk_accessible_class_init (GtkAccessibleClass *klass)
{
  klass->connect_widget_destroyed = gtk_accessible_real_connect_widget_destroyed;

  g_type_class_add_private (klass, sizeof (GtkAccessiblePrivate));
}

/**
 * gtk_accessible_set_widget:
 * @accessible: a #GtkAccessible
 * @widget: a #GtkWidget
 *
 * Sets the #GtkWidget corresponding to the #GtkAccessible.
 *
 * Since: 2.22
 **/
void
gtk_accessible_set_widget (GtkAccessible *accessible,
                           GtkWidget     *widget)
{
  g_return_if_fail (GTK_IS_ACCESSIBLE (accessible));

  accessible->priv->widget = widget;
}

/**
 * gtk_accessible_get_widget:
 * @accessible: a #GtkAccessible
 *
 * Gets the #GtkWidget corresponding to the #GtkAccessible. The returned widget
 * does not have a reference added, so you do not need to unref it.
 *
 * Returns: (transfer none): pointer to the #GtkWidget corresponding to
 *   the #GtkAccessible, or %NULL.
 *
 * Since: 2.22
 **/
GtkWidget*
gtk_accessible_get_widget (GtkAccessible *accessible)
{
  g_return_val_if_fail (GTK_IS_ACCESSIBLE (accessible), NULL);

  return accessible->priv->widget;
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
  GtkAccessiblePrivate *priv = accessible->priv;

  if (priv->widget)
  {
    g_signal_connect (priv->widget,
                      "destroy",
                      G_CALLBACK (gtk_widget_destroyed),
                      &priv->widget);
  }
}
