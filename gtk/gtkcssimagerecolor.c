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

#include "gtkcssimagerecolorprivate.h"
#include "gtkcssimageprivate.h"
#include "gtkcsspalettevalueprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtksnapshotprivate.h"
#include "gdktextureutilsprivate.h"
#include "svg/gtksvg.h"
#include "gtksymbolicpaintable.h"

#include "gtkstyleproviderprivate.h"

G_DEFINE_TYPE (GtkCssImageRecolor, _gtk_css_image_recolor, GTK_TYPE_CSS_IMAGE)

static void
gtk_css_image_recolor_print (GtkCssImage *image,
                             GString     *string)
{
  GtkCssImageRecolor *recolor = GTK_CSS_IMAGE_RECOLOR (image);
  char *uri;

  g_string_append (string, "-gtk-recolor(url(");
  uri = g_file_get_uri (recolor->file);
  g_string_append (string, uri);
  g_free (uri);
  g_string_append (string, ")");
  if (recolor->palette)
    {
      g_string_append (string, ",");
      gtk_css_value_print (recolor->palette, string);
    }
  g_string_append (string, ")");
}

static void
gtk_css_image_recolor_dispose (GObject *object)
{
  GtkCssImageRecolor *recolor = GTK_CSS_IMAGE_RECOLOR (object);

  g_clear_pointer (&recolor->palette, gtk_css_value_unref);
  g_clear_pointer (&recolor->color, gtk_css_value_unref);
  g_clear_object (&recolor->file);
  g_clear_object (&recolor->paintable);

  G_OBJECT_CLASS (_gtk_css_image_recolor_parent_class)->dispose (object);
}

static void
gtk_css_image_recolor_load_paintable (GtkCssImageRecolor  *recolor,
                                      GError             **error)
{
  char *uri;

  if (recolor->paintable)
    return;

  uri = g_file_get_uri (recolor->file);

  if (g_file_has_uri_scheme (recolor->file, "resource"))
    {
      char *resource_path = g_uri_unescape_string (uri + strlen ("resource://"), NULL);

      if (g_str_has_suffix (resource_path, ".svg"))
        {
          GtkSvg *svg = gtk_svg_new ();

          if (g_str_has_suffix (resource_path, "-symbolic.svg") ||
              g_str_has_suffix (resource_path, "-symbolic-ltr.svg") ||
              g_str_has_suffix (resource_path, "-symbolic-rtl.svg"))
            gtk_svg_set_features (svg, GTK_SVG_DEFAULT_FEATURES | GTK_SVG_TRADITIONAL_SYMBOLIC);

          gtk_svg_load_from_resource (svg, resource_path);
          recolor->paintable = GDK_PAINTABLE (svg);
        }
      else if (g_str_has_suffix (uri, ".symbolic.png"))
        {
          recolor->paintable = GDK_PAINTABLE (gdk_texture_new_from_resource (resource_path));
        }

      g_free (resource_path);
    }
  else
    {
      const char *path = g_file_peek_path (recolor->file);

      if (g_str_has_suffix (path, ".svg"))
        {
          GtkSvg *svg;
          char *data;
          size_t size;

          svg = gtk_svg_new ();

          if (g_str_has_suffix (path, "-symbolic.svg") ||
              g_str_has_suffix (path, "-symbolic-ltr.svg") ||
              g_str_has_suffix (path, "-symbolic-rtl.svg"))
            gtk_svg_set_features (svg, GTK_SVG_DEFAULT_FEATURES | GTK_SVG_TRADITIONAL_SYMBOLIC);


          if (g_file_get_contents (path, &data, &size, NULL))
            {
              GBytes *bytes = g_bytes_new_take (data, size);
              gtk_svg_load_from_bytes (svg, bytes);
              g_bytes_unref (bytes);
            }

          recolor->paintable = GDK_PAINTABLE (svg);
        }
      else if (g_str_has_suffix (uri, ".symbolic.png"))
        {
          recolor->paintable = GDK_PAINTABLE (gdk_texture_new_from_file (recolor->file, NULL));
        }
    }

  if (recolor->paintable)
    {
      recolor->width = gdk_paintable_get_intrinsic_width (recolor->paintable);
      recolor->height = gdk_paintable_get_intrinsic_height (recolor->paintable);
    }

  g_free (uri);
}

