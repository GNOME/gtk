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

#include <string.h>
#include <gtk/gtk.h>
#include "gtkimageaccessible.h"

struct _GtkImageAccessiblePrivate
{
  gchar *image_description;
  gchar *stock_name;
};

static void atk_image_interface_init (AtkImageIface  *iface);

G_DEFINE_TYPE_WITH_CODE (GtkImageAccessible, gtk_image_accessible, GTK_TYPE_WIDGET_ACCESSIBLE,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_IMAGE, atk_image_interface_init))

static void
gtk_image_accessible_initialize (AtkObject *accessible,
                                 gpointer   data)
{
  ATK_OBJECT_CLASS (gtk_image_accessible_parent_class)->initialize (accessible, data);

  accessible->role = ATK_ROLE_ICON;
}

static void
gtk_image_accessible_finalize (GObject *object)
{
  GtkImageAccessible *aimage = GTK_IMAGE_ACCESSIBLE (object);

  g_free (aimage->priv->image_description);
  g_free (aimage->priv->stock_name);

  G_OBJECT_CLASS (gtk_image_accessible_parent_class)->finalize (object);
}

static const gchar *
gtk_image_accessible_get_name (AtkObject *accessible)
{
  GtkWidget* widget;
  GtkImage *image;
  GtkImageAccessible *image_accessible;
  GtkStockItem stock_item;
  gchar *stock_id;
  const gchar *name;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (accessible));
  if (widget == NULL)
    return NULL;

  name = ATK_OBJECT_CLASS (gtk_image_accessible_parent_class)->get_name (accessible);
  if (name)
    return name;

  image = GTK_IMAGE (widget);
  image_accessible = GTK_IMAGE_ACCESSIBLE (accessible);

  g_free (image_accessible->priv->stock_name);
  image_accessible->priv->stock_name = NULL;

  if (gtk_image_get_storage_type (image) != GTK_IMAGE_STOCK)
    return NULL;

  gtk_image_get_stock (image, &stock_id, NULL);
  if (stock_id == NULL)
    return NULL;

  if (!gtk_stock_lookup (stock_id, &stock_item))
    return NULL;

  image_accessible->priv->stock_name = _gtk_toolbar_elide_underscores (stock_item.label);
  return image_accessible->priv->stock_name;
}

static void
gtk_image_accessible_class_init (GtkImageAccessibleClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  AtkObjectClass  *class = ATK_OBJECT_CLASS (klass);

  gobject_class->finalize = gtk_image_accessible_finalize;
  class->initialize = gtk_image_accessible_initialize;
  class->get_name = gtk_image_accessible_get_name;

  g_type_class_add_private (klass, sizeof (GtkImageAccessiblePrivate));
}

static void
gtk_image_accessible_init (GtkImageAccessible *image)
{
  image->priv = G_TYPE_INSTANCE_GET_PRIVATE (image,
                                             GTK_TYPE_IMAGE_ACCESSIBLE,
                                             GtkImageAccessiblePrivate);
}

static const gchar *
gtk_image_accessible_get_image_description (AtkImage *image)
{
  GtkImageAccessible *accessible = GTK_IMAGE_ACCESSIBLE (image);

  return accessible->priv->image_description;
}

static void
gtk_image_accessible_get_image_position (AtkImage     *image,
                                         gint         *x,
                                         gint         *y,
                                         AtkCoordType  coord_type)
{
  atk_component_get_position (ATK_COMPONENT (image), x, y, coord_type);
}

static void
gtk_image_accessible_get_image_size (AtkImage *image,
                                     gint     *width,
                                     gint     *height)
{
  GtkWidget* widget;
  GtkImage *gtk_image;
  GtkImageType image_type;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (image));
  if (widget == NULL)
    {
      *height = -1;
      *width = -1;
      return;
    }

  gtk_image = GTK_IMAGE (widget);

  image_type = gtk_image_get_storage_type (gtk_image);
  switch (image_type)
    {
    case GTK_IMAGE_PIXBUF:
      {
        GdkPixbuf *pixbuf;

        pixbuf = gtk_image_get_pixbuf (gtk_image);
        *height = gdk_pixbuf_get_height (pixbuf);
        *width = gdk_pixbuf_get_width (pixbuf);
        break;
      }
    case GTK_IMAGE_STOCK:
    case GTK_IMAGE_ICON_SET:
    case GTK_IMAGE_ICON_NAME:
    case GTK_IMAGE_GICON:
      {
        GtkIconSize size;
        GtkSettings *settings;

        settings = gtk_settings_get_for_screen (gtk_widget_get_screen (widget));

        g_object_get (gtk_image, "icon-size", &size, NULL);
        gtk_icon_size_lookup_for_settings (settings, size, width, height);
        break;
      }
    case GTK_IMAGE_ANIMATION:
      {
        GdkPixbufAnimation *animation;

        animation = gtk_image_get_animation (gtk_image);
        *height = gdk_pixbuf_animation_get_height (animation);
        *width = gdk_pixbuf_animation_get_width (animation);
        break;
      }
    default:
      {
        *height = -1;
        *width = -1;
        break;
      }
    }
}

static gboolean
gtk_image_accessible_set_image_description (AtkImage    *image,
                                            const gchar *description)
{
  GtkImageAccessible* accessible = GTK_IMAGE_ACCESSIBLE (image);

  g_free (accessible->priv->image_description);
  accessible->priv->image_description = g_strdup (description);

  return TRUE;
}

static void
atk_image_interface_init (AtkImageIface *iface)
{
  iface->get_image_description = gtk_image_accessible_get_image_description;
  iface->get_image_position = gtk_image_accessible_get_image_position;
  iface->get_image_size = gtk_image_accessible_get_image_size;
  iface->set_image_description = gtk_image_accessible_set_image_description;
}
