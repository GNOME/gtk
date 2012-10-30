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
#include "gtkimagecellaccessible.h"

struct _GtkImageCellAccessiblePrivate
{
  gchar *image_description;
};

static void atk_image_interface_init (AtkImageIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkImageCellAccessible, gtk_image_cell_accessible, GTK_TYPE_RENDERER_CELL_ACCESSIBLE,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_IMAGE, atk_image_interface_init))

static void
gtk_image_cell_accessible_finalize (GObject *object)
{
  GtkImageCellAccessible *image_cell = GTK_IMAGE_CELL_ACCESSIBLE (object);

  g_free (image_cell->priv->image_description);
  G_OBJECT_CLASS (gtk_image_cell_accessible_parent_class)->finalize (object);
}

static void
gtk_image_cell_accessible_class_init (GtkImageCellAccessibleClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gtk_image_cell_accessible_finalize;

  g_type_class_add_private (klass, sizeof (GtkImageCellAccessiblePrivate));
}

static void
gtk_image_cell_accessible_init (GtkImageCellAccessible *image_cell)
{
  image_cell->priv = G_TYPE_INSTANCE_GET_PRIVATE (image_cell,
                                                  GTK_TYPE_IMAGE_CELL_ACCESSIBLE,
                                                  GtkImageCellAccessiblePrivate);
}

static const gchar *
gtk_image_cell_accessible_get_image_description (AtkImage *image)
{
  GtkImageCellAccessible *image_cell = GTK_IMAGE_CELL_ACCESSIBLE (image);

  return image_cell->priv->image_description;
}

static gboolean
gtk_image_cell_accessible_set_image_description (AtkImage    *image,
                                                 const gchar *description)
{
  GtkImageCellAccessible *image_cell = GTK_IMAGE_CELL_ACCESSIBLE (image);

  g_free (image_cell->priv->image_description);
  image_cell->priv->image_description = g_strdup (description);

  if (image_cell->priv->image_description)
    return TRUE;
  else
    return FALSE;
}

static void
gtk_image_cell_accessible_get_image_position (AtkImage     *image,
                                              gint         *x,
                                              gint         *y,
                                              AtkCoordType  coord_type)
{
  atk_component_get_position (ATK_COMPONENT (image), x, y, coord_type);
}

static void
gtk_image_cell_accessible_get_image_size (AtkImage *image,
                                          gint     *width,
                                          gint     *height)
{
  GtkImageCellAccessible *cell = GTK_IMAGE_CELL_ACCESSIBLE (image);
  GtkCellRenderer *cell_renderer;
  GdkPixbuf *pixbuf = NULL;

  *width = 0;
  *height = 0;

  g_object_get (cell, "renderer", &cell_renderer, NULL);
  g_object_get (cell_renderer,
                "pixbuf", &pixbuf,
                NULL);
  g_object_unref (cell_renderer);

  if (pixbuf)
    {
      *width = gdk_pixbuf_get_width (pixbuf);
      *height = gdk_pixbuf_get_height (pixbuf);
      g_object_unref (pixbuf);
    }
}

static void
atk_image_interface_init (AtkImageIface  *iface)
{
  iface->get_image_description = gtk_image_cell_accessible_get_image_description;
  iface->set_image_description = gtk_image_cell_accessible_set_image_description;
  iface->get_image_position = gtk_image_cell_accessible_get_image_position;
  iface->get_image_size = gtk_image_cell_accessible_get_image_size;
}
