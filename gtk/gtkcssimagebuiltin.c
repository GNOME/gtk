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

#include "gtkcssimagebuiltinprivate.h"

G_DEFINE_TYPE (GtkCssImageBuiltin, gtk_css_image_builtin, GTK_TYPE_CSS_IMAGE)

static GtkCssImage *the_one_true_image;

static void
gtk_css_image_builtin_draw (GtkCssImage        *image,
                            cairo_t            *cr,
                            double              width,
                            double              height)
{
  /* It's a builtin image, other code will draw things */
}


static gboolean
gtk_css_image_builtin_parse (GtkCssImage  *image,
                             GtkCssParser *parser)
{
  if (!_gtk_css_parser_try (parser, "builtin", TRUE))
    {
      _gtk_css_parser_error (parser, "Expected 'builtin'");
      return FALSE;
    }

  return TRUE;
}

static void
gtk_css_image_builtin_print (GtkCssImage *image,
                             GString     *string)
{
  g_string_append (string, "builtin");
}

static GtkCssImage *
gtk_css_image_builtin_compute (GtkCssImage             *image,
                               guint                    property_id,
                               GtkStyleProviderPrivate *provider,
                               int                      scale,
                               GtkCssComputedValues    *values,
                               GtkCssComputedValues    *parent_values,
                               GtkCssDependencies      *dependencies)
{
  return g_object_ref (image);
}

static gboolean
gtk_css_image_builtin_equal (GtkCssImage *image1,
                             GtkCssImage *image2)
{
  return TRUE;
}

static GtkCssImage *
gtk_css_image_builtin_transition (GtkCssImage *start,
                                  GtkCssImage *end,
                                  guint        property_id,
                                  double       progress)
{
  /* builtin images always look the same, so start == end */
  return g_object_ref (start);
}

static void
gtk_css_image_builtin_dispose (GObject *object)
{
  if (the_one_true_image == GTK_CSS_IMAGE (object))
    the_one_true_image = NULL;

  G_OBJECT_CLASS (gtk_css_image_builtin_parent_class)->dispose (object);
}

static void
gtk_css_image_builtin_class_init (GtkCssImageBuiltinClass *klass)
{
  GtkCssImageClass *image_class = GTK_CSS_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  image_class->draw = gtk_css_image_builtin_draw;
  image_class->parse = gtk_css_image_builtin_parse;
  image_class->print = gtk_css_image_builtin_print;
  image_class->compute = gtk_css_image_builtin_compute;
  image_class->equal = gtk_css_image_builtin_equal;
  image_class->transition = gtk_css_image_builtin_transition;

  object_class->dispose = gtk_css_image_builtin_dispose;
}

static void
gtk_css_image_builtin_init (GtkCssImageBuiltin *builtin)
{
}

GtkCssImage *
gtk_css_image_builtin_new (void)
{
  if (the_one_true_image == NULL)
    the_one_true_image = g_object_new (GTK_TYPE_CSS_IMAGE_BUILTIN, NULL);
  else
    g_object_ref (the_one_true_image);

  return the_one_true_image;
}

