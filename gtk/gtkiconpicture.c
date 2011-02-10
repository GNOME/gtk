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

#include "gtkiconpicture.h"

#include "gtkintl.h"
#include "gtkprivate.h"

struct _GtkIconPicturePrivate {
  GIcon *icon;
};

enum {
  PROP_0,
  PROP_ICON
};

G_DEFINE_TYPE (GtkIconPicture, gtk_icon_picture, GTK_TYPE_ICON_THEME_PICTURE)

/**
 * SECTION:gtkiconsetpicture
 * @Short_description: Pictures for #GtkIcons
 * @Title: GtkIconPicture
 * @See_also: #GtkIcon
 *
 * A #GtkIconPicture is an implementation of #GdkPicture for icon sets.
 */

static void
gtk_icon_picture_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GtkIconPicture *picture = GTK_ICON_PICTURE (object);
  GtkIconPicturePrivate *priv = picture->priv;

  switch (prop_id)
    {
    case PROP_ICON:
      g_value_set_object (value, priv->icon);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_icon_picture_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GtkIconPicture *picture = GTK_ICON_PICTURE (object);

  switch (prop_id)
    {
    case PROP_ICON:
      gtk_icon_picture_set_icon (picture, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_icon_picture_dispose (GObject *object)
{
  GtkIconPicture *icon = GTK_ICON_PICTURE (object);
  GtkIconPicturePrivate *priv = icon->priv;

  if (priv->icon) {
    g_object_unref (priv->icon);
    priv->icon = NULL;
  }

  G_OBJECT_CLASS (gtk_icon_picture_parent_class)->dispose (object);
}

GtkIconInfo *
gtk_icon_picture_lookup (GtkIconThemePicture * picture,
                         GtkIconTheme *        theme,
                         int                   size,
                         GtkIconLookupFlags    flags)
{
  GtkIconPicture *icon = GTK_ICON_PICTURE (picture);
  GtkIconPicturePrivate *priv = icon->priv;

  if (priv->icon == NULL)
    return NULL;

  return gtk_icon_theme_lookup_by_gicon (theme,
                                         priv->icon,
                                         size,
                                         flags);
}

static void
gtk_icon_picture_class_init (GtkIconPictureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkIconThemePictureClass *icon_theme_picture_class = GTK_ICON_THEME_PICTURE_CLASS (klass);

  object_class->get_property = gtk_icon_picture_get_property;
  object_class->set_property = gtk_icon_picture_set_property;
  object_class->dispose = gtk_icon_picture_dispose;

  icon_theme_picture_class->lookup = gtk_icon_picture_lookup;

  /**
   * GtkIconPicture:icon:
   *
   * The icon to display.
   *
   * Since: 3.2
   */
  g_object_class_install_property (object_class,
                                   PROP_ICON,
                                   g_param_spec_object ("icon",
                                                        P_("Icon"),
                                                        P_("Icon to display"),
                                                        G_TYPE_ICON,
                                                        GTK_PARAM_READWRITE));

  g_type_class_add_private (klass, sizeof (GtkIconPicturePrivate));
}

static void
gtk_icon_picture_init (GtkIconPicture *picture)
{
  picture->priv = G_TYPE_INSTANCE_GET_PRIVATE (picture,
                                               GTK_TYPE_ICON_PICTURE,
                                               GtkIconPicturePrivate);
}

/**
 * gtk_icon_picture_new:
 * @icon_name: the name of the icon to display
 * @size: The icon size to use or -1 for default
 *
 * Creates a new #GtkIconPicture displaying the given @icon.
 *
 * Returns: a new picture
 **/
GdkPicture *
gtk_icon_picture_new (GIcon *     icon,
                      GtkIconSize size)
{
  return g_object_new (GTK_TYPE_ICON_PICTURE,
                       "icon", icon,
                       "size", size,
                       NULL);
}

GIcon *
gtk_icon_picture_get_icon (GtkIconPicture * picture)
{
  g_return_val_if_fail (GTK_IS_ICON_PICTURE (picture), NULL);

  return picture->priv->icon;
}

void
gtk_icon_picture_set_icon (GtkIconPicture *picture,
                           GIcon *         icon)
{
  GtkIconPicturePrivate *priv;

  g_return_if_fail (GTK_IS_ICON_PICTURE (picture));

  priv = picture->priv;

  if (icon == priv->icon)
    return;

  if (icon)
    g_object_ref (icon);

  if (priv->icon)
    g_object_unref (priv->icon);
  
  priv->icon = icon;

  gtk_icon_theme_picture_update (GTK_ICON_THEME_PICTURE (picture));
  g_object_notify (G_OBJECT (picture), "icon");
}

