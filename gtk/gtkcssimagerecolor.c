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
#include "gtkcssimagesurfaceprivate.h"
#include "gtkcsspalettevalueprivate.h"
#include "gtkcssrgbavalueprivate.h"
#include "gtkiconthemeprivate.h"
#include "gdkpixbufutilsprivate.h"

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
      _gtk_css_value_print (recolor->palette, string);
    }
  g_string_append (string, ")");
}

static void
gtk_css_image_recolor_dispose (GObject *object)
{
  GtkCssImageRecolor *recolor = GTK_CSS_IMAGE_RECOLOR (object);

  g_clear_pointer (&recolor->palette, _gtk_css_value_unref);
  g_clear_object (&recolor->file);
  g_clear_object (&recolor->texture);

  G_OBJECT_CLASS (_gtk_css_image_recolor_parent_class)->dispose (object);
}

static void
lookup_symbolic_colors (GtkCssStyle *style,
                        GtkCssValue *palette,
                        GdkRGBA     *color_out,
                        GdkRGBA     *success_out,
                        GdkRGBA     *warning_out,
                        GdkRGBA     *error_out)
{
  GtkCssValue *color;
  const GdkRGBA *lookup;

  color = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_COLOR);
  *color_out = *_gtk_css_rgba_value_get_rgba (color);

  lookup = gtk_css_palette_value_get_color (palette, "success");
  if (lookup)
    *success_out = *lookup;
  else
    *success_out = *color_out;

  lookup = gtk_css_palette_value_get_color (palette, "warning");
  if (lookup)
    *warning_out = *lookup;
  else
    *warning_out = *color_out;

  lookup = gtk_css_palette_value_get_color (palette, "error");
  if (lookup)
    *error_out = *lookup;
  else
    *error_out = *color_out;
}

static void
gtk_css_image_recolor_load_texture (GtkCssImageRecolor  *recolor,
                                    GError             **error)
{
  char *uri;

  if (recolor->texture)
    return;

  uri = g_file_get_uri (recolor->file);

  if (g_file_has_uri_scheme (recolor->file, "resource"))
    {
      char *resource_path = g_uri_unescape_string (uri + strlen ("resource://"), NULL);

      if (g_str_has_suffix (uri, ".symbolic.png"))
        recolor->texture = gdk_texture_new_from_resource (resource_path);
      else
        recolor->texture = gtk_make_symbolic_texture_from_resource (resource_path, 0, 0, 1.0, NULL);

      g_free (resource_path);
    }
  else
    {
      if (g_str_has_suffix (uri, ".symbolic.png"))
        recolor->texture = gdk_texture_new_from_file (recolor->file, NULL);
      else
        recolor->texture = gtk_make_symbolic_texture_from_file (recolor->file, 0, 0, 1.0, NULL);
    }

  g_free (uri);
}

static GtkCssImage *
gtk_css_image_recolor_load (GtkCssImageRecolor  *recolor,
                            GtkCssStyle         *style,
                            GtkCssValue         *palette,
                            gint                 scale,
                            GError             **gerror)
{
  GError *local_error = NULL;
  GtkCssImageRecolor *image;

  image = g_object_new (GTK_TYPE_CSS_IMAGE_RECOLOR, NULL);

  lookup_symbolic_colors (style, palette, &image->color, &image->success, &image->warning, &image->error);
  gtk_css_image_recolor_load_texture (recolor, &local_error);

  image->file = g_object_ref (recolor->file);

  if (recolor->texture)
    image->texture = g_object_ref (recolor->texture);
  else
    {
      if (gerror)
        {
          char *uri;

          uri = g_file_get_uri (recolor->file);
          g_set_error (gerror,
                       GTK_CSS_PROVIDER_ERROR,
                       GTK_CSS_PROVIDER_ERROR_FAILED,
                       "Error loading image '%s': %s", uri, local_error->message);
          g_free (uri);
       }
    }

  g_clear_error (&local_error);

  return GTK_CSS_IMAGE (image);
}

