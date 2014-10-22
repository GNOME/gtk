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

#include "gtkcssimageprivate.h"

#include "gtkcssstyleprivate.h"

/* for the types only */
#include "gtk/gtkcssimagecrossfadeprivate.h"
#include "gtk/gtkcssimagegradientprivate.h"
#include "gtk/gtkcssimageiconthemeprivate.h"
#include "gtk/gtkcssimagelinearprivate.h"
#include "gtk/gtkcssimageurlprivate.h"
#include "gtk/gtkcssimagescaledprivate.h"
#include "gtk/gtkcssimagewin32private.h"

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
gtk_css_image_real_compute (GtkCssImage             *image,
                            guint                    property_id,
                            GtkStyleProviderPrivate *provider,
			    int                      scale,
                            GtkCssStyle    *values,
                            GtkCssStyle    *parent_values,
                            GtkCssDependencies      *dependencies)
{
  return g_object_ref (image);
}

static gboolean
gtk_css_image_real_equal (GtkCssImage *image1,
                          GtkCssImage *image2)
{
  return FALSE;
}

static GtkCssImage *
gtk_css_image_real_transition (GtkCssImage *start,
                               GtkCssImage *end,
                               guint        property_id,
                               double       progress)
{
  if (progress <= 0.0)
    return g_object_ref (start);
  else if (progress >= 1.0)
    return end ? g_object_ref (end) : NULL;
  else
    return _gtk_css_image_cross_fade_new (start, end, progress);
}

