/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/css/gtkcss.h>
#include "gtk/css/gtkcsstokenizerprivate.h"
#include "gtk/css/gtkcssparserprivate.h"
#include "gtkcssvalueprivate.h"

G_BEGIN_DECLS

typedef enum {
  GTK_CSS_COLOR_SPACE_SRGB,
  GTK_CSS_COLOR_SPACE_SRGB_LINEAR,
} GtkCssColorSpace;

GtkCssValue *   gtk_css_color_value_new_transparent     (void) G_GNUC_PURE;
GtkCssValue *   gtk_css_color_value_new_white           (void) G_GNUC_PURE;
GtkCssValue *   gtk_css_color_value_new_literal         (const GdkRGBA  *color) G_GNUC_PURE;
GtkCssValue *   gtk_css_value_value_new_color           (GtkCssColorSpace color_space,
                                                         float            values[4]) G_GNUC_PURE;
GtkCssValue *   gtk_css_color_value_new_name            (const char     *name) G_GNUC_PURE;
GtkCssValue *   gtk_css_color_value_new_shade           (GtkCssValue    *color,
                                                         double          factor) G_GNUC_PURE;
GtkCssValue *   gtk_css_color_value_new_alpha           (GtkCssValue    *color,
                                                         double          factor) G_GNUC_PURE;
GtkCssValue *   gtk_css_color_value_new_mix             (GtkCssValue    *color1,
                                                         GtkCssValue    *color2,
                                                         double          factor) G_GNUC_PURE;
GtkCssValue *   gtk_css_color_value_new_current_color   (void) G_GNUC_PURE;
GtkCssValue *   gtk_css_color_value_new_oklab           (float           L,
                                                         float           a,
                                                         float           b,
                                                         float           alpha) G_GNUC_PURE;
GtkCssValue *   gtk_css_color_value_new_oklch           (float           L,
                                                         float           C,
                                                         float           H,
                                                         float           alpha) G_GNUC_PURE;

gboolean        gtk_css_color_value_can_parse           (GtkCssParser   *parser);
GtkCssValue *   gtk_css_color_value_parse               (GtkCssParser   *parser);

GtkCssValue *   gtk_css_color_value_resolve             (GtkCssValue      *color,
                                                         GtkStyleProvider *provider,
                                                         GtkCssValue      *current);
const GdkRGBA * gtk_css_color_value_get_rgba            (const GtkCssValue *color) G_GNUC_CONST;


G_END_DECLS