static void
gtk_css_image_recolor_snapshot (GtkCssImage *image,
                                GtkSnapshot *snapshot,
                                double       width,
                                double       height)
{
  GtkCssImageRecolor *recolor = GTK_CSS_IMAGE_RECOLOR (image);
  graphene_matrix_t matrix;
  graphene_vec4_t offset;
  GdkRGBA fg = recolor->color;
  GdkRGBA sc = recolor->success;
  GdkRGBA wc = recolor->warning;
  GdkRGBA ec = recolor->error;

  if (recolor->texture == NULL)
    return;

  graphene_matrix_init_from_float (&matrix,
          (float[16]) {
                       sc.red - fg.red, sc.green - fg.green, sc.blue - fg.blue, 0,
                       wc.red - fg.red, wc.green - fg.green, wc.blue - fg.blue, 0,
                       ec.red - fg.red, ec.green - fg.green, ec.blue - fg.blue, 0,
                       0, 0, 0, fg.alpha
                      });
  graphene_vec4_init (&offset, fg.red, fg.green, fg.blue, 0);
  gtk_snapshot_push_color_matrix (snapshot, &matrix, &offset, "Recolor");

  gtk_snapshot_append_texture (snapshot,
                               recolor->texture,
                               &GRAPHENE_RECT_INIT (0, 0, width, height),
                               "Recolor Image %dx%d",
                               gdk_texture_get_width (recolor->texture),
                               gdk_texture_get_height (recolor->texture));

  gtk_snapshot_pop (snapshot);
}

static GtkCssImage *
gtk_css_image_recolor_compute (GtkCssImage      *image,
                               guint             property_id,
                               GtkStyleProvider *provider,
                               GtkCssStyle      *style,
                               GtkCssStyle      *parent_style)
{
  GtkCssImageRecolor *recolor = GTK_CSS_IMAGE_RECOLOR (image);
  GtkCssValue *palette;
  GtkCssImage *img;
  int scale;
  GError *error = NULL;

  scale = gtk_style_provider_get_scale (provider);

  if (recolor->palette)
    palette = _gtk_css_value_compute (recolor->palette, property_id, provider, style, parent_style);
  else
    palette = _gtk_css_value_ref (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_PALETTE));

  img = gtk_css_image_recolor_load (recolor, style, palette, scale, &error);

  if (error)
    {
      GtkCssSection *section = gtk_css_style_get_section (style, property_id);
      gtk_style_provider_emit_error (provider, section, error);
      g_error_free (error);
    }

  _gtk_css_value_unref (palette);

  return img;
}

static gboolean
gtk_css_image_recolor_parse (GtkCssImage  *image,
                             GtkCssParser *parser)
{
  GtkCssImageRecolor *recolor = GTK_CSS_IMAGE_RECOLOR (image);

  if (!_gtk_css_parser_try (parser, "-gtk-recolor", TRUE))
    {
      _gtk_css_parser_error (parser, "'-gtk-recolor'");
      return FALSE;
    }

  if (!_gtk_css_parser_try (parser, "(", TRUE))
    {
      _gtk_css_parser_error (parser, "Expected '(' after '-gtk-recolor'");
      return FALSE;
    }

  recolor->file = _gtk_css_parser_read_url (parser);
  if (recolor->file == NULL)
    {
      _gtk_css_parser_error (parser, "Expected a url here");
      return FALSE;
    }

  if ( _gtk_css_parser_try (parser, ",", TRUE))
    {
      recolor->palette = gtk_css_palette_value_parse (parser);
      if (recolor->palette == NULL)
        {
          _gtk_css_parser_error (parser, "A palette is required here");
          return FALSE;
        }
    }

  if (!_gtk_css_parser_try (parser, ")", TRUE))
    {
      _gtk_css_parser_error (parser,
                             "Expected ')' at end of '-gtk-recolor'");
      return FALSE;
    }

  return TRUE;
}

static int
gtk_css_image_recolor_get_width (GtkCssImage *image)
{
  GtkCssImageRecolor *recolor = GTK_CSS_IMAGE_RECOLOR (image);

  gtk_css_image_recolor_load_texture (recolor, NULL);

  if (recolor->texture == NULL)
    return 0;

  return gdk_texture_get_width (recolor->texture);
}

static int
gtk_css_image_recolor_get_height (GtkCssImage *image)
{
  GtkCssImageRecolor *recolor = GTK_CSS_IMAGE_RECOLOR (image);

  gtk_css_image_recolor_load_texture (recolor, NULL);

  if (recolor->texture == NULL)
    return 0;

  return gdk_texture_get_height (recolor->texture);
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

  object_class->dispose = gtk_css_image_recolor_dispose;
}

static void
_gtk_css_image_recolor_init (GtkCssImageRecolor *image_recolor)
{
}
