/*
 * Copyright © 2012 Red Hat Inc.
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

#include <math.h>
#include <string.h>

#include "gtkcssimagecrossfadeprivate.h"

#include "gtkcssnumbervalueprivate.h"

G_DEFINE_TYPE (GtkCssImageCrossFade, _gtk_css_image_cross_fade, GTK_TYPE_CSS_IMAGE)

static int
gtk_css_image_cross_fade_get_width (GtkCssImage *image)
{
  GtkCssImageCrossFade *cross_fade = GTK_CSS_IMAGE_CROSS_FADE (image);
  int start_width, end_width;

  if (cross_fade->start)
    {
      start_width = _gtk_css_image_get_width (cross_fade->start);
      /* no intrinsic width, what now? */
      if (start_width == 0)
        return 0;
    }
  else
    start_width = 0;

  if (cross_fade->end)
    {
      end_width = _gtk_css_image_get_width (cross_fade->end);
      /* no intrinsic width, what now? */
      if (end_width == 0)
        return 0;
    }
  else
    end_width = 0;

  return start_width + (end_width - start_width) * cross_fade->progress;
}

static int
gtk_css_image_cross_fade_get_height (GtkCssImage *image)
{
  GtkCssImageCrossFade *cross_fade = GTK_CSS_IMAGE_CROSS_FADE (image);
  int start_height, end_height;

  if (cross_fade->start)
    {
      start_height = _gtk_css_image_get_height (cross_fade->start);
      /* no intrinsic height, what now? */
      if (start_height == 0)
        return 0;
    }
  else
    start_height = 0;

  if (cross_fade->end)
    {
      end_height = _gtk_css_image_get_height (cross_fade->end);
      /* no intrinsic height, what now? */
      if (end_height == 0)
        return 0;
    }
  else
    end_height = 0;

  return start_height + (end_height - start_height) * cross_fade->progress;
}

static gboolean
gtk_css_image_cross_fade_equal (GtkCssImage *image1,
                                GtkCssImage *image2)
{
  GtkCssImageCrossFade *cross_fade1 = GTK_CSS_IMAGE_CROSS_FADE (image1);
  GtkCssImageCrossFade *cross_fade2 = GTK_CSS_IMAGE_CROSS_FADE (image2);

  return cross_fade1->progress == cross_fade2->progress &&
         _gtk_css_image_equal (cross_fade1->start, cross_fade2->start) &&
         _gtk_css_image_equal (cross_fade1->end, cross_fade2->end);
}

static gboolean
gtk_css_image_cross_fade_is_dynamic (GtkCssImage *image)
{
  GtkCssImageCrossFade *cross_fade = GTK_CSS_IMAGE_CROSS_FADE (image);

  return (cross_fade->start && gtk_css_image_is_dynamic (cross_fade->start))
      || (cross_fade->end && gtk_css_image_is_dynamic (cross_fade->end));
}

static GtkCssImage *
gtk_css_image_cross_fade_get_dynamic_image (GtkCssImage *image,
                                            gint64       monotonic_time)
{
  GtkCssImageCrossFade *cross_fade = GTK_CSS_IMAGE_CROSS_FADE (image);
  GtkCssImage *start, *end, *result;

  if (cross_fade->start)
    start = gtk_css_image_get_dynamic_image (cross_fade->start, monotonic_time);
  else
    start = NULL;
  if (cross_fade->end)
    end = gtk_css_image_get_dynamic_image (cross_fade->end, monotonic_time);
  else
    end = NULL;

  result = _gtk_css_image_cross_fade_new (start, end, cross_fade->progress);

  if (start)
    g_object_unref (start);
  if (end)
    g_object_unref (end);

  return result;
}

static void
gtk_css_image_cross_fade_snapshot (GtkCssImage *image,
                                   GtkSnapshot *snapshot,
                                   double       width,
                                   double       height)
{
  GtkCssImageCrossFade *cross_fade = GTK_CSS_IMAGE_CROSS_FADE (image);

  gtk_snapshot_push_cross_fade (snapshot, cross_fade->progress, "CrossFadeImage<%g>", cross_fade->progress);

  if (cross_fade->start)
    gtk_css_image_snapshot (cross_fade->start, snapshot, width, height);
  gtk_snapshot_pop (snapshot);

  if (cross_fade->end)
    gtk_css_image_snapshot (cross_fade->end, snapshot, width, height);
  gtk_snapshot_pop (snapshot);
}

