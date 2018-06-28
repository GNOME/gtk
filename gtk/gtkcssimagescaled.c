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

#include "gtkstyleproviderprivate.h"

G_DEFINE_TYPE (GtkCssImageScaled, _gtk_css_image_scaled, GTK_TYPE_CSS_IMAGE)

static int
gtk_css_image_scaled_get_width (GtkCssImage *image)
{
  GtkCssImageScaled *scaled = GTK_CSS_IMAGE_SCALED (image);

  return _gtk_css_image_get_width (scaled->images[0])/scaled->scales[0];
}

static int
gtk_css_image_scaled_get_height (GtkCssImage *image)
{
  GtkCssImageScaled *scaled = GTK_CSS_IMAGE_SCALED (image);

  return _gtk_css_image_get_height (scaled->images[0])/scaled->scales[0];
}

static double
gtk_css_image_scaled_get_aspect_ratio (GtkCssImage *image)
{
  GtkCssImageScaled *scaled = GTK_CSS_IMAGE_SCALED (image);

  return _gtk_css_image_get_aspect_ratio (scaled->images[0]);
}

static void
gtk_css_image_scaled_snapshot (GtkCssImage *image,
                               GtkSnapshot *snapshot,
                               double       width,
                               double       height)
{
  GtkCssImageScaled *scaled = GTK_CSS_IMAGE_SCALED (image);

  gtk_css_image_snapshot (scaled->images[0], snapshot, width, height);
  // FIXME apply scale
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
      g_string_append_printf (string, ",%d", scaled->scales[i]);
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
  g_free (scaled->scales);
  scaled->scales = NULL;

  G_OBJECT_CLASS (_gtk_css_image_scaled_parent_class)->dispose (object);
}


static GtkCssImage *
gtk_css_image_scaled_compute (GtkCssImage      *image,
                              guint             property_id,
                              GtkStyleProvider *provider,
                              GtkCssStyle      *style,
                              GtkCssStyle      *parent_style)
{
  GtkCssImageScaled *scaled = GTK_CSS_IMAGE_SCALED (image);
  int scale;
  GtkCssImageScaled *res;
  int i;
  int best;

  scale = gtk_style_provider_get_scale (provider);
  scale = MAX(scale, 1);

  best = 0;
  for (i = 0; i < scaled->n_images; i++)
    {
      if (scaled->scales[i] == scale)
        {
          best = i;
          break;
        }
      else if ((scaled->scales[best] < scaled->scales[i] && scaled->scales[i] < scale) ||
               (scale < scaled->scales[i] && scaled->scales[i] < scaled->scales[best]) ||
               (scaled->scales[best] < scale && scaled->scales[i] > scale))
        {
          best = i;
        }
    }

  res = g_object_new (GTK_TYPE_CSS_IMAGE_SCALED, NULL);
  res->n_images = 1;
  res->images = g_new (GtkCssImage *, 1);
  res->scales = g_new (int, 1);

  res->images[0] = _gtk_css_image_compute (scaled->images[best],
                                           property_id,
                                           provider,
                                           style,
                                           parent_style);
  res->scales[0] = scaled->scales[best];

  return GTK_CSS_IMAGE (res);
}


static gboolean
gtk_css_image_scaled_parse (GtkCssImage  *image,
                            GtkCssParser *parser)
{
  GtkCssImageScaled *scaled = GTK_CSS_IMAGE_SCALED (image);
  GPtrArray *images;
  GArray *scales;
  int last_scale;
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
  scales = g_array_new (FALSE, FALSE, sizeof (int));

  last_scale = 0;
  do
    {
      child = _gtk_css_image_new_parse (parser);
      if (child == NULL)
        {
          g_ptr_array_free (images, TRUE);
          g_array_free (scales, TRUE);
          return FALSE;
        }
      g_ptr_array_add (images, child);
      if (!_gtk_css_parser_try (parser, ",", TRUE))
        {
          last_scale += 1;
          g_array_append_val (scales, last_scale);
          break;
        }
      else if (_gtk_css_parser_try_int (parser, &last_scale))
        {
          g_array_append_val (scales, last_scale);
          if (!_gtk_css_parser_try (parser, ",", TRUE))
            break;
        }
      else
        {
          last_scale += 1;
          g_array_append_val (scales, last_scale);
        }
    }
  while (TRUE);

  if (!_gtk_css_parser_try (parser, ")", TRUE))
    {
      g_ptr_array_free (images, TRUE);
      g_array_free (scales, TRUE);
      _gtk_css_parser_error (parser,
                             "Expected ')' at end of '-gtk-scaled'");
      return FALSE;
    }

  scaled->n_images = images->len;
  scaled->images = (GtkCssImage **) g_ptr_array_free (images, FALSE);
  scaled->scales = (int *) g_array_free (scales, FALSE);

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
  image_class->snapshot = gtk_css_image_scaled_snapshot;
  image_class->parse = gtk_css_image_scaled_parse;
  image_class->compute = gtk_css_image_scaled_compute;
  image_class->print = gtk_css_image_scaled_print;

  object_class->dispose = gtk_css_image_scaled_dispose;
}

static void
_gtk_css_image_scaled_init (GtkCssImageScaled *image_scaled)
{
}
