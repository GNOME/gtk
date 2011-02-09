/* GTK - The GIMP Drawing Kit
 * Copyright (C) 2010 Benjamin Otte <otte@gnome.org>
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

#include "gtkstockpicture.h"

#include <cairo-gobject.h>

#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkstyledpicture.h"

struct _GtkStockPicturePrivate {
  GdkPicture *picture;
  char *stock_id;
  GtkIconSize size;
};

enum {
  PROP_0,
  PROP_STOCK,
  PROP_SIZE
};

static void
gtk_stock_picture_update_picture (GtkStockPicture *picture)
{
  GtkStockPicturePrivate *priv = picture->priv;
  GtkIconSet *icon_set;
  GtkStyleContext *style;
  GdkPixbuf *pixbuf;
  GtkWidgetPath *path;

  path = gtk_widget_path_new ();
  style = gtk_style_context_new ();
  gtk_style_context_set_path (style, path);
  gtk_widget_path_free (path);

  if (priv->stock_id)
    icon_set = gtk_style_context_lookup_icon_set (style, priv->stock_id);
  else
    icon_set = NULL;

  if (icon_set == NULL)
    {
      icon_set = gtk_style_context_lookup_icon_set (style, GTK_STOCK_MISSING_IMAGE);
      g_assert (icon_set);
    }

  pixbuf = gtk_icon_set_render_icon_pixbuf (icon_set, style, priv->size);
  gdk_pixbuf_picture_set_pixbuf (GDK_PIXBUF_PICTURE (priv->picture), pixbuf);
  g_object_unref (pixbuf);

  g_object_unref (style);
}

static GdkPicture *
gtk_stock_picture_update_styled (GtkStyledPicture *styled,
                                 GdkPicture       *pixbuf_picture)
{
  GdkPicture *stock = gtk_picture_get_unstyled (GDK_PICTURE (styled));
  GtkWidget *widget = gtk_styled_picture_get_widget (styled);
  GtkStockPicturePrivate *priv = GTK_STOCK_PICTURE (stock)->priv;
  GdkPixbuf *pixbuf;

  if (priv->stock_id)
    pixbuf = gtk_widget_render_icon_pixbuf (widget, 
                                            priv->stock_id,
                                            priv->size);
  else
    pixbuf = NULL;

  if (pixbuf == NULL)
    {
      pixbuf = gtk_widget_render_icon_pixbuf (widget, 
                                              GTK_STOCK_MISSING_IMAGE,
                                              priv->size);
      g_assert (pixbuf != NULL);
    }

  gdk_pixbuf_picture_set_pixbuf (GDK_PIXBUF_PICTURE (pixbuf_picture), pixbuf);
  g_object_unref (pixbuf);

  g_object_ref (pixbuf_picture);
  return pixbuf_picture;
}

static GdkPicture *
gtk_stock_picture_attach (GdkPicture *picture,
                          GtkWidget  *widget)
{
  GdkPicture *styled;
  GdkPicture *pixbuf_picture;

  styled = gtk_styled_picture_new (picture, widget);
  pixbuf_picture = gdk_pixbuf_picture_new (NULL);
  g_signal_connect (styled, "update", G_CALLBACK (gtk_stock_picture_update_styled), pixbuf_picture);
  gtk_styled_picture_update (GTK_STYLED_PICTURE (styled));
  /* We can get rid of it here. styled will have a reference to it
   * and keep that reference until it goes away. */
  g_object_unref (pixbuf_picture);

  return styled;
}

static void
gtk_stock_picture_stylable_picture_init (GtkStylablePictureInterface *iface)
{
  iface->attach = gtk_stock_picture_attach;
}

G_DEFINE_TYPE_WITH_CODE (GtkStockPicture, gtk_stock_picture, GDK_TYPE_PICTURE,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_STYLABLE_PICTURE,
						gtk_stock_picture_stylable_picture_init))

/**
 * SECTION:gtkstockpicture
 * @Short_description: Pictures for stock icons
 * @Title: GtkStockPicture
 * @See_also: #GtkIconFactory
 *
 * A #GtkStockPicture is an implementation of #GdkPicture for stock icons.
 */

