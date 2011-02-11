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

#include "gtknamedpicture.h"

#include "gtkintl.h"
#include "gtkprivate.h"

struct _GtkNamedPicturePrivate {
  char *name;
};

enum {
  PROP_0,
  PROP_NAME
};

G_DEFINE_TYPE (GtkNamedPicture, gtk_named_picture, GTK_TYPE_ICON_THEME_PICTURE)

/**
 * SECTION:gtknamedpicture
 * @Short_description: Pictures for named icons
 * @Title: GtkNamedPicture
 * @See_also: #GtkIconTheme
 *
 * A #GtkNamedPicture is an implementation of #GdkPicture for named icons.
 */

static void
gtk_named_picture_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GtkNamedPicture *picture = GTK_NAMED_PICTURE (object);
  GtkNamedPicturePrivate *priv = picture->priv;

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, priv->name);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_named_picture_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GtkNamedPicture *picture = GTK_NAMED_PICTURE (object);

  switch (prop_id)
    {
    case PROP_NAME:
      gtk_named_picture_set_name (picture, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_named_picture_dispose (GObject *object)
{
  GtkNamedPicture *named = GTK_NAMED_PICTURE (object);
  GtkNamedPicturePrivate *priv = named->priv;

  g_free (priv->name);
  priv->name = NULL;

  G_OBJECT_CLASS (gtk_named_picture_parent_class)->dispose (object);
}

GtkIconInfo *
gtk_named_picture_lookup (GtkIconThemePicture * picture,
                          GtkIconTheme *        theme,
                          int                   size,
                          GtkIconLookupFlags    flags)
{
  GtkNamedPicture *icon = GTK_NAMED_PICTURE (picture);
  GtkNamedPicturePrivate *priv = icon->priv;

  if (priv->name == NULL)
    return NULL;

  return gtk_icon_theme_lookup_icon (theme,
                                     priv->name,
                                     size,
                                     flags);
}

static void
gtk_named_picture_class_init (GtkNamedPictureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkIconThemePictureClass *icon_theme_picture_class = GTK_ICON_THEME_PICTURE_CLASS (klass);

  object_class->get_property = gtk_named_picture_get_property;
  object_class->set_property = gtk_named_picture_set_property;
  object_class->dispose = gtk_named_picture_dispose;

  icon_theme_picture_class->lookup = gtk_named_picture_lookup;

  /**
   * GtkNamedPicture:name:
   *
   * Name of the icon to display.
   *
   * Since: 3.2
   */
  g_object_class_install_property (object_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        P_("Name"),
                                                        P_("Name of the icon to display"),
                                                        NULL,
                                                        GTK_PARAM_READWRITE));

  g_type_class_add_private (klass, sizeof (GtkNamedPicturePrivate));
}

static void
gtk_named_picture_init (GtkNamedPicture *picture)
{
  picture->priv = G_TYPE_INSTANCE_GET_PRIVATE (picture,
                                               GTK_TYPE_NAMED_PICTURE,
                                               GtkNamedPicturePrivate);
}

/**
 * gtk_named_picture_new:
 * @name: the name of the icon to display
 * @size: The icon size
 *
 * Creates a new #GtkNamedPicture displaying the icon for the
 * given @name.
 *
 * Returns: a new picture
 **/
GdkPicture *
gtk_named_picture_new (const char *name,
                       GtkIconSize size)
{
  g_return_val_if_fail (name != NULL, NULL);

  return g_object_new (GTK_TYPE_NAMED_PICTURE,
                       "name", name,
                       "size", size,
                       NULL);
}

const char *
gtk_named_picture_get_name (GtkNamedPicture * picture)
{
  g_return_val_if_fail (GTK_IS_NAMED_PICTURE (picture), NULL);

  return picture->priv->name;
}

void
gtk_named_picture_set_name (GtkNamedPicture *picture,
                            const char *     name)
{
  GtkNamedPicturePrivate *priv;

  g_return_if_fail (GTK_IS_NAMED_PICTURE (picture));

  priv = picture->priv;

  g_free (priv->name);
  priv->name = g_strdup (name);

  gtk_icon_theme_picture_update (GTK_ICON_THEME_PICTURE (picture));
  g_object_notify (G_OBJECT (picture), "name");
}

