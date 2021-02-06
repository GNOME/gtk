/*
 * Copyright © 2021 Red Hat Inc.
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

#include <math.h>
#include <string.h>

#include "gtkcssimagefilterprivate.h"

#include "gtkcssfiltervalueprivate.h"


G_DEFINE_TYPE (GtkCssImageFilter, gtk_css_image_filter, GTK_TYPE_CSS_IMAGE)

static int
gtk_css_image_filter_get_width (GtkCssImage *image)
{
  GtkCssImageFilter *self = GTK_CSS_IMAGE_FILTER (image);

  return _gtk_css_image_get_width (self->image);
}

static int
gtk_css_image_filter_get_height (GtkCssImage *image)
{
  GtkCssImageFilter *self = GTK_CSS_IMAGE_FILTER (image);

  return _gtk_css_image_get_height (self->image);
}

static gboolean
gtk_css_image_filter_equal (GtkCssImage *image1,
                            GtkCssImage *image2)
{
  GtkCssImageFilter *filter1 = GTK_CSS_IMAGE_FILTER (image1);
  GtkCssImageFilter *filter2 = GTK_CSS_IMAGE_FILTER (image2);

  return _gtk_css_image_equal (filter1->image, filter2->image) &&
         _gtk_css_value_equal (filter1->filter, filter2->filter);
}

static gboolean
gtk_css_image_filter_is_dynamic (GtkCssImage *image)
{
  GtkCssImageFilter *self = GTK_CSS_IMAGE_FILTER (image);

  return gtk_css_image_is_dynamic (self->image);
}

static GtkCssImage *
gtk_css_image_filter_get_dynamic_image (GtkCssImage *image,
                                        gint64       monotonic_time)
{
  GtkCssImageFilter *self = GTK_CSS_IMAGE_FILTER (image);

  return gtk_css_image_filter_new (gtk_css_image_get_dynamic_image (self->image, monotonic_time),
                                   self->filter);
}

static void
gtk_css_image_filter_snapshot (GtkCssImage *image,
                               GtkSnapshot *snapshot,
                               double       width,
                               double       height)
{
  GtkCssImageFilter *self = GTK_CSS_IMAGE_FILTER (image);

  gtk_css_filter_value_push_snapshot (self->filter, snapshot);
  gtk_css_image_snapshot (self->image, snapshot, width, height);
  gtk_css_filter_value_pop_snapshot (self->filter, snapshot);
}

static guint
gtk_css_image_filter_parse_arg (GtkCssParser *parser,
                                guint         arg,
                                gpointer      data)
{
  GtkCssImageFilter *self = data;

  switch (arg)
    {
    case 0:
      self->image = _gtk_css_image_new_parse (parser);
      if (self->image == NULL)
        return 0;
      return 1;

    case 1:
      self->filter = gtk_css_filter_value_parse (parser);
      if (self->filter == NULL)
        return 0;
      return 1;

    default:
      g_assert_not_reached ();
      return 0;
    }
}

static gboolean
gtk_css_image_filter_parse (GtkCssImage  *image,
                            GtkCssParser *parser)
{
  if (!gtk_css_parser_has_function (parser, "filter"))
    {
      gtk_css_parser_error_syntax (parser, "Expected 'filter('");
      return FALSE;
    }

  return gtk_css_parser_consume_function (parser, 2, 2, gtk_css_image_filter_parse_arg, image);
}

static void
gtk_css_image_filter_print (GtkCssImage *image,
                            GString     *string)
{
  GtkCssImageFilter *self = GTK_CSS_IMAGE_FILTER (image);

  g_string_append (string, "filter(");
  _gtk_css_image_print (self->image, string);
  g_string_append (string, ",");
  _gtk_css_value_print (self->filter, string);
  g_string_append (string, ")");
}

static GtkCssImage *
gtk_css_image_filter_compute (GtkCssImage      *image,
                              guint             property_id,
                              GtkStyleProvider *provider,
                              GtkCssStyle      *style,
                              GtkCssStyle      *parent_style)
{
  GtkCssImageFilter *self = GTK_CSS_IMAGE_FILTER (image);

  return gtk_css_image_filter_new (_gtk_css_image_compute (self->image, property_id, provider, style, parent_style),
                                   _gtk_css_value_compute (self->filter, property_id, provider, style, parent_style));
}

static void
gtk_css_image_filter_dispose (GObject *object)
{
  GtkCssImageFilter *self = GTK_CSS_IMAGE_FILTER (object);

  g_clear_object (&self->image);
  g_clear_pointer (&self->filter, _gtk_css_value_unref);

  G_OBJECT_CLASS (gtk_css_image_filter_parent_class)->dispose (object);
}

static gboolean
gtk_css_image_filter_is_computed (GtkCssImage *image)
{
  GtkCssImageFilter *self = GTK_CSS_IMAGE_FILTER (image);

  return gtk_css_image_is_computed (self->image) &&
         gtk_css_value_is_computed (self->filter);
}

static void
gtk_css_image_filter_class_init (GtkCssImageFilterClass *klass)
{
  GtkCssImageClass *image_class = GTK_CSS_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  image_class->get_width = gtk_css_image_filter_get_width;
  image_class->get_height = gtk_css_image_filter_get_height;
  image_class->compute = gtk_css_image_filter_compute;
  image_class->equal = gtk_css_image_filter_equal;
  image_class->snapshot = gtk_css_image_filter_snapshot;
  image_class->is_dynamic = gtk_css_image_filter_is_dynamic;
  image_class->get_dynamic_image = gtk_css_image_filter_get_dynamic_image;
  image_class->parse = gtk_css_image_filter_parse;
  image_class->print = gtk_css_image_filter_print;
  image_class->is_computed = gtk_css_image_filter_is_computed;

  object_class->dispose = gtk_css_image_filter_dispose;
}

static void
gtk_css_image_filter_init (GtkCssImageFilter *self)
{
}

GtkCssImage *
gtk_css_image_filter_new (GtkCssImage *image,
                          GtkCssValue *filter)
{
  GtkCssImageFilter *self;

  g_return_val_if_fail (GTK_IS_CSS_IMAGE (image), NULL);

  self = g_object_new (GTK_TYPE_CSS_IMAGE_FILTER, NULL);

  self->image = g_object_ref (image);
  self->filter = gtk_css_value_ref (filter);

  return GTK_CSS_IMAGE (self);
}

