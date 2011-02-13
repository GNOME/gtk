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

#include "gdkpixbufanimationpicture.h"

#include <cairo-gobject.h>

#include "gdkintl.h"
#include "gdkinternals.h"

struct _GdkPixbufAnimationPicturePrivate {
  GdkPixbufAnimation *animation;
  GdkPixbufAnimationIter *iter;
  guint timeout_id;
};

G_DEFINE_TYPE (GdkPixbufAnimationPicture, gdk_pixbuf_animation_picture, GDK_TYPE_PICTURE)

enum {
  PROP_0,
  PROP_ANIMATION,
};

/**
 * SECTION:gdkpixbufanimationpicture
 * @Short_description: Pictures for pixbuf animations
 * @Title: GdkPixbufAnimationPicture
 * @See_also: #GdkPicture, #GdkPixbufAnimation
 *
 * A #GdkPixbufAnimationPicture is an implementation of #GdkPicture for a
 * #GdkPixbufAnimation. It is meant to help in the porting of applications
 * from #GdkPixbuf use to #GdkPicture use.
 *
 * You should try not to use it for newly written code.
 */

static void
gdk_pixbuf_animation_picture_get_property (GObject    *object,
                                           guint       prop_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
  GdkPixbufAnimationPicture *picture = GDK_PIXBUF_ANIMATION_PICTURE (object);
  GdkPixbufAnimationPicturePrivate *priv = picture->priv;

  switch (prop_id)
    {
    case PROP_ANIMATION:
      g_value_set_object (value, priv->animation);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_pixbuf_animation_picture_set_property (GObject      *object,
                                           guint         prop_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
  GdkPixbufAnimationPicture *picture = GDK_PIXBUF_ANIMATION_PICTURE (object);

  switch (prop_id)
    {
    case PROP_ANIMATION:
      gdk_pixbuf_animation_picture_set_animation (picture, g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_pixbuf_animation_picture_dispose (GObject *object)
{
  GdkPixbufAnimationPicture *picture = GDK_PIXBUF_ANIMATION_PICTURE (object);

  gdk_pixbuf_animation_picture_set_animation (picture, NULL);

  G_OBJECT_CLASS (gdk_pixbuf_animation_picture_parent_class)->dispose (object);
}

static cairo_surface_t *
gdk_pixbuf_animation_picture_ref_surface (GdkPicture *picture)
{
  GdkPixbufAnimationPicture *animation = GDK_PIXBUF_ANIMATION_PICTURE (picture);
  GdkPixbufAnimationPicturePrivate *priv = animation->priv;

  if (!priv->animation)
    return cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 0, 0);

  if (!priv->iter)
    return _gdk_cairo_create_surface_for_pixbuf (gdk_pixbuf_animation_get_static_image (priv->animation));

  return _gdk_cairo_create_surface_for_pixbuf (gdk_pixbuf_animation_iter_get_pixbuf (priv->iter));
}

static void
gdk_pixbuf_animation_picture_class_init (GdkPixbufAnimationPictureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkPictureClass *picture_class = GDK_PICTURE_CLASS (klass);

  object_class->get_property = gdk_pixbuf_animation_picture_get_property;
  object_class->set_property = gdk_pixbuf_animation_picture_set_property;
  object_class->dispose = gdk_pixbuf_animation_picture_dispose;

  picture_class->ref_surface = gdk_pixbuf_animation_picture_ref_surface;

  /**
   * GdkPixbufAnimationPicture:animation:
   *
   * The animation to be drawn or %NULL if none was set.
   *
   * Since: 3.2
   */
  g_object_class_install_property (object_class,
                                   PROP_ANIMATION,
                                   g_param_spec_object ("animation",
                                                        P_("Animation"),
                                                        P_("the pixbuf animation to display"),
                                                        GDK_TYPE_PIXBUF_ANIMATION,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (klass, sizeof (GdkPixbufAnimationPicturePrivate));
}

static void
gdk_pixbuf_animation_picture_init (GdkPixbufAnimationPicture *picture)
{
  GdkPixbufAnimationPicturePrivate *priv;

  picture->priv = G_TYPE_INSTANCE_GET_PRIVATE (picture,
                                               GDK_TYPE_PIXBUF_ANIMATION_PICTURE,
                                               GdkPixbufAnimationPicturePrivate);
  priv = picture->priv;
}

/**
 * gdk_pixbuf_animation_picture_new:
 * @animation: the pixbuf animation to create the picture with or %NULL
 *
 * Creates a new #GdkPixbufAnimationPicture displaying the given @pixbuf_animation.
 *
 * Returns: a new picture
 **/
GdkPicture *
gdk_pixbuf_animation_picture_new (GdkPixbufAnimation *animation)
{
  g_return_val_if_fail (animation == NULL || GDK_IS_PIXBUF_ANIMATION (animation), NULL);

  return g_object_new (GDK_TYPE_PIXBUF_ANIMATION_PICTURE, "animation", animation, NULL);
}

static void
gdk_pixbuf_animation_iter_remove_timeout (GdkPixbufAnimationPicture *picture)
{
  GdkPixbufAnimationPicturePrivate *priv = picture->priv;

  if (priv->timeout_id == 0)
    return;

  g_source_remove (priv->timeout_id);
  priv->timeout_id = 0;
}

static gboolean gdk_pixbuf_animation_iter_timeout_callback (gpointer data);
static void
gdk_pixbuf_animation_iter_add_timeout (GdkPixbufAnimationPicture *picture)
{
  GdkPixbufAnimationPicturePrivate *priv = picture->priv;
  guint delay;

  g_assert (!priv->timeout_id);

  delay = gdk_pixbuf_animation_iter_get_delay_time (priv->iter);

  if (delay > 0)
    priv->timeout_id = gdk_threads_add_timeout (delay,
                                                gdk_pixbuf_animation_iter_timeout_callback,
                                                picture);
  }

static gboolean
gdk_pixbuf_animation_iter_timeout_callback (gpointer data)
{
  GdkPixbufAnimationPicture *picture = data;
  GdkPixbufAnimationPicturePrivate *priv = picture->priv;

  priv->timeout_id = 0;

  gdk_pixbuf_animation_iter_advance (priv->iter, NULL);

  gdk_pixbuf_animation_iter_add_timeout (picture);

  gdk_picture_changed (GDK_PICTURE (picture));

  return FALSE;
}

void
gdk_pixbuf_animation_picture_set_animation (GdkPixbufAnimationPicture *picture,
                                            GdkPixbufAnimation *       animation)
{
  GdkPixbufAnimationPicturePrivate *priv;

  g_return_if_fail (GDK_IS_PIXBUF_ANIMATION_PICTURE (picture));
  g_return_if_fail (animation == NULL || GDK_IS_PIXBUF_ANIMATION (animation));

  priv = picture->priv;

  if (animation)
    g_object_ref (animation);

  if (priv->iter)
    {
      gdk_pixbuf_animation_iter_remove_timeout (picture);
      g_object_unref (priv->iter);
      priv->iter = NULL;
    }
  if (priv->animation)
    {
      g_object_unref (priv->animation);
      priv->animation = NULL;
    }

  priv->animation = animation;
  if (animation)
    {
      GdkPixbuf *pixbuf;

      if (!gdk_pixbuf_animation_is_static_image (animation))
        {
          priv->iter = gdk_pixbuf_animation_get_iter (animation, NULL);
          gdk_pixbuf_animation_iter_add_timeout (picture);
        }

      pixbuf = gdk_pixbuf_animation_get_static_image (animation);
      gdk_picture_resized (GDK_PICTURE (picture),
                           gdk_pixbuf_get_width (pixbuf),
                           gdk_pixbuf_get_height (pixbuf));
    }
  else
    gdk_picture_resized (GDK_PICTURE (picture), 0, 0);

  g_object_notify (G_OBJECT (picture), "animation");
}

GdkPixbufAnimation *
gdk_pixbuf_animation_picture_get_animation (GdkPixbufAnimationPicture *picture)
{
  g_return_val_if_fail (GDK_IS_PIXBUF_ANIMATION_PICTURE (picture), NULL);

  return picture->priv->animation;
}

