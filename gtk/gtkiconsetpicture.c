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

#include "gtkiconsetpicture.h"

#include <cairo-gobject.h>

#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtkstyledpicture.h"

struct _GtkIconSetPicturePrivate {
  GdkPicture *picture;
  GtkIconSet *set;
  GtkIconSize size;
};

enum {
  PROP_0,
  PROP_ICON_SET,
  PROP_SIZE
};

static void
gtk_icon_set_picture_update_picture (GtkIconSetPicture *picture)
{
  GtkIconSetPicturePrivate *priv = picture->priv;
  GtkIconSet *icon_set;
  GtkStyleContext *style;
  GdkPixbuf *pixbuf;
  GtkWidgetPath *path;
  
  path = gtk_widget_path_new ();
  style = gtk_style_context_new ();
  gtk_style_context_set_path (style, path);
  gtk_widget_path_free (path);

  if (priv->set == NULL)
    {
      icon_set = gtk_style_context_lookup_icon_set (style, GTK_STOCK_MISSING_IMAGE);
      g_assert (icon_set);
    }
  else
    icon_set = priv->set;

  pixbuf = gtk_icon_set_render_icon_pixbuf (icon_set, style, priv->size);
  gdk_pixbuf_picture_set_pixbuf (GDK_PIXBUF_PICTURE (priv->picture), pixbuf);
  g_object_unref (pixbuf);

  g_object_unref (style);
}

static GdkPicture *
gtk_icon_set_picture_update_styled (GtkStyledPicture *styled,
                                    GdkPicture *pixbuf_picture)
{
  GdkPicture *icon = gtk_picture_get_unstyled (GDK_PICTURE (styled));
  GtkWidget *widget = gtk_styled_picture_get_widget (styled);
  GtkIconSetPicturePrivate *priv = GTK_ICON_SET_PICTURE (icon)->priv;
  GtkIconSet *icon_set;
  GtkStyleContext *style;
  GdkPixbuf *pixbuf;

  style = gtk_widget_get_style_context (widget);
  gtk_style_context_save (style);
  gtk_style_context_set_state (style, gtk_widget_get_state_flags (widget));

  if (priv->set == NULL)
    {
      icon_set = gtk_style_context_lookup_icon_set (style, GTK_STOCK_MISSING_IMAGE);
      g_assert (icon_set);
    }
  else
    icon_set = priv->set;

  pixbuf = gtk_icon_set_render_icon_pixbuf (icon_set, style, priv->size);
  gdk_pixbuf_picture_set_pixbuf (GDK_PIXBUF_PICTURE (pixbuf_picture), pixbuf);
  g_object_unref (pixbuf);

  gtk_style_context_restore (style);

  g_object_ref (pixbuf_picture);
  return pixbuf_picture;
}

static GdkPicture *
gtk_icon_set_picture_attach (GdkPicture *picture,
                             GtkWidget  *widget)
{
  GdkPicture *styled;
  GdkPicture *pixbuf_picture;

  styled = gtk_styled_picture_new (picture, widget);
  pixbuf_picture = gdk_pixbuf_picture_new (NULL);
  g_signal_connect (styled, "update", G_CALLBACK (gtk_icon_set_picture_update_styled), pixbuf_picture);
  gtk_styled_picture_update (GTK_STYLED_PICTURE (styled));
  /* We can get rid of it here. styled will have a reference to it
   * and keep that reference until it goes away. */
  g_object_unref (pixbuf_picture);

  return styled;
}

static void
gtk_icon_set_picture_stylable_picture_init (GtkStylablePictureInterface *iface)
{
  iface->attach = gtk_icon_set_picture_attach;
}

G_DEFINE_TYPE_WITH_CODE (GtkIconSetPicture, gtk_icon_set_picture, GDK_TYPE_PICTURE,
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_STYLABLE_PICTURE,
						gtk_icon_set_picture_stylable_picture_init))

/**
 * SECTION:gtkiconsetpicture
 * @Short_description: Pictures for #GtkIconSets
 * @Title: GtkIconSetPicture
 * @See_also: #GtkIconSet
 *
 * A #GtkIconSetPicture is an implementation of #GdkPicture for icon sets.
 */

