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

#include "gtkcssimageiconthemeprivate.h"

#include <math.h>

#include "gtkcssrgbavalueprivate.h"
#include "gtksettingsprivate.h"
#include "gtkstyleproviderprivate.h"

G_DEFINE_TYPE (GtkCssImageIconTheme, _gtk_css_image_icon_theme, GTK_TYPE_CSS_IMAGE)

static double
gtk_css_image_icon_theme_get_aspect_ratio (GtkCssImage *image)
{
  /* icon theme icons only take a single size when requesting, so we insist on being square */
  return 1.0;
}

static void
gtk_css_image_icon_theme_draw (GtkCssImage        *image,
                               cairo_t            *cr,
                               double              width,
                               double              height)
{
  GtkCssImageIconTheme *icon_theme = GTK_CSS_IMAGE_ICON_THEME (image);
  GError *error = NULL;
  GtkIconInfo *icon_info;
  GdkPixbuf *pixbuf;
  gint size;

  size = floor (MIN (width, height));
  if (size <= 0)
    return;

  icon_info = gtk_icon_theme_lookup_icon_for_scale (icon_theme->icon_theme,
                                                    icon_theme->name,
                                                    size,
                                                    icon_theme->scale,
                                                    GTK_ICON_LOOKUP_USE_BUILTIN);
  if (icon_info == NULL)
    {
      /* XXX: render missing icon image here? */
      return;
    }

  pixbuf = gtk_icon_info_load_symbolic (icon_info,
                                        &icon_theme->color,
                                        NULL, NULL, NULL,
                                        NULL,
                                        &error);
  if (pixbuf == NULL)
    {
      /* XXX: render missing icon image here? */
      g_error_free (error);
      return;
    }

  cairo_translate (cr, width / 2.0, height / 2.0);
  cairo_scale (cr, 1.0 / icon_theme->scale, 1.0 / icon_theme->scale);
  gdk_cairo_set_source_pixbuf (cr,
                               pixbuf,
                               - gdk_pixbuf_get_width (pixbuf) / 2.0,
                               - gdk_pixbuf_get_height (pixbuf) / 2.0);
  cairo_paint (cr);

  g_object_unref (pixbuf);
  g_object_unref (icon_info);
}


static gboolean
gtk_css_image_icon_theme_parse (GtkCssImage  *image,
                                GtkCssParser *parser)
{
  GtkCssImageIconTheme *icon_theme = GTK_CSS_IMAGE_ICON_THEME (image);

  if (!_gtk_css_parser_try (parser, "-gtk-icontheme(", TRUE))
    {
      _gtk_css_parser_error (parser, "Expected '-gtk-icontheme('");
      return FALSE;
    }

  icon_theme->name = _gtk_css_parser_read_string (parser);
  if (icon_theme->name == NULL)
    return FALSE;

  if (!_gtk_css_parser_try (parser, ")", TRUE))
    {
      _gtk_css_parser_error (parser, "Missing closing bracket at end of '-gtk-icontheme'");
      return FALSE;
    }

  return TRUE;
}

static void
gtk_css_image_icon_theme_print (GtkCssImage *image,
                                GString     *string)
{
  GtkCssImageIconTheme *icon_theme = GTK_CSS_IMAGE_ICON_THEME (image);

  g_string_append (string, "-gtk-icontheme(");
  _gtk_css_print_string (string, icon_theme->name);
  g_string_append (string, ")");
}

static GtkCssImage *
gtk_css_image_icon_theme_compute (GtkCssImage             *image,
                                  guint                    property_id,
                                  GtkStyleProviderPrivate *provider,
                                  GtkCssStyle             *style,
                                  GtkCssStyle             *parent_style,
                                  GtkCssDependencies      *dependencies)
{
  GtkCssImageIconTheme *icon_theme = GTK_CSS_IMAGE_ICON_THEME (image);
  GtkCssImageIconTheme *copy;
  GtkSettings *settings;
  GdkScreen *screen;

  settings = _gtk_style_provider_private_get_settings (provider);
  if (settings == NULL)
    screen = gdk_screen_get_default ();
  else
    screen = _gtk_settings_get_screen (settings);

  copy = g_object_new (GTK_TYPE_CSS_IMAGE_ICON_THEME, NULL);
  copy->name = g_strdup (icon_theme->name);
  copy->icon_theme = gtk_icon_theme_get_for_screen (screen);
  copy->scale = _gtk_style_provider_private_get_scale (provider);
  copy->color = *_gtk_css_rgba_value_get_rgba (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_COLOR));

  *dependencies = GTK_CSS_DEPENDS_ON_COLOR;

  return GTK_CSS_IMAGE (copy);
}

static gboolean
gtk_css_image_icon_theme_equal (GtkCssImage *image1,
                                GtkCssImage *image2)
{
  GtkCssImageIconTheme *icon_theme1 = GTK_CSS_IMAGE_ICON_THEME (image1);
  GtkCssImageIconTheme *icon_theme2 = GTK_CSS_IMAGE_ICON_THEME (image2);

  return g_str_equal (icon_theme1->name, icon_theme2->name);
}

static void
gtk_css_image_icon_theme_dispose (GObject *object)
{
  GtkCssImageIconTheme *icon_theme = GTK_CSS_IMAGE_ICON_THEME (object);

  g_free (icon_theme->name);
  icon_theme->name = NULL;

  G_OBJECT_CLASS (_gtk_css_image_icon_theme_parent_class)->dispose (object);
}

static void
_gtk_css_image_icon_theme_class_init (GtkCssImageIconThemeClass *klass)
{
  GtkCssImageClass *image_class = GTK_CSS_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  image_class->get_aspect_ratio = gtk_css_image_icon_theme_get_aspect_ratio;
  image_class->draw = gtk_css_image_icon_theme_draw;
  image_class->parse = gtk_css_image_icon_theme_parse;
  image_class->print = gtk_css_image_icon_theme_print;
  image_class->compute = gtk_css_image_icon_theme_compute;
  image_class->equal = gtk_css_image_icon_theme_equal;

  object_class->dispose = gtk_css_image_icon_theme_dispose;
}

static void
_gtk_css_image_icon_theme_init (GtkCssImageIconTheme *icon_theme)
{
  icon_theme->icon_theme = gtk_icon_theme_get_default ();
  icon_theme->scale = 1;
}

