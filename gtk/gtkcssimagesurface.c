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

  return cairo_image_surface_get_width (surface->surface);
}

static int
gtk_css_image_surface_get_height (GtkCssImage *image)
{
  GtkCssImageSurface *surface = GTK_CSS_IMAGE_SURFACE (image);

  return cairo_image_surface_get_height (surface->surface);
}

static void
gtk_css_image_surface_draw (GtkCssImage *image,
                            cairo_t     *cr,
                            double       width,
                            double       height)
{
  GtkCssImageSurface *surface = GTK_CSS_IMAGE_SURFACE (image);

  cairo_rectangle (cr, 0, 0, width, height);
  cairo_scale (cr,
               width / cairo_image_surface_get_width (surface->surface),
               height / cairo_image_surface_get_height (surface->surface));
  cairo_set_source_surface (cr, surface->surface, 0, 0);
  cairo_pattern_set_extend (cairo_get_source (cr), CAIRO_EXTEND_PAD);
  cairo_fill (cr);
}

static cairo_status_t
surface_write (void                *closure,
               const unsigned char *data,
               unsigned int         length)
{
  g_byte_array_append (closure, data, length);

  return CAIRO_STATUS_SUCCESS;
}

static void
gtk_css_image_surface_print (GtkCssImage *image,
                             GString     *string)
{
#if CAIRO_HAS_PNG_FUNCTIONS
  GtkCssImageSurface *surface = GTK_CSS_IMAGE_SURFACE (image);
  GByteArray *array;
  char *base64;
  
  array = g_byte_array_new ();
  cairo_surface_write_to_png_stream (surface->surface, surface_write, array);
  base64 = g_base64_encode (array->data, array->len);
  g_byte_array_free (array, TRUE);

  g_string_append (string, "url(\"data:image/png;base64,");
  g_string_append (string, base64);
  g_string_append (string, "\")");

  g_free (base64);
#else
  g_string_append (string, "none /* you need cairo png functions enabled to make this work */");
#endif
}

static void
gtk_css_image_surface_dispose (GObject *object)
{
  GtkCssImageSurface *surface = GTK_CSS_IMAGE_SURFACE (object);

  if (surface->surface)
    {
      cairo_surface_destroy (surface->surface);
      surface->surface = NULL;
    }

  G_OBJECT_CLASS (_gtk_css_image_surface_parent_class)->dispose (object);
}

static void
_gtk_css_image_surface_class_init (GtkCssImageSurfaceClass *klass)
{
  GtkCssImageClass *image_class = GTK_CSS_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  image_class->get_width = gtk_css_image_surface_get_width;
  image_class->get_height = gtk_css_image_surface_get_height;
  image_class->draw = gtk_css_image_surface_draw;
  image_class->print = gtk_css_image_surface_print;

  object_class->dispose = gtk_css_image_surface_dispose;
}

static void
_gtk_css_image_surface_init (GtkCssImageSurface *image_surface)
{
}

GtkCssImage *
_gtk_css_image_surface_new (cairo_surface_t *surface)
{
  GtkCssImage *image;

  g_return_val_if_fail (surface != NULL, NULL);

  image = g_object_new (GTK_TYPE_CSS_IMAGE_SURFACE, NULL);
  
  GTK_CSS_IMAGE_SURFACE (image)->surface = cairo_surface_reference (surface);

  return image;
}

GtkCssImage *
_gtk_css_image_surface_new_for_pixbuf (GdkPixbuf *pixbuf)
{
  GtkCssImage *image;
  cairo_surface_t *surface;
  cairo_t *cr;

  g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        gdk_pixbuf_get_width (pixbuf),
                                        gdk_pixbuf_get_height (pixbuf));
  cr = cairo_create (surface);
  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
  cairo_paint (cr);
  cairo_destroy (cr);

  image = _gtk_css_image_surface_new (surface);

  cairo_surface_destroy (surface);

  return image;
}

