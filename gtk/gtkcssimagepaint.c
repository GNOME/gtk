/*
 * Copyright © 2018 Benjamin Otte
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

#include <string.h>

#include "gtkcssimagepaintprivate.h"

#include "gtkcssimageinvalidprivate.h"
#include "gtkcssimagepaintableprivate.h"
#include "gtkstyleproviderprivate.h"

G_DEFINE_TYPE (GtkCssImagePaint, gtk_css_image_paint, GTK_TYPE_CSS_IMAGE)

static void
gtk_css_image_paint_snapshot (GtkCssImage *image,
                              GtkSnapshot *snapshot,
                              double       width,
                              double       height)
{
}

static GtkCssImage *
gtk_css_image_paint_compute (GtkCssImage      *image,
                             guint             property_id,
                             GtkStyleProvider *provider,
                             GtkCssStyle      *style,
                             GtkCssStyle      *parent_style)
{
  GtkCssImagePaint *paint = GTK_CSS_IMAGE_PAINT (image);
  GdkPaintable *paintable, *static_paintable;
  GtkCssImage *result;

  paintable = gtk_style_provider_get_paint (provider, paint->name);
  if (paintable == NULL)
    return gtk_css_image_invalid_new ();

  static_paintable = gdk_paintable_get_current_image (paintable);
  result = gtk_css_image_paintable_new (paintable, static_paintable);
  g_object_unref (static_paintable);

  return result;
}

static gboolean
gtk_css_image_paint_equal (GtkCssImage *image1,
                           GtkCssImage *image2)
{
  GtkCssImagePaint *paint1 = GTK_CSS_IMAGE_PAINT (image1);
  GtkCssImagePaint *paint2 = GTK_CSS_IMAGE_PAINT (image2);

  return g_str_equal (paint1->name, paint2->name);
}

static gboolean
gtk_css_image_paint_parse (GtkCssImage  *image,
                           GtkCssParser *parser)
{
  GtkCssImagePaint *paint = GTK_CSS_IMAGE_PAINT (image);

  if (!_gtk_css_parser_try (parser, "-gtk-paint", TRUE))
    {
      _gtk_css_parser_error (parser, "'-gtk-paint'");
      return FALSE;
    }

  if (!_gtk_css_parser_try (parser, "(", TRUE))
    {
      _gtk_css_parser_error (parser, "Expected '(' after '-gtk-paint'");
      return FALSE;
    }

  paint->name = _gtk_css_parser_try_ident (parser, TRUE);
  if (paint->name == NULL)
    {
      _gtk_css_parser_error (parser, "Expected the name of the paint");
      return FALSE;
    }

  if (!_gtk_css_parser_try (parser, ")", TRUE))
    {
      _gtk_css_parser_error (parser,
                             "Expected ')' at end of '-gtk-paint'");
      return FALSE;
    }

  return TRUE;
}

static void
gtk_css_image_paint_print (GtkCssImage *image,
                           GString     *string)
{
  GtkCssImagePaint *paint = GTK_CSS_IMAGE_PAINT (image);

  g_string_append (string, "paint (");
  g_string_append (string, paint->name);
  g_string_append (string, ")");
}

static void
gtk_css_image_paint_dispose (GObject *object)
{
  GtkCssImagePaint *paint = GTK_CSS_IMAGE_PAINT (object);

  g_clear_pointer (&paint->name, g_free);

  G_OBJECT_CLASS (gtk_css_image_paint_parent_class)->dispose (object);
}

static void
gtk_css_image_paint_class_init (GtkCssImagePaintClass *klass)
{
  GtkCssImageClass *image_class = GTK_CSS_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  image_class->compute = gtk_css_image_paint_compute;
  image_class->snapshot = gtk_css_image_paint_snapshot;
  image_class->parse = gtk_css_image_paint_parse;
  image_class->print = gtk_css_image_paint_print;
  image_class->equal = gtk_css_image_paint_equal;

  object_class->dispose = gtk_css_image_paint_dispose;
}

static void
gtk_css_image_paint_init (GtkCssImagePaint *image_paint)
{
}

