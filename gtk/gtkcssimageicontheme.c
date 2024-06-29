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

#include "gtk/css/gtkcssserializerprivate.h"
#include "gtksettingsprivate.h"
#include "gtksnapshot.h"
#include "gtkstyleproviderprivate.h"
#include "gtksymbolicpaintable.h"
#include "gtkiconthemeprivate.h"
#include "gtkcsscolorvalueprivate.h"
#include "gtkcsspalettevalueprivate.h"

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
  GtkIconPaintable *icon;
  double icon_width, icon_height;
  int size;
  double x, y;
  GdkRGBA colors[4];

  size = floor (MIN (width, height));
  if (size <= 0)
    return;

  if (size == icon_theme->cached_size &&
      icon_theme->cached_icon != NULL)
    {
      icon = icon_theme->cached_icon;
    }
  else
    {
      icon = gtk_icon_theme_lookup_icon (icon_theme->icon_theme,
                                         icon_theme->name,
                                         NULL,
                                         size,
                                         icon_theme->scale,
                                         GTK_TEXT_DIR_NONE,
                                         0);

      g_assert (icon != NULL);

      g_clear_object (&icon_theme->cached_icon);

      icon_theme->cached_size = size;
      icon_theme->cached_icon = icon;
    }

  icon_width = (double) MIN (gdk_paintable_get_intrinsic_width (GDK_PAINTABLE (icon)), width);
  icon_height = (double) MIN (gdk_paintable_get_intrinsic_height (GDK_PAINTABLE (icon)), height);

  x = (width - icon_width) / 2;
  y = (height - icon_height) / 2;

  if (x != 0 || y != 0)
    {
      gtk_snapshot_save (snapshot);
      gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x, y));
    }

  for (guint i = 0; i < 4; i++)
    colors[i] = *gtk_css_color_value_get_rgba (icon_theme->colors[i]);

  gtk_symbolic_paintable_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (icon),
                                            snapshot,
                                            icon_width,
                                            icon_height,
                                            colors,
                                            G_N_ELEMENTS (icon_theme->colors));
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
  gtk_css_print_string (string, icon_theme->name, FALSE);
  g_string_append (string, ")");
}

static GtkCssImage *
gtk_css_image_icon_theme_compute (GtkCssImage          *image,
                                  guint                 property_id,
                                  GtkCssComputeContext *context)
{
  GtkCssImageIconTheme *icon_theme = GTK_CSS_IMAGE_ICON_THEME (image);
  GtkCssImageIconTheme *copy;
  GtkSettings *settings;
  GdkDisplay *display;
  const char *names[4] = { NULL, "success", "warning", "error" };

  copy = g_object_new (GTK_TYPE_CSS_IMAGE_ICON_THEME, NULL);
  copy->name = g_strdup (icon_theme->name);
  settings = gtk_style_provider_get_settings (context->provider);
  display = _gtk_settings_get_display (settings);
  copy->icon_theme = gtk_icon_theme_get_for_display (display);
  copy->serial = gtk_icon_theme_get_serial (copy->icon_theme);
  copy->scale = gtk_style_provider_get_scale (context->provider);

  for (guint i = 0; i < 4; i++)
    {
      GtkCssValue *color = NULL;

      if (names[i])
        color = gtk_css_palette_value_get_color (context->style->core->icon_palette, names[i]);
      if (color)
        copy->colors[i] = gtk_css_value_ref (color);
      else
        copy->colors[i] = gtk_css_value_ref (context->style->core->color);
    }

  return GTK_CSS_IMAGE (copy);
}

static gboolean
gtk_css_image_icon_theme_equal (GtkCssImage *image1,
                                GtkCssImage *image2)
{
  GtkCssImageIconTheme *icon_theme1 = (GtkCssImageIconTheme *) image1;
  GtkCssImageIconTheme *icon_theme2 = (GtkCssImageIconTheme *) image2;

  return icon_theme1->serial == icon_theme2->serial &&
         icon_theme1->icon_theme == icon_theme2->icon_theme &&
         g_str_equal (icon_theme1->name, icon_theme2->name);
}

static void
gtk_css_image_icon_theme_dispose (GObject *object)
{
  GtkCssImageIconTheme *icon_theme = GTK_CSS_IMAGE_ICON_THEME (object);

  g_free (icon_theme->name);
  icon_theme->name = NULL;

  g_clear_object (&icon_theme->cached_icon);
  g_clear_pointer (&icon_theme->colors[0], gtk_css_value_unref);
  g_clear_pointer (&icon_theme->colors[1], gtk_css_value_unref);
  g_clear_pointer (&icon_theme->colors[2], gtk_css_value_unref);
  g_clear_pointer (&icon_theme->colors[3], gtk_css_value_unref);

  G_OBJECT_CLASS (_gtk_css_image_icon_theme_parent_class)->dispose (object);
}

static gboolean
gtk_css_image_icon_theme_contains_current_color (GtkCssImage *image)
{
  GtkCssImageIconTheme *icon_theme = GTK_CSS_IMAGE_ICON_THEME (image);

  for (guint i = 0; i < 4; i++)
    {
      if (!icon_theme->colors[i] ||
          gtk_css_value_contains_current_color (icon_theme->colors[i]))
        return TRUE;
    }

  return FALSE;
}

static GtkCssImage *
gtk_css_image_icon_theme_resolve (GtkCssImage          *image,
                                  GtkCssComputeContext *context,
                                  GtkCssValue          *current)
{
  GtkCssImageIconTheme *icon_theme = GTK_CSS_IMAGE_ICON_THEME (image);
  GtkCssImageIconTheme *copy;

  if (!gtk_css_image_icon_theme_contains_current_color (image))
    return g_object_ref (image);

  copy = g_object_new (GTK_TYPE_CSS_IMAGE_ICON_THEME, NULL);
  copy->name = g_strdup (icon_theme->name);
  copy->icon_theme = icon_theme->icon_theme;
  copy->serial = icon_theme->serial;
  copy->scale = icon_theme->scale;

  for (guint i = 0; i < 4; i++)
    copy->colors[i] = gtk_css_value_resolve (icon_theme->colors[i], context, current);

  return GTK_CSS_IMAGE (copy);
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
  image_class->contains_current_color = gtk_css_image_icon_theme_contains_current_color;
  image_class->resolve = gtk_css_image_icon_theme_resolve;
  object_class->dispose = gtk_css_image_icon_theme_dispose;
}

static void
_gtk_css_image_icon_theme_init (GtkCssImageIconTheme *icon_theme)
{
  icon_theme->icon_theme = gtk_icon_theme_get_for_display (gdk_display_get_default ());
  icon_theme->scale = 1;
  icon_theme->cached_size = -1;
  icon_theme->cached_icon = NULL;
}