static void
_gtk_css_image_class_init (GtkCssImageClass *klass)
{
  klass->get_width = gtk_css_image_real_get_width;
  klass->get_height = gtk_css_image_real_get_height;
  klass->get_aspect_ratio = gtk_css_image_real_get_aspect_ratio;
  klass->compute = gtk_css_image_real_compute;
  klass->equal = gtk_css_image_real_equal;
  klass->transition = gtk_css_image_real_transition;
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
_gtk_css_image_compute (GtkCssImage             *image,
                        guint                    property_id,
                        GtkStyleProviderPrivate *provider,
			int                      scale,
                        GtkCssStyle    *values,
                        GtkCssStyle    *parent_values,
                        GtkCssDependencies      *dependencies)
{
  GtkCssDependencies unused;
  GtkCssImageClass *klass;

  g_return_val_if_fail (GTK_IS_CSS_IMAGE (image), NULL);
  g_return_val_if_fail (GTK_IS_CSS_STYLE (values), NULL);
  g_return_val_if_fail (parent_values == NULL || GTK_IS_CSS_STYLE (parent_values), NULL);

  if (dependencies == NULL)
    dependencies = &unused;
  *dependencies = 0;

  klass = GTK_CSS_IMAGE_GET_CLASS (image);

  return klass->compute (image, property_id, provider, scale, values, parent_values, dependencies);
}

GtkCssImage *
_gtk_css_image_transition (GtkCssImage *start,
                           GtkCssImage *end,
                           guint        property_id,
                           double       progress)
{
  GtkCssImageClass *klass;

  g_return_val_if_fail (start == NULL || GTK_IS_CSS_IMAGE (start), NULL);
  g_return_val_if_fail (end == NULL || GTK_IS_CSS_IMAGE (end), NULL);

  progress = CLAMP (progress, 0.0, 1.0);

  if (start == NULL)
    {
      if (end == NULL)
        return NULL;
      else
        {
          start = end;
          end = NULL;
          progress = 1.0 - progress;
        }
    }

  klass = GTK_CSS_IMAGE_GET_CLASS (start);

  return klass->transition (start, end, property_id, progress);
}

gboolean
_gtk_css_image_equal (GtkCssImage *image1,
                      GtkCssImage *image2)
{
  GtkCssImageClass *klass;

  g_return_val_if_fail (image1 == NULL || GTK_IS_CSS_IMAGE (image1), FALSE);
  g_return_val_if_fail (image2 == NULL || GTK_IS_CSS_IMAGE (image2), FALSE);

  if (image1 == image2)
    return TRUE;

  if (image1 == NULL || image2 == NULL)
    return FALSE;

  if (G_OBJECT_TYPE (image1) != G_OBJECT_TYPE (image2))
    return FALSE;

  klass = GTK_CSS_IMAGE_GET_CLASS (image1);

  return klass->equal (image1, image2);
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
  g_return_if_fail (width > 0);
  g_return_if_fail (height > 0);

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

/* Applies the algorithm outlined in
 * http://dev.w3.org/csswg/css3-images/#default-sizing
 */
void
_gtk_css_image_get_concrete_size (GtkCssImage *image,
                                  double       specified_width,
                                  double       specified_height,
                                  double       default_width,
                                  double       default_height,
                                  double      *concrete_width,
                                  double      *concrete_height)
{
  double image_width, image_height, image_aspect;

  g_return_if_fail (GTK_IS_CSS_IMAGE (image));
  g_return_if_fail (specified_width >= 0);
  g_return_if_fail (specified_height >= 0);
  g_return_if_fail (default_width > 0);
  g_return_if_fail (default_height > 0);
  g_return_if_fail (concrete_width != NULL);
  g_return_if_fail (concrete_height != NULL);

  /* If the specified size is a definite width and height,
   * the concrete object size is given that width and height.
   */
  if (specified_width && specified_height)
    {
      *concrete_width = specified_width;
      *concrete_height = specified_height;
      return;
    }

  image_width  = _gtk_css_image_get_width (image);
  image_height = _gtk_css_image_get_height (image);
  image_aspect = _gtk_css_image_get_aspect_ratio (image);

  /* If the specified size has neither a definite width nor height,
   * and has no additional contraints, the dimensions of the concrete
   * object size are calculated as follows:
   */
  if (specified_width == 0.0 && specified_height == 0.0)
    {
      /* If the object has only an intrinsic aspect ratio,
       * the concrete object size must have that aspect ratio,
       * and additionally be as large as possible without either
       * its height or width exceeding the height or width of the
       * default object size.
       */
      if (image_aspect > 0 && image_width == 0 && image_height == 0)
        {
          if (image_aspect * default_height > default_width)
            {
              *concrete_width = default_height * image_aspect;
              *concrete_height = default_height;
            }
          else
            {
              *concrete_width = default_width;
              *concrete_height = default_width / image_aspect;
            }
        }
      else
        {
          /* Otherwise, the width and height of the concrete object
           * size is the same as the object's intrinsic width and
           * intrinsic height, if they exist.
           * If the concrete object size is still missing a width or
           * height, and the object has an intrinsic aspect ratio,
           * the missing dimension is calculated from the present
           * dimension and the intrinsic aspect ratio.
           * Otherwise, the missing dimension is taken from the default
           * object size. 
           */
          if (image_width)
            *concrete_width = image_width;
          else if (image_aspect)
            *concrete_width = image_height * image_aspect;
          else
            *concrete_width = default_width;

          if (image_height)
            *concrete_height = image_height;
          else if (image_aspect)
            *concrete_height = image_width / image_aspect;
          else
            *concrete_height = default_height;
        }

      return;
    }

  /* If the specified size has only a width or height, but not both,
   * then the concrete object size is given that specified width or height.
   * The other dimension is calculated as follows:
   * If the object has an intrinsic aspect ratio, the missing dimension of
   * the concrete object size is calculated using the intrinsic aspect-ratio
   * and the present dimension.
   * Otherwise, if the missing dimension is present in the object's intrinsic
   * dimensions, the missing dimension is taken from the object's intrinsic
   * dimensions.
   * Otherwise, the missing dimension of the concrete object size is taken
   * from the default object size. 
   */
  if (specified_width)
    {
      *concrete_width = specified_width;
      if (image_aspect)
        *concrete_height = specified_width / image_aspect;
      else if (image_height)
        *concrete_height = image_height;
      else
        *concrete_height = default_height;
    }
  else
    {
      *concrete_height = specified_height;
      if (image_aspect)
        *concrete_width = specified_height * image_aspect;
      else if (image_width)
        *concrete_width = image_width;
      else
        *concrete_width = default_width;
    }
}

cairo_surface_t *
_gtk_css_image_get_surface (GtkCssImage     *image,
                            cairo_surface_t *target,
                            int              surface_width,
                            int              surface_height)
{
  cairo_surface_t *result;
  cairo_t *cr;

  g_return_val_if_fail (GTK_IS_CSS_IMAGE (image), NULL);
  g_return_val_if_fail (surface_width > 0, NULL);
  g_return_val_if_fail (surface_height > 0, NULL);


  if (target)
    {
      result = cairo_surface_create_similar (target,
					     CAIRO_CONTENT_COLOR_ALPHA,
					     surface_width,
					     surface_height);
    }
  else
    result = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                         surface_width,
                                         surface_height);

  cr = cairo_create (result);
  _gtk_css_image_draw (image, cr, surface_width, surface_height);
  cairo_destroy (cr);

  return result;
}

static GType
gtk_css_image_get_parser_type (GtkCssParser *parser)
{
  static const struct {
    const char *prefix;
    GType (* type_func) (void);
  } image_types[] = {
    { "url", _gtk_css_image_url_get_type },
    { "-gtk-gradient", _gtk_css_image_gradient_get_type },
    { "-gtk-icontheme", _gtk_css_image_icon_theme_get_type },
    { "-gtk-scaled", _gtk_css_image_scaled_get_type },
    { "-gtk-win32-theme-part", _gtk_css_image_win32_get_type },
    { "linear-gradient", _gtk_css_image_linear_get_type },
    { "repeating-linear-gradient", _gtk_css_image_linear_get_type },
    { "cross-fade", _gtk_css_image_cross_fade_get_type }
  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS (image_types); i++)
    {
      if (_gtk_css_parser_has_prefix (parser, image_types[i].prefix))
        return image_types[i].type_func ();
    }

  return G_TYPE_INVALID;
}

/**
 * _gtk_css_image_can_parse:
 * @parser: a css parser
 *
 * Checks if the parser can potentially parse the given stream as an
 * image from looking at the first token of @parser. This is useful for
 * implementing shorthand properties. A successful parse of an image
 * can not be guaranteed.
 *
 * Returns: %TURE if it looks like an image.
 **/
gboolean
_gtk_css_image_can_parse (GtkCssParser *parser)
{
  return gtk_css_image_get_parser_type (parser) != G_TYPE_INVALID;
}

GtkCssImage *
_gtk_css_image_new_parse (GtkCssParser *parser)
{
  GtkCssImageClass *klass;
  GtkCssImage *image;
  GType image_type;

  g_return_val_if_fail (parser != NULL, NULL);

  image_type = gtk_css_image_get_parser_type (parser);
  if (image_type == G_TYPE_INVALID)
    {
      _gtk_css_parser_error (parser, "Not a valid image");
      return NULL;
    }

  image = g_object_new (image_type, NULL);

  klass = GTK_CSS_IMAGE_GET_CLASS (image);
  if (!klass->parse (image, parser))
    {
      g_object_unref (image);
      return NULL;
    }

  return image;
}

