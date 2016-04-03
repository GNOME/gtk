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

#include "gtkcssimagewin32private.h"

#include "gtkcssprovider.h"
#include "gtkcssnumbervalueprivate.h"

G_DEFINE_TYPE (GtkCssImageWin32, _gtk_css_image_win32, GTK_TYPE_CSS_IMAGE)

static void
gtk_css_image_win32_draw (GtkCssImage        *image,
                          cairo_t            *cr,
                          double              width,
                          double              height)
{
  GtkCssImageWin32 *wimage = GTK_CSS_IMAGE_WIN32 (image);
  cairo_surface_t *surface;
  int dx, dy;

  surface = gtk_win32_theme_create_surface (wimage->theme, wimage->part, wimage->state, wimage->margins,
				            width, height, &dx, &dy);
  
  if (wimage->state2 >= 0)
    {
      cairo_surface_t *surface2;
      cairo_t *cr2;
      int dx2, dy2;

      surface2 = gtk_win32_theme_create_surface (wimage->theme, wimage->part2, wimage->state2, wimage->margins,
						 width, height, &dx2, &dy2);

      cr2 = cairo_create (surface);

      cairo_set_source_surface (cr2, surface2, dx2 - dx, dy2-dy);
      cairo_paint_with_alpha (cr2, wimage->over_alpha);

      cairo_destroy (cr2);

      cairo_surface_destroy (surface2);
    }

  cairo_set_source_surface (cr, surface, dx, dy);
  cairo_pattern_set_extend (cairo_get_source (cr), CAIRO_EXTEND_NONE);
  cairo_rectangle (cr, 0, 0, width, height);
  cairo_fill (cr);

  cairo_surface_destroy (surface);
}

static gboolean
gtk_css_image_win32_parse (GtkCssImage  *image,
                           GtkCssParser *parser)
{
  GtkCssImageWin32 *wimage = GTK_CSS_IMAGE_WIN32 (image);

  if (!_gtk_css_parser_try (parser, "-gtk-win32-theme-part", TRUE))
    {
      _gtk_css_parser_error (parser, "'-gtk-win32-theme-part'");
      return FALSE;
    }
  
  if (!_gtk_css_parser_try (parser, "(", TRUE))
    {
      _gtk_css_parser_error (parser,
                             "Expected '(' after '-gtk-win32-theme-part'");
      return FALSE;
    }
  
  wimage->theme = gtk_win32_theme_parse (parser);
  if (wimage->theme == NULL)
    return FALSE;

  if (! _gtk_css_parser_try (parser, ",", TRUE))
    {
      _gtk_css_parser_error (parser, "Expected ','");
      return FALSE;
    }

  if (!_gtk_css_parser_try_int (parser, &wimage->part))
    {
      _gtk_css_parser_error (parser, "Expected a valid integer value");
      return FALSE;
    }

  if (! _gtk_css_parser_try (parser, ",", TRUE))
    {
      _gtk_css_parser_error (parser, "Expected ','");
      return FALSE;
    }

  if (!_gtk_css_parser_try_int (parser, &wimage->state))
    {
      _gtk_css_parser_error (parser, "Expected a valid integer value");
      return FALSE;
    }

  while ( _gtk_css_parser_try (parser, ",", TRUE))
    {
      if ( _gtk_css_parser_try (parser, "over", TRUE))
        {
          if (!_gtk_css_parser_try (parser, "(", TRUE))
            {
              _gtk_css_parser_error (parser,
                                     "Expected '(' after 'over'");
              return FALSE;
            }

          if (!_gtk_css_parser_try_int (parser, &wimage->part2))
            {
              _gtk_css_parser_error (parser, "Expected a valid integer value");
              return FALSE;
            }

          if (! _gtk_css_parser_try (parser, ",", TRUE))
            {
              _gtk_css_parser_error (parser, "Expected ','");
              return FALSE;
            }

          if (!_gtk_css_parser_try_int (parser, &wimage->state2))
            {
              _gtk_css_parser_error (parser, "Expected a valid integer value");
              return FALSE;
            }

          if ( _gtk_css_parser_try (parser, ",", TRUE))
            {
              if (!_gtk_css_parser_try_double (parser, &wimage->over_alpha))
                {
                  _gtk_css_parser_error (parser, "Expected a valid double value");
                  return FALSE;
                }
            }

          if (!_gtk_css_parser_try (parser, ")", TRUE))
            {
              _gtk_css_parser_error (parser,
                                     "Expected ')' at end of 'over'");
              return FALSE;
            }
        }
      else if ( _gtk_css_parser_try (parser, "margins", TRUE))
        {
          guint i;

          if (!_gtk_css_parser_try (parser, "(", TRUE))
            {
              _gtk_css_parser_error (parser,
                                     "Expected '(' after 'margins'");
              return FALSE;
            }

          for (i = 0; i < 4; i++)
            {
              if (!_gtk_css_parser_try_int (parser, &wimage->margins[i]))
                break;
            }
          
          if (i == 0)
            {
              _gtk_css_parser_error (parser, "Expected valid margins");
              return FALSE;
            }

          if (i == 1)
            wimage->margins[1] = wimage->margins[0];
          if (i <= 2)
            wimage->margins[2] = wimage->margins[1];
          if (i <= 3)
            wimage->margins[3] = wimage->margins[2];
          
          if (!_gtk_css_parser_try (parser, ")", TRUE))
            {
              _gtk_css_parser_error (parser,
                                     "Expected ')' at end of 'margins'");
              return FALSE;
            }
        }
      else
        {
          _gtk_css_parser_error (parser,
                                 "Expected identifier");
          return FALSE;
        }
    }

  if (!_gtk_css_parser_try (parser, ")", TRUE))
    {
      _gtk_css_parser_error (parser,
			     "Expected ')'");
      return FALSE;
    }
  
  return TRUE;
}