static void
gtk_icon_set_picture_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GtkIconSetPicture *picture = GTK_ICON_SET_PICTURE (object);
  GtkIconSetPicturePrivate *priv = picture->priv;

  switch (prop_id)
    {
    case PROP_ICON_SET:
      g_value_set_boxed (value, priv->set);
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
gtk_icon_set_picture_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GtkIconSetPicture *picture = GTK_ICON_SET_PICTURE (object);

  switch (prop_id)
    {
    case PROP_ICON_SET:
      gtk_icon_set_picture_set_icon_set (picture, g_value_get_boxed (value));
      break;
    case PROP_SIZE:
      gtk_icon_set_picture_set_size (picture, g_value_get_int (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_icon_set_picture_dispose (GObject *object)
{
  GtkIconSetPicture *icon_set = GTK_ICON_SET_PICTURE (object);
  GtkIconSetPicturePrivate *priv = icon_set->priv;

  if (priv->set) {
    gtk_icon_set_unref (priv->set);
    priv->set = NULL;
  }

  G_OBJECT_CLASS (gtk_icon_set_picture_parent_class)->dispose (object);
}

static cairo_surface_t *
gtk_icon_set_picture_ref_surface (GdkPicture *picture)
{
  GtkIconSetPicture *icon_set = GTK_ICON_SET_PICTURE (picture);

  return gdk_picture_ref_surface (icon_set->priv->picture);
}

static void
gtk_icon_set_picture_draw (GdkPicture *picture,
                           cairo_t    *cr)
{
  GtkIconSetPicture *icon_set = GTK_ICON_SET_PICTURE (picture);

  gdk_picture_draw (icon_set->priv->picture, cr);
}

static void
gtk_icon_set_picture_class_init (GtkIconSetPictureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkPictureClass *picture_class = GDK_PICTURE_CLASS (klass);

  object_class->get_property = gtk_icon_set_picture_get_property;
  object_class->set_property = gtk_icon_set_picture_set_property;
  object_class->dispose = gtk_icon_set_picture_dispose;

  picture_class->ref_surface = gtk_icon_set_picture_ref_surface;
  picture_class->draw = gtk_icon_set_picture_draw;

  /**
   * GtkIconSetPicture:size:
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
   * GtkIconSetPicture:icon-set:
   *
   * The icon set to display.
   *
   * Since: 3.2
   */
  g_object_class_install_property (object_class,
                                   PROP_ICON_SET,
                                   g_param_spec_boxed ("icon-set",
                                                       P_("Icon set"),
                                                       P_("Icon set to display"),
                                                       GTK_TYPE_ICON_SET,
                                                       GTK_PARAM_READWRITE));

  g_type_class_add_private (klass, sizeof (GtkIconSetPicturePrivate));
}

static void
gtk_icon_set_picture_resized_callback (GdkPicture *    pixbuf,
                                   GtkIconSetPicture *icon_set)
{
  gdk_picture_resized (GDK_PICTURE (icon_set),
                       gdk_picture_get_width (pixbuf),
                       gdk_picture_get_height (pixbuf));
}

static void
gtk_icon_set_picture_init (GtkIconSetPicture *picture)
{
  GtkIconSetPicturePrivate *priv;

  picture->priv = G_TYPE_INSTANCE_GET_PRIVATE (picture,
                                               GTK_TYPE_ICON_SET_PICTURE,
                                               GtkIconSetPicturePrivate);

  priv = picture->priv;
  priv->size = GTK_ICON_SIZE_BUTTON;
  priv->picture = gdk_pixbuf_picture_new (NULL);
  g_signal_connect_swapped (priv->picture,
                            "changed",
                            G_CALLBACK (gdk_picture_changed_region),
                            picture);
  g_signal_connect (priv->picture,
                    "resized",
                    G_CALLBACK (gtk_icon_set_picture_resized_callback),
                    picture);

  gtk_icon_set_picture_update_picture (picture);
}

/**
 * gtk_icon_set_picture_new:
 * @icon_set_name: the name of the icon_set to display
 * @size: The icon_set size to use or -1 for default
 *
 * Creates a new #GtkIconSetPicture displaying the given @icon_set.
 *
 * Returns: a new picture
 **/
GdkPicture *
gtk_icon_set_picture_new (GtkIconSet *icon_set,
                          GtkIconSize size)
{
  return g_object_new (GTK_TYPE_ICON_SET_PICTURE,
                       "icon-set", icon_set,
                       "size", size,
                       NULL);
}

GtkIconSize
gtk_icon_set_picture_get_size (GtkIconSetPicture *picture)
{
  g_return_val_if_fail (GTK_IS_ICON_SET_PICTURE (picture), -1);

  return picture->priv->size;
}

void
gtk_icon_set_picture_set_size (GtkIconSetPicture *picture,
                           GtkIconSize     size)
{
  GtkIconSetPicturePrivate *priv;

  g_return_if_fail (GTK_IS_ICON_SET_PICTURE (picture));

  priv = picture->priv;

  if (priv->size == size)
    return;

  priv->size = size;
  
  gtk_icon_set_picture_update_picture (picture);
  g_object_notify (G_OBJECT (picture), "size");
}

GtkIconSet *
gtk_icon_set_picture_get_icon_set (GtkIconSetPicture * picture)
{
  g_return_val_if_fail (GTK_IS_ICON_SET_PICTURE (picture), NULL);

  return picture->priv->set;
}

void
gtk_icon_set_picture_set_icon_set (GtkIconSetPicture *picture,
                                   GtkIconSet *       icon_set)
{
  GtkIconSetPicturePrivate *priv;

  g_return_if_fail (GTK_IS_ICON_SET_PICTURE (picture));

  priv = picture->priv;

  if (icon_set)
    gtk_icon_set_ref (icon_set);

  if (priv->set)
    gtk_icon_set_ref (priv->set);
  
  priv->set = icon_set;

  gtk_icon_set_picture_update_picture (picture);
  g_object_notify (G_OBJECT (picture), "icon-set");
}

