/*
 * Copyright © 2013 Red Hat Inc.
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
 * Authors: Alexander Larsson <alexl@gnome.org>
 */

#include "config.h"

#include "gtkcssimagescaledprivate.h"

G_DEFINE_TYPE (GtkCssImageScaled, _gtk_css_image_scaled, GTK_TYPE_CSS_IMAGE)

static int
gtk_css_image_scaled_get_width (GtkCssImage *image)
{
  GtkCssImageScaled *scaled = GTK_CSS_IMAGE_SCALED (image);

  return _gtk_css_image_get_width (scaled->images[scaled->scale - 1]) / scaled->scale;
}

static int
gtk_css_image_scaled_get_height (GtkCssImage *image)
{
  GtkCssImageScaled *scaled = GTK_CSS_IMAGE_SCALED (image);

  return _gtk_css_image_get_height (scaled->images[scaled->scale - 1]) / scaled->scale;
}

static double
gtk_css_image_scaled_get_aspect_ratio (GtkCssImage *image)
{
  GtkCssImageScaled *scaled = GTK_CSS_IMAGE_SCALED (image);

  return _gtk_css_image_get_aspect_ratio (scaled->images[scaled->scale - 1]);
}

static void
gtk_css_image_scaled_draw (GtkCssImage *image,
			   cairo_t     *cr,
			   double       width,
			   double       height)
{
  GtkCssImageScaled *scaled = GTK_CSS_IMAGE_SCALED (image);

  _gtk_css_image_draw (scaled->images[scaled->scale - 1], cr, width, height);
}

static void
gtk_css_image_scaled_print (GtkCssImage *image,
                             GString     *string)
{
  GtkCssImageScaled *scaled = GTK_CSS_IMAGE_SCALED (image);
  int i;
  
  g_string_append (string, "-gtk-scaled(");
  for (i = 0; i < scaled->n_images; i++)
    {
      _gtk_css_image_print (scaled->images[i], string);
      if (i != scaled->n_images - 1)
	g_string_append (string, ",");
    }
  g_string_append (string, ")");
}

static void
gtk_css_image_scaled_dispose (GObject *object)
{
  GtkCssImageScaled *scaled = GTK_CSS_IMAGE_SCALED (object);
  int i;

  for (i = 0; i < scaled->n_images; i++)
    g_object_unref (scaled->images[i]);
  g_free (scaled->images);
  scaled->images = NULL;

  G_OBJECT_CLASS (_gtk_css_image_scaled_parent_class)->dispose (object);
}


static GtkCssImage *
gtk_css_image_scaled_compute (GtkCssImage             *image,
			      guint                    property_id,
			      GtkStyleProviderPrivate *provider,
			      int                      scale,
			      GtkCssStyle    *values,
			      GtkCssStyle    *parent_values,
			      GtkCssDependencies      *dependencies)
{
  GtkCssImageScaled *scaled = GTK_CSS_IMAGE_SCALED (image);
  GtkCssImageScaled *copy;
  int i;

  scale = MAX(MIN (scale, scaled->n_images), 1);

  if (scaled->scale == scale)
    return g_object_ref (scaled);
  else
    {
      copy = g_object_new (_gtk_css_image_scaled_get_type (), NULL);
      copy->scale = scale;
      copy->n_images = scaled->n_images;
      copy->images = g_new (GtkCssImage *, scaled->n_images);
      for (i = 0; i < scaled->n_images; i++)
        {
          if (i == scale - 1)
            copy->images[i] = _gtk_css_image_compute (scaled->images[i],
                                                      property_id,
                                                      provider,
                                                      scale,
                                                      values,
                                                      parent_values,
                                                      dependencies);
          else
            copy->images[i] = g_object_ref (scaled->images[i]);
        }

      return GTK_CSS_IMAGE (copy);
    }
}

static gboolean
gtk_css_image_scaled_parse (GtkCssImage  *image,
			    GtkCssParser *parser)
{
  GtkCssImageScaled *scaled = GTK_CSS_IMAGE_SCALED (image);
  GPtrArray *images;
  GtkCssImage *child;

  if (!_gtk_css_parser_try (parser, "-gtk-scaled", TRUE))
    {
      _gtk_css_parser_error (parser, "'-gtk-scaled'");
      return FALSE;
    }

  if (!_gtk_css_parser_try (parser, "(", TRUE))
    {
      _gtk_css_parser_error (parser,
                             "Expected '(' after '-gtk-scaled'");
      return FALSE;
    }

  images = g_ptr_array_new_with_free_func (g_object_unref);

  do
    {
      child = _gtk_css_image_new_parse (parser);
      if (child == NULL)
	{
	  g_ptr_array_free (images, TRUE);
	  return FALSE;
	}
      g_ptr_array_add (images, child);
      
    }
  while ( _gtk_css_parser_try (parser, ",", TRUE));

  if (!_gtk_css_parser_try (parser, ")", TRUE))
    {
      g_ptr_array_free (images, TRUE);
      _gtk_css_parser_error (parser,
                             "Expected ')' at end of '-gtk-scaled'");
      return FALSE;
    }

  scaled->n_images = images->len;
  scaled->images = (GtkCssImage **) g_ptr_array_free (images, FALSE);

  return TRUE;
}

static void
_gtk_css_image_scaled_class_init (GtkCssImageScaledClass *klass)
{
  GtkCssImageClass *image_class = GTK_CSS_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  image_class->get_width = gtk_css_image_scaled_get_width;
  image_class->get_height = gtk_css_image_scaled_get_height;
  image_class->get_aspect_ratio = gtk_css_image_scaled_get_aspect_ratio;
  image_class->draw = gtk_css_image_scaled_draw;
  image_class->parse = gtk_css_image_scaled_parse;
  image_class->compute = gtk_css_image_scaled_compute;
  image_class->print = gtk_css_image_scaled_print;

  object_class->dispose = gtk_css_image_scaled_dispose;
}

static void
_gtk_css_image_scaled_init (GtkCssImageScaled *image_scaled)
{
  image_scaled->scale = 1;
}