static GtkCssImage *
gtk_css_image_recolor_load (GtkCssImageRecolor    *recolor,
                            GtkCssComputeContext  *context,
                            GtkCssValue           *palette,
                            int                    scale,
                            GError               **gerror)
{
  GError *local_error = NULL;
  GtkCssImageRecolor *image;

  image = g_object_new (GTK_TYPE_CSS_IMAGE_RECOLOR, NULL);

  image->file = g_object_ref (recolor->file);
  image->palette = gtk_css_value_ref (palette);
  image->color = gtk_css_value_ref (context->style->core->color);

  gtk_css_image_recolor_load_paintable (recolor, &local_error);

  if (recolor->paintable)
    {
      image->paintable = g_object_ref (recolor->paintable);
      image->width = recolor->width;
      image->height = recolor->height;
    }
  else
    {
      if (gerror)
        {
          char *uri;

          uri = g_file_get_uri (recolor->file);
          g_set_error (gerror,
                       GTK_CSS_PARSER_ERROR,
                       GTK_CSS_PARSER_ERROR_FAILED,
                       "Error loading image '%s': %s", uri, local_error ? local_error->message : "");
          g_free (uri);
       }
    }

  g_clear_error (&local_error);

  return GTK_CSS_IMAGE (image);
}

static void
init_color_matrix (graphene_matrix_t *color_matrix,
                   graphene_vec4_t   *color_offset,
                   const GdkRGBA     *foreground_color,
                   const GdkRGBA     *success_color,
                   const GdkRGBA     *warning_color,
                   const GdkRGBA     *error_color)
{
  const GdkRGBA fg_default = { 0.7450980392156863, 0.7450980392156863, 0.7450980392156863, 1.0};
  const GdkRGBA success_default = { 0.3046921492332342,0.6015716792553597, 0.023437857633325704, 1.0};
  const GdkRGBA warning_default = {0.9570458533607996, 0.47266346227206835, 0.2421911955443656, 1.0 };
  const GdkRGBA error_default = { 0.796887159533074, 0 ,0, 1.0 };
  const GdkRGBA *fg = foreground_color ? foreground_color : &fg_default;
  const GdkRGBA *sc = success_color ? success_color : &success_default;
  const GdkRGBA *wc = warning_color ? warning_color : &warning_default;
  const GdkRGBA *ec = error_color ? error_color : &error_default;

  graphene_matrix_init_from_float (color_matrix,
                                   (float[16]) {
                                     sc->red - fg->red, sc->green - fg->green, sc->blue - fg->blue, 0,
                                     wc->red - fg->red, wc->green - fg->green, wc->blue - fg->blue, 0,
                                     ec->red - fg->red, ec->green - fg->green, ec->blue - fg->blue, 0,
                                     0, 0, 0, fg->alpha
                                   });
  graphene_vec4_init (color_offset, fg->red, fg->green, fg->blue, 0);
}

static void
gtk_css_image_recolor_snapshot (GtkCssImage *image,
                                GtkSnapshot *snapshot,
                                double       width,
                                double       height)
{
  GtkCssImageRecolor *recolor = GTK_CSS_IMAGE_RECOLOR (image);
  GdkRGBA colors[5];
  const char *symbolic[5] = {
    [GTK_SYMBOLIC_COLOR_FOREGROUND] = "foreground",
    [GTK_SYMBOLIC_COLOR_SUCCESS] = "success",
    [GTK_SYMBOLIC_COLOR_WARNING] = "warning",
    [GTK_SYMBOLIC_COLOR_ERROR] = "error",
    [GTK_SYMBOLIC_COLOR_ACCENT] = "accent",
  };

  if (recolor->paintable == NULL)
    return;

  colors[GTK_SYMBOLIC_COLOR_FOREGROUND] = *gtk_css_color_value_get_rgba (recolor->color);

  for (unsigned int i = GTK_SYMBOLIC_COLOR_SUCCESS; i <= GTK_SYMBOLIC_COLOR_ACCENT; i++)
    {
      const GtkCssValue *color = gtk_css_palette_value_get_color (recolor->palette, symbolic[i]);
      if (color)
        colors[i] = *gtk_css_color_value_get_rgba (color);
      else
        colors[i] = colors[GTK_SYMBOLIC_COLOR_FOREGROUND];
    }

  if (GTK_IS_SYMBOLIC_PAINTABLE (recolor->paintable))
    {
      gtk_symbolic_paintable_snapshot_with_weight (GTK_SYMBOLIC_PAINTABLE (recolor->paintable),
                                                   snapshot,
                                                   width, height,
                                                   colors, 5,
                                                   400);
    }
  else
    {
      graphene_matrix_t matrix;
      graphene_vec4_t offset;

      init_color_matrix (&matrix, &offset,
                         &colors[GTK_SYMBOLIC_COLOR_FOREGROUND],
                         &colors[GTK_SYMBOLIC_COLOR_SUCCESS],
                         &colors[GTK_SYMBOLIC_COLOR_WARNING],
                         &colors[GTK_SYMBOLIC_COLOR_ERROR]);

      gtk_snapshot_push_color_matrix (snapshot, &matrix, &offset);
      gdk_paintable_snapshot (recolor->paintable, snapshot, width, height);
      gtk_snapshot_pop (snapshot);
    }
}

