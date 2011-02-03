/* GDK - The GIMP Drawing Kit
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

#include "gdkpixbufpicture.h"

#include <cairo-gobject.h>

#include "gdkintl.h"
#include "gdkinternals.h"

struct _GdkPixbufPicturePrivate {
  GdkPixbuf *pixbuf;
  cairo_surface_t *surface;
  guint keep_pixbuf : 1;
  guint keep_surface : 1;
};

G_DEFINE_TYPE (GdkPixbufPicture, gdk_pixbuf_picture, GDK_TYPE_PICTURE)

enum {
  PROP_0,
  PROP_PIXBUF,
  PROP_KEEP_PIXBUF,
  PROP_KEEP_SURFACE
};

/**
 * SECTION:gdkpixbufpicture
 * @Short_description: Pictures for pixbufs
 * @Title: GdkPixbufPicture
 * @See_also: #GdkPicture
 *
 * A #GdkPixbufPicture is an implementation of #GdkPicture for a #GdkPixbuf.
 * It is meant to help in the porting of applications from #GdkPixbuf use
 * to #GdkPicture use.
 *
 * You should try not to use it for newly written code.
 */

static void
gdk_pixbuf_picture_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GdkPixbufPicture *picture = GDK_PIXBUF_PICTURE (object);
  GdkPixbufPicturePrivate *priv = picture->priv;

  switch (prop_id)
    {
    case PROP_PIXBUF:
      g_value_set_object (value, gdk_pixbuf_picture_get_pixbuf (picture));
      break;
    case PROP_KEEP_PIXBUF:
      g_value_set_boolean (value, priv->keep_pixbuf);
      break;
    case PROP_KEEP_SURFACE:
      g_value_set_boolean (value, priv->keep_surface);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_pixbuf_picture_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GdkPixbufPicture *picture = GDK_PIXBUF_PICTURE (object);

  switch (prop_id)
    {
    case PROP_PIXBUF:
      gdk_pixbuf_picture_set_pixbuf (picture, g_value_get_object (value));
      break;
    case PROP_KEEP_PIXBUF:
      gdk_pixbuf_picture_set_keep_pixbuf (picture, g_value_get_boolean (value));
      break;
    case PROP_KEEP_SURFACE:
      gdk_pixbuf_picture_set_keep_surface (picture, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_pixbuf_picture_dispose (GObject *object)
{
  GdkPixbufPicture *picture = GDK_PIXBUF_PICTURE (object);

  gdk_pixbuf_picture_set_pixbuf (picture, NULL);

  G_OBJECT_CLASS (gdk_pixbuf_picture_parent_class)->dispose (object);
}

static cairo_surface_t *
gdk_pixbuf_picture_ref_surface (GdkPicture *picture)
{
  GdkPixbufPicture *pixbuf_picture = GDK_PIXBUF_PICTURE (picture);
  GdkPixbufPicturePrivate *priv = pixbuf_picture->priv;
  cairo_surface_t *surface;

  if (priv->surface)
    return cairo_surface_reference (priv->surface);

  if (priv->pixbuf)
    surface = _gdk_cairo_create_surface_for_pixbuf (priv->pixbuf);
  else
    surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 0, 0);

  if (priv->keep_surface)
    priv->surface = cairo_surface_reference (surface);

  return surface;
}

static void
gdk_pixbuf_picture_class_init (GdkPixbufPictureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkPictureClass *picture_class = GDK_PICTURE_CLASS (klass);

  object_class->get_property = gdk_pixbuf_picture_get_property;
  object_class->set_property = gdk_pixbuf_picture_set_property;
  object_class->dispose = gdk_pixbuf_picture_dispose;

  picture_class->ref_surface = gdk_pixbuf_picture_ref_surface;

  /**
   * GdkPixbufPicture:pixbuf:
   *
   * The pixbuf to be drawn or %NULL if none was set or GdkPixbufPicture:keep-pixbuf
   * is %FALSE.
   *
   * Since: 3.2
   */
  g_object_class_install_property (object_class,
                                   PROP_PIXBUF,
                                   g_param_spec_object ("pixbuf",
                                                        P_("pixbuf"),
                                                        P_("the pixbuf to display"),
                                                        GDK_TYPE_PIXBUF,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GdkPixbufPicture:keep-pixbuf:
   *
   * Whether to cache the pixbuf internally or to convert it into a #cairo_surface_t immediately.
   *
   * Since: 3.2
   */
  g_object_class_install_property (object_class,
                                   PROP_KEEP_PIXBUF,
                                   g_param_spec_boolean ("keep-pixbuf",
                                                         P_("keep pixbuf"),
                                                         P_("TRUE to keep the pixbuf around"),
                                                         TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GdkPixbufPicture:keep-surface:
   *
   * Whether to keep the #cairo_surface_t internally that was created when drawing the picture.
   *
   * Since: 3.2
   */
  g_object_class_install_property (object_class,
                                   PROP_KEEP_SURFACE,
                                   g_param_spec_boolean ("keep-surface",
                                                         P_("keep surface"),
                                                         P_("TRUE to keep the surface around"),
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (klass, sizeof (GdkPixbufPicturePrivate));
}

static void
gdk_pixbuf_picture_init (GdkPixbufPicture *picture)
{
  GdkPixbufPicturePrivate *priv;

  picture->priv = G_TYPE_INSTANCE_GET_PRIVATE (picture,
                                               GDK_TYPE_PIXBUF_PICTURE,
                                               GdkPixbufPicturePrivate);
  priv = picture->priv;

  priv->keep_pixbuf = TRUE;
}

/**
 * gdk_pixbuf_picture_new:
 * @pixbuf: the pixbuf to create the picture with or %NULL
 *
 * Creates a new #GdkPixbufPicture displaying the given @pixbuf.
 *
 * Returns: a new picture
 **/
GdkPicture *
gdk_pixbuf_picture_new (GdkPixbuf *pixbuf)
{
  g_return_val_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf), NULL);

  return g_object_new (GDK_TYPE_PIXBUF_PICTURE, "pixbuf", pixbuf, NULL);
}

void
gdk_pixbuf_picture_set_pixbuf (GdkPixbufPicture *picture,
                               GdkPixbuf *       pixbuf)
{
  GdkPixbufPicturePrivate *priv;

  g_return_if_fail (GDK_IS_PIXBUF_PICTURE (picture));
  g_return_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf));

  priv = picture->priv;

  if (pixbuf)
    g_object_ref (pixbuf);

  if (priv->surface)
    {
      cairo_surface_destroy (priv->surface);
      priv->surface = NULL;
    }
  if (priv->pixbuf)
    {
      g_object_unref (priv->pixbuf);
      priv->pixbuf = NULL;
    }

  if (priv->keep_pixbuf)
    {
      priv->pixbuf = pixbuf;
    }
  else
    {
      if (pixbuf)
        priv->surface = _gdk_cairo_create_surface_for_pixbuf (pixbuf);
    }

  g_object_notify (G_OBJECT (picture), "pixbuf");

  if (pixbuf)
    {
      gdk_picture_resized (GDK_PICTURE (picture),
                           gdk_pixbuf_get_width (pixbuf),
                           gdk_pixbuf_get_height (pixbuf));
    }
  else
    gdk_picture_resized (GDK_PICTURE (picture), 0, 0);
}

GdkPixbuf *
gdk_pixbuf_picture_get_pixbuf (GdkPixbufPicture *picture)
{
  g_return_val_if_fail (GDK_IS_PIXBUF_PICTURE (picture), NULL);

  return picture->priv->pixbuf;
}

void
gdk_pixbuf_picture_set_keep_pixbuf (GdkPixbufPicture *picture,
                                    gboolean          keep_pixbuf)
{
  GdkPixbufPicturePrivate *priv;

  g_return_if_fail (GDK_IS_PIXBUF_PICTURE (picture));

  priv = picture->priv;

  if (priv->keep_pixbuf == keep_pixbuf)
    return;

  priv->keep_pixbuf = keep_pixbuf;
  if (keep_pixbuf)
    {
      /* This is equal to setting the pixbuf to NULL because
       * there's no pixbuf we could keep now. */
      gdk_pixbuf_picture_set_pixbuf (picture, NULL);
    }
  else
    {
      gdk_pixbuf_picture_set_keep_surface (picture, TRUE);

      if (priv->pixbuf)
        {
          priv->surface = _gdk_cairo_create_surface_for_pixbuf (priv->pixbuf);
          g_object_unref (priv->pixbuf);
          priv->pixbuf = NULL;
        }
    }

  g_object_notify (G_OBJECT (picture), "keep-pixbuf");
}

gboolean
gdk_pixbuf_picture_get_keep_pixbuf (GdkPixbufPicture *picture)
{
  g_return_val_if_fail (GDK_IS_PIXBUF_PICTURE (picture), FALSE);

  return picture->priv->keep_pixbuf;
}

void
gdk_pixbuf_picture_set_keep_surface (GdkPixbufPicture *picture,
                                     gboolean          keep_surface)
{
  GdkPixbufPicturePrivate *priv;

  g_return_if_fail (GDK_IS_PIXBUF_PICTURE (picture));

  priv = picture->priv;

  if (priv->keep_surface == keep_surface)
    return;

  priv->keep_surface = keep_surface;
  if (!keep_surface)
    {
      if (priv->surface)
        {
          cairo_surface_destroy (priv->surface);
          priv->surface = NULL;
        }

      gdk_pixbuf_picture_set_keep_pixbuf (picture, TRUE);
    }

  g_object_notify (G_OBJECT (picture), "keep-surface");
}

gboolean
gdk_pixbuf_picture_get_keep_surface (GdkPixbufPicture *picture)
{
  g_return_val_if_fail (GDK_IS_PIXBUF_PICTURE (picture), FALSE);

  return picture->priv->keep_surface;
}

