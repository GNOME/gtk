/*
 * Copyright © 2016 Red Hat Inc.
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtkcssimagefallbackprivate.h"
#include "gtkcssimagesurfaceprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkcssrgbavalueprivate.h"

#include "gtkstyleproviderprivate.h"

G_DEFINE_TYPE (GtkCssImageFallback, _gtk_css_image_fallback, GTK_TYPE_CSS_IMAGE)

static int
gtk_css_image_fallback_get_width (GtkCssImage *image)
{
  GtkCssImageFallback *fallback = GTK_CSS_IMAGE_FALLBACK (image);

  if (fallback->used < 0)
    return 0;

  return _gtk_css_image_get_width (fallback->images[fallback->used]);
}

static int
gtk_css_image_fallback_get_height (GtkCssImage *image)
{
  GtkCssImageFallback *fallback = GTK_CSS_IMAGE_FALLBACK (image);

  if (fallback->used < 0)
    return 0;

  return _gtk_css_image_get_height (fallback->images[fallback->used]);
}

static double
gtk_css_image_fallback_get_aspect_ratio (GtkCssImage *image)
{
  GtkCssImageFallback *fallback = GTK_CSS_IMAGE_FALLBACK (image);

  if (fallback->used < 0)
    return 0;

  return _gtk_css_image_get_aspect_ratio (fallback->images[fallback->used]);
}

static void
gtk_css_image_fallback_draw (GtkCssImage *image,
                             cairo_t     *cr,
                             double       width,
                             double       height)
{
  GtkCssImageFallback *fallback = GTK_CSS_IMAGE_FALLBACK (image);

  if (fallback->used < 0)
    {
      if (fallback->color)
        gdk_cairo_set_source_rgba (cr, _gtk_css_rgba_value_get_rgba (fallback->color));
      else
        cairo_set_source_rgb (cr, 1.0, 0.0, 0.0);

      cairo_rectangle (cr, 0, 0, width, height);
      cairo_fill (cr);
    }
  else
    _gtk_css_image_draw (fallback->images[fallback->used], cr, width, height);
}

static void
gtk_css_image_fallback_print (GtkCssImage *image,
                              GString     *string)
{
  GtkCssImageFallback *fallback = GTK_CSS_IMAGE_FALLBACK (image);
  int i;

  g_string_append (string, "image(");
  for (i = 0; i < fallback->n_images; i++)
    {
      if (i > 0)
        g_string_append (string, ",");
      _gtk_css_image_print (fallback->images[i], string);
    }
  if (fallback->color)
    {
      if (fallback->n_images > 0)
        g_string_append (string, ",");
      _gtk_css_value_print (fallback->color, string);
    }

  g_string_append (string, ")");
}

static void
gtk_css_image_fallback_dispose (GObject *object)
{
  GtkCssImageFallback *fallback = GTK_CSS_IMAGE_FALLBACK (object);
  int i;

  for (i = 0; i < fallback->n_images; i++)
    g_object_unref (fallback->images[i]);
  g_free (fallback->images);
  fallback->images = NULL;

  if (fallback->color)
    {
      _gtk_css_value_unref (fallback->color);
      fallback->color = NULL;
    }

  G_OBJECT_CLASS (_gtk_css_image_fallback_parent_class)->dispose (object);
}


static GtkCssImage *
gtk_css_image_fallback_compute (GtkCssImage             *image,
                                guint                    property_id,
                                GtkStyleProviderPrivate *provider,
                                GtkCssStyle             *style,
                                GtkCssStyle             *parent_style)
{
  GtkCssImageFallback *fallback = GTK_CSS_IMAGE_FALLBACK (image);
  GtkCssImageFallback *copy;
  int i;

  if (fallback->used < 0)
    {
      copy = g_object_new (_gtk_css_image_fallback_get_type (), NULL);
      copy->n_images = fallback->n_images;
      copy->images = g_new (GtkCssImage *, fallback->n_images);
      for (i = 0; i < fallback->n_images; i++)
        {
          copy->images[i] = _gtk_css_image_compute (fallback->images[i],
                                                    property_id,
                                                    provider,
                                                    style,
                                                    parent_style);

          /* Assume that failing to load an image leaves a 0x0 surface image */
          if (GTK_IS_CSS_IMAGE_SURFACE (copy->images[i]) &&
              _gtk_css_image_get_width (copy->images[i]) == 0 &&
              _gtk_css_image_get_height (copy->images[i]) == 0)
            continue;

          if (copy->used < 0)
            copy->used = i;
        }

      if (fallback->color)
        copy->color = _gtk_css_value_compute (fallback->color,
                                              property_id,
                                              provider,
                                              style,
                                              parent_style);
      else
        copy->color = NULL;

      return GTK_CSS_IMAGE (copy);
    }
  else
    return GTK_CSS_IMAGE (g_object_ref (fallback));
}

static gboolean
gtk_css_image_fallback_parse (GtkCssImage  *image,
                              GtkCssParser *parser)
{
  GtkCssImageFallback *fallback = GTK_CSS_IMAGE_FALLBACK (image);
  GPtrArray *images;
  GtkCssImage *child;

  if (!_gtk_css_parser_try (parser, "image", TRUE))
    {
      _gtk_css_parser_error (parser, "'image'");
      return FALSE;
    }

  if (!_gtk_css_parser_try (parser, "(", TRUE))
    {
      _gtk_css_parser_error (parser,
                             "Expected '(' after 'image'");
      return FALSE;
    }

  images = g_ptr_array_new_with_free_func (g_object_unref);

  do
    {
      child = NULL;
      if (_gtk_css_image_can_parse (parser))
        child = _gtk_css_image_new_parse (parser);
      if (child == NULL)
        {
          fallback->color = _gtk_css_color_value_parse (parser);
          if (fallback->color)
            break;

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
                             "Expected ')' at end of 'image'");
      return FALSE;
    }

  fallback->n_images = images->len;
  fallback->images = (GtkCssImage **) g_ptr_array_free (images, FALSE);

  return TRUE;
}

static void
_gtk_css_image_fallback_class_init (GtkCssImageFallbackClass *klass)
{
  GtkCssImageClass *image_class = GTK_CSS_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  image_class->get_width = gtk_css_image_fallback_get_width;
  image_class->get_height = gtk_css_image_fallback_get_height;
  image_class->get_aspect_ratio = gtk_css_image_fallback_get_aspect_ratio;
  image_class->draw = gtk_css_image_fallback_draw;
  image_class->parse = gtk_css_image_fallback_parse;
  image_class->compute = gtk_css_image_fallback_compute;
  image_class->print = gtk_css_image_fallback_print;

  object_class->dispose = gtk_css_image_fallback_dispose;
}

static void
_gtk_css_image_fallback_init (GtkCssImageFallback *image_fallback)
{
  image_fallback->used = -1;
}
