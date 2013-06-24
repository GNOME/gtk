/* GTK+ - accessibility implementations
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <gtk/gtk.h>
#include "gtkarrowaccessible.h"

struct _GtkArrowAccessiblePrivate
{
  gchar *image_description;
};

static void atk_image_interface_init (AtkImageIface  *iface);

G_DEFINE_TYPE_WITH_CODE (GtkArrowAccessible, gtk_arrow_accessible, GTK_TYPE_WIDGET_ACCESSIBLE,
                         G_ADD_PRIVATE (GtkArrowAccessible)
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_IMAGE, atk_image_interface_init))

static void
gtk_arrow_accessible_initialize (AtkObject *accessible,
                                 gpointer   data)
{
  ATK_OBJECT_CLASS (gtk_arrow_accessible_parent_class)->initialize (accessible, data);

  accessible->role = ATK_ROLE_ICON;
}

static void
gtk_arrow_accessible_finalize (GObject *object)
{
  GtkArrowAccessible *arrow = GTK_ARROW_ACCESSIBLE (object);

  g_free (arrow->priv->image_description);

  G_OBJECT_CLASS (gtk_arrow_accessible_parent_class)->finalize (object);
}

static void
gtk_arrow_accessible_class_init (GtkArrowAccessibleClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass *atk_object_class = ATK_OBJECT_CLASS (klass);

  atk_object_class->initialize = gtk_arrow_accessible_initialize;

  gobject_class->finalize = gtk_arrow_accessible_finalize;
}

static void
gtk_arrow_accessible_init (GtkArrowAccessible *arrow)
{
  arrow->priv = gtk_arrow_accessible_get_instance_private (arrow);
}

static const gchar *
gtk_arrow_accessible_get_image_description (AtkImage *obj)
{
  GtkArrowAccessible *arrow = GTK_ARROW_ACCESSIBLE (obj);

  return arrow->priv->image_description;
}

static gboolean
gtk_arrow_accessible_set_image_description (AtkImage    *obj,
                                            const gchar *description)
{
  GtkArrowAccessible *arrow = GTK_ARROW_ACCESSIBLE (obj);

  g_free (arrow->priv->image_description);
  arrow->priv->image_description = g_strdup (description);

  return TRUE;

}

static void
atk_image_interface_init (AtkImageIface *iface)
{
  iface->get_image_description = gtk_arrow_accessible_get_image_description;
  iface->set_image_description = gtk_arrow_accessible_set_image_description;
}
