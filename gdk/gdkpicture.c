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

#include "gdkpicture.h"

#include <cairo-gobject.h>

#include "gdkintl.h"

struct _GdkPicturePrivate {
  int width;
  int height;
};

G_DEFINE_ABSTRACT_TYPE (GdkPicture, gdk_picture, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_WIDTH,
  PROP_HEIGHT
};

enum {
  RESIZED,
  CHANGED,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

/**
 * SECTION:gdkpicture
 * @Short_description: pixel areas to display
 * @Title: GdkPicture
 * @See_also: #GdkMotionPicture
 *
 * A #GdkPicture is used to represent a pixel area of a specific size.
 * Its main job is being the interface between applications that want
 * to display pictures without a lot of work and people that want to
 * provide pictures that can be rendered.
 *
 * Pictures can resize themselves and change their contents over time.
 * If you need to react to updates, you should connect to the
 * GdkPicture::resized and GdkPicture::changed signals.
 *
 * A lot of #GdkPicture subclasses require a running main loop, so if
 * you want to use #GdkPicture without a main loop, you need to make
 * sure that it actually works.
 *
 * #GdkPicture is meant to replace #GdkPixbuf.
 */

static void
gdk_picture_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  GdkPicture *picture = GDK_PICTURE (object);
  GdkPicturePrivate *priv = picture->priv;

