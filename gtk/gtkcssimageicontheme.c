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
  GtkIcon *icon_info;
  double icon_width, icon_height;
  gint size;
  double x, y;

  size = floor (MIN (width, height));
  if (size <= 0)
    return;

  if (size == icon_theme->cached_size &&
      icon_theme->cached_icon != NULL)
    {
      icon_info = icon_theme->cached_icon;
    }
  else
    {
      icon_info = gtk_icon_theme_lookup_icon (icon_theme->icon_theme,
                                              icon_theme->name,
                                              size,
                                              icon_theme->scale,
                                              GTK_ICON_LOOKUP_USE_BUILTIN);
      if (icon_info == NULL)
        icon_info = gtk_icon_theme_lookup_icon (icon_theme->icon_theme,
                                                "image-missing",
                                                size, icon_theme->scale,
                                                GTK_ICON_LOOKUP_USE_BUILTIN | GTK_ICON_LOOKUP_GENERIC_FALLBACK);

      g_assert (icon_info != NULL);

      g_clear_object (&icon_theme->cached_icon);

      icon_theme->cached_size = size;
      icon_theme->cached_icon = icon_info;
    }

  icon_width = (double) MIN (gdk_paintable_get_intrinsic_width (GDK_PAINTABLE (icon_info)), width);
  icon_height = (double) MIN (gdk_paintable_get_intrinsic_height (GDK_PAINTABLE (icon_info)), height);

  x = (width - icon_width) / 2;
  y = (height - icon_height) / 2;

  if (x != 0 || y != 0)
    {
      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, y));
    }
  gtk_icon_snapshot_with_colors (icon_info, snapshot,
                                      icon_width,
                                      icon_height,
                                      &icon_theme->color,
                                      &icon_theme->success,
                                      &icon_theme->warning,
                                      &icon_theme->error);
  if (x != 0 || y != 0)
    gtk_snapshot_restore (snapshot);
}

static guint
gtk_css_image_icon_theme_parse_arg (GtkCssParser *parser,
                                    guint         arg,
                                    gpointer      data)
{
  GtkCssImageIconTheme *icon_theme = data;

  icon_theme->name = gtk_css_parser_consume_string (parser);
  if (icon_theme->name == NULL)
    return 0;

  return 1;
}

static gboolean
gtk_css_image_icon_theme_parse (GtkCssImage  *image,
                                GtkCssParser *parser)
{
  if (!gtk_css_parser_has_function (parser, "-gtk-icontheme"))
    {
      gtk_css_parser_error_syntax (parser, "Expected '-gtk-icontheme('");
      return FALSE;
    }

  return gtk_css_parser_consume_function (parser, 1, 1, gtk_css_image_icon_theme_parse_arg, image);
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
gtk_css_image_icon_theme_compute (GtkCssImage      *image,
                                  guint             property_id,
                                  GtkStyleProvider *provider,
                                  GtkCssStyle      *style,
                                  GtkCssStyle      *parent_style)
{
  GtkCssImageIconTheme *icon_theme = GTK_CSS_IMAGE_ICON_THEME (image);
  GtkCssImageIconTheme *copy;

  copy = g_object_new (GTK_TYPE_CSS_IMAGE_ICON_THEME, NULL);
  copy->name = g_strdup (icon_theme->name);
  copy->icon_theme = gtk_css_icon_theme_value_get_icon_theme (style->core->icon_theme);
  copy->scale = gtk_style_provider_get_scale (provider);
  gtk_icon_theme_lookup_symbolic_colors (style, &copy->color, &copy->success, &copy->warning, &copy->error);

  return GTK_CSS_IMAGE (copy);
}

static gboolean
gtk_css_image_icon_theme_equal (GtkCssImage *image1,
                                GtkCssImage *image2)
{
  GtkCssImageIconTheme *icon_theme1 = (GtkCssImageIconTheme *) image1;
  GtkCssImageIconTheme *icon_theme2 = (GtkCssImageIconTheme *) image2;

  return g_str_equal (icon_theme1->name, icon_theme2->name);
}

static void
gtk_css_image_icon_theme_dispose (GObject *object)
{
  GtkCssImageIconTheme *icon_theme = GTK_CSS_IMAGE_ICON_THEME (object);

  g_free (icon_theme->name);
  icon_theme->name = NULL;

  g_clear_object (&icon_theme->cached_icon);

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
  icon_theme->cached_icon = NULL;
}

