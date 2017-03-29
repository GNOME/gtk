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

#include "gtkcssiconthemevalueprivate.h"
#include "gtkcssrgbavalueprivate.h"
#include "gtksettingsprivate.h"
#include "gtksnapshot.h"
#include "gtkstyleproviderprivate.h"
#include "gtkiconthemeprivate.h"

G_DEFINE_TYPE (GtkCssImageIconTheme, _gtk_css_image_icon_theme, GTK_TYPE_CSS_IMAGE)

static double
gtk_css_image_icon_theme_get_aspect_ratio (GtkCssImage *image)
{
  /* icon theme icons only take a single size when requesting, so we insist on being square */
  return 1.0;
}

static void
gtk_css_image_icon_theme_snapshot (GtkCssImage *image,
                                   GtkSnapshot *snapshot,
                                   double       width,
                                   double       height)
{
  GtkCssImageIconTheme *icon_theme = GTK_CSS_IMAGE_ICON_THEME (image);
  GskTexture *texture;
  double texture_width, texture_height;
  gint size;

  size = floor (MIN (width, height));
  if (size <= 0)
    return;

  if (size == icon_theme->cached_size &&
      icon_theme->cached_texture != NULL)
    {
      texture = icon_theme->cached_texture;
    }
  else
    {
      GError *error = NULL;
      GtkIconInfo *icon_info;
      GdkPixbuf *pixbuf;

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
                                            &icon_theme->success,
                                            &icon_theme->warning,
                                            &icon_theme->error,
                                            NULL,
                                            &error);
      if (pixbuf == NULL)
        {
          /* XXX: render missing icon image here? */
          g_error_free (error);
          return;
        }

      texture = gsk_texture_new_for_pixbuf (pixbuf);

      g_clear_object (&icon_theme->cached_texture);
      icon_theme->cached_size = size;
      icon_theme->cached_texture = texture;

      g_object_unref (pixbuf);
      g_object_unref (icon_info);
    }

  texture_width = (double) gsk_texture_get_width (texture) / icon_theme->scale;
  texture_height = (double) gsk_texture_get_height (texture) / icon_theme->scale;

  gtk_snapshot_append_texture (snapshot,
                               texture,
                               &GRAPHENE_RECT_INIT(
                                   (width - texture_width) / 2.0,
                                   (height - texture_height) / 2.0,
                                   texture_width,
                                   texture_height
                               ),
                               "CssImageIconTheme<%s@%d>", icon_theme->name, icon_theme->scale);
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
                                  GtkCssStyle             *parent_style)
{
  GtkCssImageIconTheme *icon_theme = GTK_CSS_IMAGE_ICON_THEME (image);
  GtkCssImageIconTheme *copy;

  copy = g_object_new (GTK_TYPE_CSS_IMAGE_ICON_THEME, NULL);
  copy->name = g_strdup (icon_theme->name);
  copy->icon_theme = gtk_css_icon_theme_value_get_icon_theme (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_ICON_THEME));
  copy->scale = _gtk_style_provider_private_get_scale (provider);
  gtk_icon_theme_lookup_symbolic_colors (style, &copy->color, &copy->success, &copy->warning, &copy->error);

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

  g_clear_object (&icon_theme->cached_texture);

  G_OBJECT_CLASS (_gtk_css_image_icon_theme_parent_class)->dispose (object);
}

static void
_gtk_css_image_icon_theme_class_init (GtkCssImageIconThemeClass *klass)
{
  GtkCssImageClass *image_class = GTK_CSS_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  image_class->get_aspect_ratio = gtk_css_image_icon_theme_get_aspect_ratio;
  image_class->snapshot = gtk_css_image_icon_theme_snapshot;
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
  icon_theme->cached_size = -1;
  icon_theme->cached_texture = NULL;
}