static gboolean
gtk_css_image_cross_fade_parse (GtkCssImage  *image,
                                GtkCssParser *parser)
{
  GtkCssImageCrossFade *cross_fade = GTK_CSS_IMAGE_CROSS_FADE (image);
  if (!_gtk_css_parser_try (parser, "cross-fade(", TRUE))
    {
      _gtk_css_parser_error (parser, "Expected 'cross-fade('");
      return FALSE;
    }

  if (gtk_css_number_value_can_parse (parser))
    {
      GtkCssValue *number;
      
      number = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_PERCENT | GTK_CSS_POSITIVE_ONLY);
      if (number == NULL)
        return FALSE;
      cross_fade->progress = _gtk_css_number_value_get (number, 1);
      _gtk_css_value_unref (number);

      if (cross_fade->progress > 1.0)
        {
          _gtk_css_parser_error (parser, "Percentages over 100%% are not allowed");
          return FALSE;
        }
    }
  else
    cross_fade->progress = 0.5;

  cross_fade->end = _gtk_css_image_new_parse (parser);
  if (cross_fade->end == NULL)
    return FALSE;

  if (_gtk_css_parser_try (parser, ",", TRUE))
    {
      /* XXX: allow parsing colors here */
      cross_fade->start = _gtk_css_image_new_parse (parser);
      if (cross_fade->start == NULL)
        return FALSE;
    }

  if (!_gtk_css_parser_try (parser, ")", TRUE))
    {
      _gtk_css_parser_error (parser, "Missing closing bracket");
      return FALSE;
    }

  return TRUE;
}

static void
gtk_css_image_cross_fade_print (GtkCssImage *image,
                                GString     *string)
{
  GtkCssImageCrossFade *cross_fade = GTK_CSS_IMAGE_CROSS_FADE (image);

  g_string_append (string, "cross-fade(");
  if (cross_fade->progress != 0.5)
    {
      g_string_append_printf (string, "%g%% ", cross_fade->progress * 100.0);
    }

  if (cross_fade->end)
    _gtk_css_image_print (cross_fade->end, string);
  else
    g_string_append (string, "none");
  if (cross_fade->start)
    {
      g_string_append (string, ", ");
      _gtk_css_image_print (cross_fade->start, string);
    }
  g_string_append (string, ")");
}

static GtkCssImage *
gtk_css_image_cross_fade_compute (GtkCssImage      *image,
                                  guint             property_id,
                                  GtkStyleProvider *provider,
                                  GtkCssStyle      *style,
                                  GtkCssStyle      *parent_style)
{
  GtkCssImageCrossFade *cross_fade = GTK_CSS_IMAGE_CROSS_FADE (image);
  GtkCssImage *start, *end, *computed;

  if (cross_fade->start)
    start = _gtk_css_image_compute (cross_fade->start, property_id, provider, style, parent_style);
  else
    start = NULL;
  if (cross_fade->end)
    end = _gtk_css_image_compute (cross_fade->end, property_id, provider, style, parent_style);
  else
    end = NULL;

  computed = _gtk_css_image_cross_fade_new (start, end, cross_fade->progress);

  if (start)
    g_object_unref (start);
  if (end)
    g_object_unref (end);

  return computed;
}

static void
gtk_css_image_cross_fade_dispose (GObject *object)
{
  GtkCssImageCrossFade *cross_fade = GTK_CSS_IMAGE_CROSS_FADE (object);

  g_clear_object (&cross_fade->start);
  g_clear_object (&cross_fade->end);

  G_OBJECT_CLASS (_gtk_css_image_cross_fade_parent_class)->dispose (object);
}

static void
_gtk_css_image_cross_fade_class_init (GtkCssImageCrossFadeClass *klass)
{
  GtkCssImageClass *image_class = GTK_CSS_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  image_class->get_width = gtk_css_image_cross_fade_get_width;
  image_class->get_height = gtk_css_image_cross_fade_get_height;
  image_class->compute = gtk_css_image_cross_fade_compute;
  image_class->equal = gtk_css_image_cross_fade_equal;
  image_class->snapshot = gtk_css_image_cross_fade_snapshot;
  image_class->is_dynamic = gtk_css_image_cross_fade_is_dynamic;
  image_class->get_dynamic_image = gtk_css_image_cross_fade_get_dynamic_image;
  image_class->parse = gtk_css_image_cross_fade_parse;
  image_class->print = gtk_css_image_cross_fade_print;

  object_class->dispose = gtk_css_image_cross_fade_dispose;
}

static void
_gtk_css_image_cross_fade_init (GtkCssImageCrossFade *image_cross_fade)
{
}

GtkCssImage *
_gtk_css_image_cross_fade_new (GtkCssImage *start,
                               GtkCssImage *end,
                               double       progress)
{
  GtkCssImageCrossFade *cross_fade;

  g_return_val_if_fail (start == NULL || GTK_IS_CSS_IMAGE (start), NULL);
  g_return_val_if_fail (end == NULL || GTK_IS_CSS_IMAGE (end), NULL);

  cross_fade = g_object_new (GTK_TYPE_CSS_IMAGE_CROSS_FADE, NULL);
  if (start)
    cross_fade->start = g_object_ref (start);
  if (end)
    cross_fade->end = g_object_ref (end);
  cross_fade->progress = progress;

  return GTK_CSS_IMAGE (cross_fade);
}