  switch (prop_id)
    {
    case PROP_WIDTH:
      g_value_set_int (value, priv->width);
      break;
    case PROP_HEIGHT:
      g_value_set_int (value, priv->height);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static cairo_surface_t *
gdk_picture_fallback_ref_surface (GdkPicture *picture)
{
  GdkPicturePrivate *priv = picture->priv;
  cairo_surface_t *surface;
  cairo_t *cr;

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        priv->width,
                                        priv->height);
  cr = cairo_create (surface);

  gdk_picture_draw (picture, cr);

  cairo_destroy (cr);

  return surface;
}

static void
gdk_picture_fallback_draw (GdkPicture *picture,
                           cairo_t    *cr)
{
  cairo_surface_t *surface;

  surface = gdk_picture_ref_surface (picture);
  cairo_set_source_surface (cr, surface, 0, 0);
  cairo_surface_destroy (surface);

  cairo_paint (cr);
}

static void
gdk_picture_class_init (GdkPictureClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gdk_picture_get_property;

  klass->ref_surface = gdk_picture_fallback_ref_surface;
  klass->draw = gdk_picture_fallback_draw;

  /**
   * GdkPicture:width:
   *
   * Number of pixels in the x direction.
   *
   * Since: 3.2
   */
  g_object_class_install_property (object_class,
                                   PROP_WIDTH,
                                   g_param_spec_int ("width",
                                                     P_("width"),
                                                     P_("Number of pixels in x direction"),
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GdkPicture:height:
   *
   * Number of pixels in the y direction.
   *
   * Since: 3.2
   */
  g_object_class_install_property (object_class,
                                   PROP_HEIGHT,
                                   g_param_spec_int ("height",
                                                     P_("height"),
                                                     P_("Number of pixels in y direction"),
                                                     0, G_MAXINT, 0,
                                                     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GdkPicture::changed:
   * @picture: the #GdkPicture that changed.
   * @region: the region that was changed
   *
   * The ::changed signal is emitted when the contents of a @picture
   * have changed and a redraw of parts of the @picture are necessary.
   * The given @region specifies the area that has changed.
   */
  signals[CHANGED] =
    g_signal_new (g_intern_static_string ("changed"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1, CAIRO_GOBJECT_TYPE_REGION);

  /**
   * GdkPicture::resized:
   * @picture: the #GdkPicture that changed size
   *
   * The ::resize signal is emitted when the width or height of a 
   * @picture has changed. This  and a redraw of parts of the
   * @picture are necessary.
   * After the emission of this signal, a #GdkPicture::changed
   * signal will be emitted, so it might not be necessary to connect
   * to both signals.
   */
  signals[RESIZED] =
    g_signal_new (g_intern_static_string ("resized"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  g_type_class_add_private (klass, sizeof (GdkPicturePrivate));
}

static void
gdk_picture_init (GdkPicture *picture)
{
  picture->priv = G_TYPE_INSTANCE_GET_PRIVATE (picture,
                                               GDK_TYPE_PICTURE,
                                               GdkPicturePrivate);
}

/**
 * gdk_picture_get_width:
 * @picture: the picture
 *
 * Gets the width of the @picture. That is the number of pixels in the
 * X direction. Note that the width may be 0 in certain cases, like
 * when a picture hasn't finished loading from a file yet.
 *
 * Returns: The width of @picture
 **/
int
gdk_picture_get_width (GdkPicture *picture)
{
  g_return_val_if_fail (GDK_IS_PICTURE (picture), 0);

  return picture->priv->width;
}

/**
 * gdk_picture_get_height:
 * @picture: the picture
 *
 * Gets the height of the @picture. That is the number of pixels in the
 * Y direction. Note that the width may be 0 in certain cases, like
 * when a picture hasn't finished loading from a file yet.
 *
 * Returns: The height of @picture
 **/
int
gdk_picture_get_height (GdkPicture *picture)
{
  g_return_val_if_fail (GDK_IS_PICTURE (picture), 0);

  return picture->priv->height;
}

/**
 * gdk_picture_ref_surface:
 * @picture: the picture to get the surface from
 *
 * Gets a #cairo_surface_t representing the @picture. This function is
 * useful when you don't just want to draw @picture, but do more
 * sophisticated things, like use it in a cairo_mask() call. You must not
 * modify the returned surface.
 * If you just want to draw the @picture, calling gdk_picture_draw() is
 *
 * Returns: (transfer full): A surface representing @picture. The surface
 *   remains a valid representation of @picture's contents until the next
 *   time the GdkPicture::changed signal is emitted. Use
 *   cairo_surface_destroy() to free the returned surface.
 **/
cairo_surface_t *
gdk_picture_ref_surface (GdkPicture *picture)
{
  GdkPictureClass *picture_class;

  g_return_val_if_fail (GDK_IS_PICTURE (picture), NULL);

  picture_class = GDK_PICTURE_GET_CLASS (picture);

  return picture_class->ref_surface (picture);
}

/**
 * gdk_picture_draw:
 * @picture: the picture to draw
 * @cr: the cairo context to draw to
 *
 * Draws the given @picture to the given cairo context. The cairo context
 * should be set to default values for everything but the source and the
 * matrix. Otherwise the results are undefined.
 **/
void
gdk_picture_draw (GdkPicture *picture,
                  cairo_t    *cr)
{
  GdkPictureClass *picture_class;

  g_return_if_fail (GDK_IS_PICTURE (picture));

  picture_class = GDK_PICTURE_GET_CLASS (picture);

  cairo_save (cr);

  picture_class->draw (picture, cr);

  cairo_restore (cr);
}

void
gdk_picture_resized (GdkPicture *picture,
                     int         new_width,
                     int         new_height)
{
  GObject *object;
  GdkPicturePrivate *priv;
  
  g_return_if_fail (GDK_IS_PICTURE (picture));
  g_return_if_fail (new_width >= 0);
  g_return_if_fail (new_height >= 0);

  object = G_OBJECT (picture);
  priv = picture->priv;

  g_object_freeze_notify (object);

  priv->width = new_width;
  g_object_notify (object, "width");
  priv->height = new_height;
  g_object_notify (object, "height");

  g_object_thaw_notify (object);

  g_signal_emit (picture, signals[RESIZED], 0);

  gdk_picture_changed (picture);
}

void
gdk_picture_changed (GdkPicture *picture)
{
  GdkPicturePrivate *priv;
  cairo_rectangle_int_t rect;
  
  g_return_if_fail (GDK_IS_PICTURE (picture));

  priv = picture->priv;

  rect.x = 0;
  rect.y = 0;
  rect.width = priv->width;
  rect.height = priv->height;

  gdk_picture_changed_rect (picture, &rect);
}

void
gdk_picture_changed_rect (GdkPicture                  *picture,
                          const cairo_rectangle_int_t *rect)
{
  cairo_region_t *region;

  g_return_if_fail (GDK_IS_PICTURE (picture));

  region = cairo_region_create_rectangle (rect);

  gdk_picture_changed_region (picture, region);

  cairo_region_destroy (region);
}

void
gdk_picture_changed_region (GdkPicture           *picture,
                            const cairo_region_t *region)
{
  g_return_if_fail (GDK_IS_PICTURE (picture));

  g_signal_emit (picture, signals[CHANGED], 0, region);
}