static guint
gtk_css_image_win32_token_parse_over_argument (GtkCssTokenSource *source,
                                               guint              arg,
                                               gpointer           data)
{
  GtkCssImageWin32 *wimage = GTK_CSS_IMAGE_WIN32 (data);

  if (arg == 0)
    {
      if (!gtk_css_token_source_consume_integer (source, &wimage->part2))
        return 0;
      return 1;
    }
  else if (arg == 1)
    {
      if (!gtk_css_token_source_consume_integer (source, &wimage->state2))
        return 0;
      return 1;
    }
  else if (arg == 2)
    {
      if (!gtk_css_token_source_consume_number (source, &wimage->over_alpha))
        return 0;
      return 1;
    }
  else
    {
      g_assert_not_reached ();
      return 0;
    }
}

static guint
gtk_css_image_win32_token_parse_margins_argument (GtkCssTokenSource *source,
                                                  guint              arg,
                                                  gpointer           data)
{
  GtkCssImageWin32 *wimage = GTK_CSS_IMAGE_WIN32 (data);
  guint i;

  for (i = 0; i < 4; i++)
    {
      if (i > 0 && !gtk_css_number_value_check_token (gtk_css_token_source_get_token (source)))
        break;

      if (!gtk_css_token_source_consume_integer (source, &wimage->margins[i]))
        return 0;
    }

  for (; i < 4; i++)
    wimage->margins[i] = wimage->margins[i - 1];

  return 1;
}

static guint
gtk_css_image_win32_token_parse_argument (GtkCssTokenSource *source,
                                          guint              arg,
                                          gpointer           data)
{
  GtkCssImageWin32 *wimage = GTK_CSS_IMAGE_WIN32 (data);

  if (arg == 0)
    {
      wimage->theme = gtk_win32_theme_token_parse (source);
      if (wimage->theme == NULL)
        return 0;
      return 1;
    }
  else if (arg == 1)
    {
      if (!gtk_css_token_source_consume_integer (source, &wimage->part))
        return 0;
      return 1;
    }
  else if (arg == 2)
    {
      if (!gtk_css_token_source_consume_integer (source, &wimage->state))
        return 0;
      return 1;
    }
  else
    {
      const GtkCssToken *token;

      token = gtk_css_token_source_get_token (source);
      if (gtk_css_token_is_function (token, "over"))
        {
          return gtk_css_token_source_consume_function (source, 
                                                        2, 3,
                                                        gtk_css_image_win32_token_parse_over_argument,
                                                        wimage) ? 1 : 0;
        }
      else if (gtk_css_token_is_function (token, "margins"))
        {
          return gtk_css_token_source_consume_function (source, 
                                                        1, 1,
                                                        gtk_css_image_win32_token_parse_margins_argument,
                                                        wimage) ? 1 : 0;
        }
      else
        {
          gtk_css_token_source_error (source, "Expected over() or margins()");
          return 0;
        }
    }
}

static gboolean
gtk_css_image_win32_token_parse (GtkCssImage       *image,
                                 GtkCssTokenSource *source)
{
  const GtkCssToken *token;

  token = gtk_css_token_source_get_token (source);
  if (!gtk_css_token_is_function (token, "-gtk-win32-theme-part"))
    {
      gtk_css_token_source_error (source, "Expected '-gtk-win32-theme-part('");
      gtk_css_token_source_consume_all (source);
      return FALSE;
    }

  return gtk_css_token_source_consume_function (source, 
                                                3, G_MAXUINT,
                                                gtk_css_image_win32_token_parse_argument,
                                                image);
}

static void
gtk_css_image_win32_print (GtkCssImage *image,
                           GString     *string)
{
  GtkCssImageWin32 *wimage = GTK_CSS_IMAGE_WIN32 (image);

  g_string_append (string, "-gtk-win32-theme-part(");
  gtk_win32_theme_print (wimage->theme, string);
  g_string_append_printf (string, ", %d, %d)", wimage->part, wimage->state);
}

static void
gtk_css_image_win32_finalize (GObject *object)
{
  GtkCssImageWin32 *wimage = GTK_CSS_IMAGE_WIN32 (object);

  if (wimage->theme)
    gtk_win32_theme_unref (wimage->theme);

  G_OBJECT_CLASS (_gtk_css_image_win32_parent_class)->finalize (object);
}

static void
_gtk_css_image_win32_class_init (GtkCssImageWin32Class *klass)
{
  GtkCssImageClass *image_class = GTK_CSS_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_css_image_win32_finalize;

  image_class->draw = gtk_css_image_win32_draw;
  image_class->parse = gtk_css_image_win32_parse;
  image_class->token_parse = gtk_css_image_win32_token_parse;
  image_class->print = gtk_css_image_win32_print;
}

static void
_gtk_css_image_win32_init (GtkCssImageWin32 *wimage)
{
  wimage->over_alpha = 1.0;
  wimage->part2 = -1;
  wimage->state2 = -1;
}