static void
gtk_stock_picture_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GtkStockPicture *picture = GTK_STOCK_PICTURE (object);
  GtkStockPicturePrivate *priv = picture->priv;

  switch (prop_id)
    {
    case PROP_STOCK:
      g_value_set_string (value, priv->stock_id);
      break;
    case PROP_SIZE:
      g_value_set_int (value, priv->size);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_stock_picture_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  GtkStockPicture *picture = GTK_STOCK_PICTURE (object);

  switch (prop_id)
    {
    case PROP_STOCK:
      gtk_stock_picture_set_stock_id (picture, g_value_get_string (value));
      break;
    case PROP_SIZE:
      gtk_stock_picture_set_size (picture, g_value_get_int (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_stock_picture_dispose (GObject *object)
{
  GtkStockPicture *icon = GTK_STOCK_PICTURE (object);
  GtkStockPicturePrivate *priv = icon->priv;

  if (priv->picture)
    {
      g_object_unref (priv->picture);
      priv->picture = NULL;
    }
  g_free (priv->stock_id);
  priv->stock_id = NULL;

  G_OBJECT_CLASS (gtk_stock_picture_parent_class)->dispose (object);
}

static cairo_surface_t *
gtk_stock_picture_ref_surface (GdkPicture *picture)
{
  GtkStockPicture *icon = GTK_STOCK_PICTURE (picture);

  return gdk_picture_ref_surface (icon->priv->picture);
}

static void
gtk_stock_picture_draw (GdkPicture *picture,
                       cairo_t    *cr)
{
  GtkStockPicture *icon = GTK_STOCK_PICTURE (picture);

  gdk_picture_draw (icon->priv->picture, cr);
}

static void
gtk_stock_picture_class_init (GtkStockPictureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkPictureClass *picture_class = GDK_PICTURE_CLASS (klass);

  object_class->get_property = gtk_stock_picture_get_property;
  object_class->set_property = gtk_stock_picture_set_property;
  object_class->dispose = gtk_stock_picture_dispose;

  picture_class->ref_surface = gtk_stock_picture_ref_surface;
  picture_class->draw = gtk_stock_picture_draw;

  /**
   * GtkStockPicture:size:
   *
   * The #GtkIconSize to use to determine the actual image size.
   *
   * Since: 3.2
   */
  g_object_class_install_property (object_class,
                                   PROP_SIZE,
                                   g_param_spec_int ("size",
                                                     P_("Size"),
                                                     P_("Symbolic size to use"),
                                                     G_MININT, G_MAXINT,
                                                     GTK_ICON_SIZE_BUTTON,
                                                     GTK_PARAM_READWRITE));

  /**
   * GtkStockPicture:stock:
   *
   * The stock id of the icon to display.
   *
   * Since: 3.2
   */
  g_object_class_install_property (object_class,
                                   PROP_STOCK,
                                   g_param_spec_string ("stock",
                                                        P_("Name"),
                                                        P_("The stock id of the icon from the icon theme"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  g_type_class_add_private (klass, sizeof (GtkStockPicturePrivate));
}

static void
gtk_stock_picture_resized_callback (GdkPicture *    pixbuf,
                                   GtkStockPicture *icon)
{
  gdk_picture_resized (GDK_PICTURE (icon),
                       gdk_picture_get_width (pixbuf),
                       gdk_picture_get_height (pixbuf));
}

static void
gtk_stock_picture_init (GtkStockPicture *picture)
{
  GtkStockPicturePrivate *priv;

  picture->priv = G_TYPE_INSTANCE_GET_PRIVATE (picture,
                                               GTK_TYPE_STOCK_PICTURE,
                                               GtkStockPicturePrivate);

  priv = picture->priv;
  priv->size = GTK_ICON_SIZE_BUTTON;
  priv->picture = gdk_pixbuf_picture_new (NULL);
  g_signal_connect_swapped (priv->picture,
                            "changed",
                            G_CALLBACK (gdk_picture_changed_region),
                            picture);
  g_signal_connect (priv->picture,
                    "resized",
                    G_CALLBACK (gtk_stock_picture_resized_callback),
                    picture);

  gtk_stock_picture_update_picture (picture);
}

/**
 * gtk_stock_picture_new:
 * @stock_id: the stock_id of the icon to display
 * @size: The icon size to use or -1 for default
 *
 * Creates a new #GtkStockPicture displaying the given @icon.
 *
 * Returns: a new picture
 **/
GdkPicture *
gtk_stock_picture_new (const char *stock_id,
                       GtkIconSize size)
{
  return g_object_new (GTK_TYPE_STOCK_PICTURE,
                       "stock", stock_id,
                       "size", size,
                       NULL);
}

GtkIconSize
gtk_stock_picture_get_size (GtkStockPicture *picture)
{
  g_return_val_if_fail (GTK_IS_STOCK_PICTURE (picture), -1);

  return picture->priv->size;
}

void
gtk_stock_picture_set_size (GtkStockPicture *picture,
                            GtkIconSize     size)
{
  GtkStockPicturePrivate *priv;

  g_return_if_fail (GTK_IS_STOCK_PICTURE (picture));

  priv = picture->priv;

  if (priv->size == size)
    return;

  priv->size = size;
  
  gtk_stock_picture_update_picture (picture);
  g_object_notify (G_OBJECT (picture), "size");
}

const char *
gtk_stock_picture_get_stock_id (GtkStockPicture * picture)
{
  g_return_val_if_fail (GTK_IS_STOCK_PICTURE (picture), NULL);

  return picture->priv->stock_id;
}

void
gtk_stock_picture_set_stock_id (GtkStockPicture *picture,
                                const char *     stock_id)
{
  GtkStockPicturePrivate *priv;

  g_return_if_fail (GTK_IS_STOCK_PICTURE (picture));

  priv = picture->priv;

  g_free (priv->stock_id);
  priv->stock_id = g_strdup (stock_id);

  gtk_stock_picture_update_picture (picture);
  g_object_notify (G_OBJECT (picture), "stock");
}

