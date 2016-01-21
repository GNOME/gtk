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
#include "gtkcssimageurlprivate.h"
#include "gtkcssimagesurfaceprivate.h"
#include "gtkcsspalettevalueprivate.h"
#include "gtkcssrgbavalueprivate.h"
#include "gtkiconthemeprivate.h"

#include "gtkstyleproviderprivate.h"

G_DEFINE_TYPE (GtkCssImageRecolor, _gtk_css_image_recolor, GTK_TYPE_CSS_IMAGE_URL)

static void
gtk_css_image_recolor_print (GtkCssImage *image,
                             GString     *string)
{
  GtkCssImageUrl *url = GTK_CSS_IMAGE_URL (image);
  GtkCssImageRecolor *recolor = GTK_CSS_IMAGE_RECOLOR (image);
  char *uri;

  g_string_append (string, "-gtk-recolor(url(");
  uri = g_file_get_uri (url->file);
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

  if (recolor->palette)
    {
      _gtk_css_value_unref (recolor->palette);
      recolor->palette = NULL;
    }

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

static GtkCssImage *
gtk_css_image_recolor_load (GtkCssImageRecolor *recolor,
                            GtkCssStyle        *style,
                            GtkCssValue        *palette)
{
  GtkCssImageUrl *url = GTK_CSS_IMAGE_URL (recolor);
  GtkIconInfo *info;
  GdkRGBA fg, success, warning, error;
  GdkPixbuf *pixbuf;
  GtkCssImage *image;
  GError *gerror = NULL;

  lookup_symbolic_colors (style, palette, &fg, &success, &warning, &error);

  info = gtk_icon_info_new_for_file (url->file, 0, 1);
  pixbuf = gtk_icon_info_load_symbolic (info, &fg, &success, &warning, &error, NULL, &gerror);
  g_object_unref (info);

  if (pixbuf == NULL)
    {
      char *uri;

      /* XXX: Get the error somehow back to the CssProvider */
      uri = g_file_get_uri (url->file);
      g_warning ("Error loading image '%s': %s\n", uri, gerror->message);
      g_error_free (gerror);
      g_free (uri);

      pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (), "image-missing", 24, 0, NULL);
    }

  image = _gtk_css_image_surface_new_for_pixbuf (pixbuf);
  g_object_unref (pixbuf);

  return image;
}

static GtkCssImage *
gtk_css_image_recolor_compute (GtkCssImage             *image,
                               guint                    property_id,
                               GtkStyleProviderPrivate *provider,
                               GtkCssStyle             *style,
                               GtkCssStyle             *parent_style)
{
  GtkCssImageRecolor *recolor = GTK_CSS_IMAGE_RECOLOR (image);
  GtkCssValue *palette;
  GtkCssImage *img;

  if (recolor->palette)
    palette = _gtk_css_value_compute (recolor->palette, property_id, provider, style, parent_style);
  else
    palette = gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_PALETTE);

  img = gtk_css_image_recolor_load (recolor, style, palette);

  _gtk_css_value_unref (palette);

  return img;
}

static gboolean
gtk_css_image_recolor_parse (GtkCssImage  *image,
                             GtkCssParser *parser)
{
  GtkCssImageUrl *url = GTK_CSS_IMAGE_URL (image);
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

  url->file = _gtk_css_parser_read_url (parser);
  if (url->file == NULL)
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

static void
_gtk_css_image_recolor_class_init (GtkCssImageRecolorClass *klass)
{
  GtkCssImageClass *image_class = GTK_CSS_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  image_class->parse = gtk_css_image_recolor_parse;
  image_class->compute = gtk_css_image_recolor_compute;
  image_class->print = gtk_css_image_recolor_print;

  object_class->dispose = gtk_css_image_recolor_dispose;
}

static void
_gtk_css_image_recolor_init (GtkCssImageRecolor *image_recolor)
{
}