static GtkCssImage *
gtk_css_image_recolor_compute (GtkCssImage          *image,
                               guint                 property_id,
                               GtkCssComputeContext *context)
{
  GtkCssImageRecolor *recolor = GTK_CSS_IMAGE_RECOLOR (image);
  GtkCssValue *palette;
  GtkCssImage *img;
  int scale;
  GError *error = NULL;

  scale = gtk_style_provider_get_scale (context->provider);

  if (recolor->palette)
    palette = gtk_css_value_compute (recolor->palette, property_id, context);
  else
    palette = gtk_css_value_ref (context->style->core->icon_palette);

  img = gtk_css_image_recolor_load (recolor, context, palette, scale, &error);

  if (error)
    {
      GtkCssSection *section = gtk_css_style_get_section (context->style, property_id);
      gtk_style_provider_emit_error (context->provider, section, error);
      g_error_free (error);
    }

  gtk_css_value_unref (palette);

  return img;
}

static guint
gtk_css_image_recolor_parse_arg (GtkCssParser *parser,
                                 guint         arg,
                                 gpointer      data)
{
  GtkCssImageRecolor *self = data;

  switch (arg)
  {
    case 0:
      {
        char *url = gtk_css_parser_consume_url (parser);
        if (url == NULL)
          return 0;
        self->file = gtk_css_parser_resolve_url (parser, url);
        g_free (url);
        if (self->file == NULL)
          return 0;
        return 1;
      }

    case 1:
      self->palette = gtk_css_palette_value_parse (parser);
      if (self->palette == NULL)
        return 0;
      return 1;

    default:
      g_assert_not_reached ();
      return 0;
  }
}

static gboolean
gtk_css_image_recolor_parse (GtkCssImage  *image,
                             GtkCssParser *parser)
{
  if (!gtk_css_parser_has_function (parser, "-gtk-recolor"))
    {
      gtk_css_parser_error_syntax (parser, "Expected '-gtk-recolor('");
      return FALSE;
    }

  return gtk_css_parser_consume_function (parser, 1, 2, gtk_css_image_recolor_parse_arg, image);
}

static int
gtk_css_image_recolor_get_width (GtkCssImage *image)
{
  GtkCssImageRecolor *recolor = GTK_CSS_IMAGE_RECOLOR (image);

  gtk_css_image_recolor_load_paintable (recolor, NULL);

  if (recolor->paintable == NULL)
    return 0;

  return (int) recolor->width;
}

static int
gtk_css_image_recolor_get_height (GtkCssImage *image)
{
  GtkCssImageRecolor *recolor = GTK_CSS_IMAGE_RECOLOR (image);

  gtk_css_image_recolor_load_paintable (recolor, NULL);

  if (recolor->paintable == NULL)
    return 0;

  return (int) recolor->height;
}

static gboolean
gtk_css_image_recolor_is_computed (GtkCssImage *image)
{
  GtkCssImageRecolor *recolor = GTK_CSS_IMAGE_RECOLOR (image);

  return recolor->paintable && gtk_css_value_is_computed (recolor->palette);
}

static gboolean
gtk_css_image_recolor_contains_current_color (GtkCssImage *image)
{
  GtkCssImageRecolor *recolor = GTK_CSS_IMAGE_RECOLOR (image);

  if (!recolor->palette || !recolor->color)
    return TRUE;

  return gtk_css_value_contains_current_color (recolor->palette) ||
         gtk_css_value_contains_current_color (recolor->color);
}

static GtkCssImage *
gtk_css_image_recolor_resolve (GtkCssImage          *image,
                               GtkCssComputeContext *context,
                               GtkCssValue          *current_color)
{
  GtkCssImageRecolor *recolor = GTK_CSS_IMAGE_RECOLOR (image);
  GtkCssImageRecolor *img;

  img = g_object_new (GTK_TYPE_CSS_IMAGE_RECOLOR, NULL);

  img->palette = gtk_css_value_resolve (recolor->palette, context, current_color);
  img->color = gtk_css_value_resolve (recolor->color, context, current_color);
  img->file = g_object_ref (recolor->file);
  if (recolor->paintable)
    img->paintable = g_object_ref (recolor->paintable);

  return GTK_CSS_IMAGE (img);
}

static void
_gtk_css_image_recolor_class_init (GtkCssImageRecolorClass *klass)
{
  GtkCssImageClass *image_class = GTK_CSS_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  image_class->get_width = gtk_css_image_recolor_get_width;
  image_class->get_height = gtk_css_image_recolor_get_height;
  image_class->compute = gtk_css_image_recolor_compute;
  image_class->snapshot = gtk_css_image_recolor_snapshot;
  image_class->parse = gtk_css_image_recolor_parse;
  image_class->print = gtk_css_image_recolor_print;
  image_class->is_computed = gtk_css_image_recolor_is_computed;
  image_class->contains_current_color = gtk_css_image_recolor_contains_current_color;
  image_class->resolve = gtk_css_image_recolor_resolve;

  object_class->dispose = gtk_css_image_recolor_dispose;
}

static void
_gtk_css_image_recolor_init (GtkCssImageRecolor *image_recolor)
{
}
