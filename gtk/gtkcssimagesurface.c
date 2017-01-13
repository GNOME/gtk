/*
 * Copyright © 2011 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkcssimagesurfaceprivate.h"

G_DEFINE_TYPE (GtkCssImageSurface, _gtk_css_image_surface, GTK_TYPE_CSS_IMAGE)

static int
gtk_css_image_surface_get_width (GtkCssImage *image)
{
  GtkCssImageSurface *surface = GTK_CSS_IMAGE_SURFACE (image);

  if (surface->texture == NULL)
    return 0;

  return gsk_texture_get_width (surface->texture);
}

static int
gtk_css_image_surface_get_height (GtkCssImage *image)
{
  GtkCssImageSurface *surface = GTK_CSS_IMAGE_SURFACE (image);

  if (surface->texture == NULL)
    return 0;

  return gsk_texture_get_height (surface->texture);
}

static void
gtk_css_image_surface_snapshot (GtkCssImage *image,
                                GtkSnapshot *snapshot,
                                double       width,
                                double       height)
{
  GtkCssImageSurface *surface = GTK_CSS_IMAGE_SURFACE (image);

  if (surface->texture == NULL)
    return;

  gtk_snapshot_append_texture (snapshot,
                               surface->texture,
                               &GRAPHENE_RECT_INIT (0, 0, width, height),
                               "Surface Image %dx%d",
                               gsk_texture_get_width (surface->texture),
                               gsk_texture_get_height (surface->texture));
}

static void
gtk_css_image_surface_print (GtkCssImage *image,
                             GString     *string)
{
  g_string_append (string, "none /* FIXME */");
}

static void
gtk_css_image_surface_dispose (GObject *object)
{
  GtkCssImageSurface *surface = GTK_CSS_IMAGE_SURFACE (object);

  g_clear_object (&surface->texture);

  G_OBJECT_CLASS (_gtk_css_image_surface_parent_class)->dispose (object);
}

static void
_gtk_css_image_surface_class_init (GtkCssImageSurfaceClass *klass)
{
  GtkCssImageClass *image_class = GTK_CSS_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  image_class->get_width = gtk_css_image_surface_get_width;
  image_class->get_height = gtk_css_image_surface_get_height;
  image_class->snapshot = gtk_css_image_surface_snapshot;
  image_class->print = gtk_css_image_surface_print;

  object_class->dispose = gtk_css_image_surface_dispose;
}

static void
_gtk_css_image_surface_init (GtkCssImageSurface *image_surface)
{
}

GtkCssImage *
gtk_css_image_surface_new (GskTexture *texture)
{
  GtkCssImage *image;

  image = g_object_new (GTK_TYPE_CSS_IMAGE_SURFACE, NULL);
  
  if (texture)
    GTK_CSS_IMAGE_SURFACE (image)->texture = g_object_ref (texture);

  return image;
}

GtkCssImage *
gtk_css_image_surface_new_for_pixbuf (GdkPixbuf *pixbuf)
{
  GtkCssImage *image;
  GskTexture *texture;

  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

  texture = gsk_texture_new_for_pixbuf (pixbuf);
  image = gtk_css_image_surface_new (texture);
  g_object_unref (texture);

  return image;
}

