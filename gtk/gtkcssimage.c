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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkcssimageprivate.h"

/* for the types only */
#include "gtk/gtkcssimageurlprivate.h"

G_DEFINE_ABSTRACT_TYPE (GtkCssImage, _gtk_css_image, G_TYPE_OBJECT)

static int
gtk_css_image_real_get_width (GtkCssImage *image)
{
  return 0;
}

static int
gtk_css_image_real_get_height (GtkCssImage *image)
{
  return 0;
}

static double
gtk_css_image_real_get_aspect_ratio (GtkCssImage *image)
{
  int width, height;

  width = _gtk_css_image_get_width (image);
  height = _gtk_css_image_get_height (image);

  if (width && height)
    return (double) width / height;
  else
    return 0;
}

static GtkCssImage *
gtk_css_image_real_compute (GtkCssImage     *image,
                            GtkStyleContext *context)
{
  return g_object_ref (image);
}

static void
_gtk_css_image_class_init (GtkCssImageClass *klass)
{
  klass->get_width = gtk_css_image_real_get_width;
  klass->get_height = gtk_css_image_real_get_height;
  klass->get_aspect_ratio = gtk_css_image_real_get_aspect_ratio;
  klass->compute = gtk_css_image_real_compute;
}

static void
_gtk_css_image_init (GtkCssImage *image)
{
}

int
_gtk_css_image_get_width (GtkCssImage *image)
{
  GtkCssImageClass *klass;

  g_return_val_if_fail (GTK_IS_CSS_IMAGE (image), 0);

  klass = GTK_CSS_IMAGE_GET_CLASS (image);

  return klass->get_width (image);
}

int
_gtk_css_image_get_height (GtkCssImage *image)
{
  GtkCssImageClass *klass;

  g_return_val_if_fail (GTK_IS_CSS_IMAGE (image), 0);

  klass = GTK_CSS_IMAGE_GET_CLASS (image);

  return klass->get_height (image);
}

double
_gtk_css_image_get_aspect_ratio (GtkCssImage *image)
{
  GtkCssImageClass *klass;

  g_return_val_if_fail (GTK_IS_CSS_IMAGE (image), 0);

  klass = GTK_CSS_IMAGE_GET_CLASS (image);

  return klass->get_aspect_ratio (image);
}

GtkCssImage *
_gtk_css_image_compute (GtkCssImage     *image,
                        GtkStyleContext *context)
{
  GtkCssImageClass *klass;

  g_return_val_if_fail (GTK_IS_CSS_IMAGE (image), NULL);
  g_return_val_if_fail (GTK_IS_STYLE_CONTEXT (context), NULL);

  klass = GTK_CSS_IMAGE_GET_CLASS (image);

  return klass->compute (image, context);
}

void
_gtk_css_image_draw (GtkCssImage        *image,
                     cairo_t            *cr,
                     double              width,
                     double              height)
{
  GtkCssImageClass *klass;

  g_return_if_fail (GTK_IS_CSS_IMAGE (image));
  g_return_if_fail (cr != NULL);

  cairo_save (cr);

  klass = GTK_CSS_IMAGE_GET_CLASS (image);

  klass->draw (image, cr, width, height);

  cairo_restore (cr);
}

void
_gtk_css_image_print (GtkCssImage *image,
                      GString     *string)
{
  GtkCssImageClass *klass;

  g_return_if_fail (GTK_IS_CSS_IMAGE (image));
  g_return_if_fail (string != NULL);

  klass = GTK_CSS_IMAGE_GET_CLASS (image);

  klass->print (image, string);
}

GtkCssImage *
_gtk_css_image_new_parse (GtkCssParser *parser,
                          GFile        *base)
{
  static const struct {
    const char *prefix;
    GType (* type_func) (void);
  } image_types[] = {
    { "url", _gtk_css_image_url_get_type }
  };
  guint i;

  g_return_val_if_fail (parser != NULL, NULL);
  g_return_val_if_fail (G_IS_FILE (base), NULL);

  for (i = 0; i < G_N_ELEMENTS (image_types); i++)
    {
      if (_gtk_css_parser_has_prefix (parser, image_types[i].prefix))
        {
          GtkCssImage *image;
          GtkCssImageClass *klass;

          image = g_object_new (image_types[i].type_func (), NULL);

          klass = GTK_CSS_IMAGE_GET_CLASS (image);
          if (!klass->parse (image, parser, base))
            {
              g_object_unref (image);
              return NULL;
            }

          return image;
        }
    }

  _gtk_css_parser_error (parser, "Not a valid image");
  return NULL;
}

