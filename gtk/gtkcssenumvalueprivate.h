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
 * Authors: Alexander Larsson <alexl@gnome.org>
 */

#ifndef __GTK_CSS_ENUM_VALUE_PRIVATE_H__
#define __GTK_CSS_ENUM_VALUE_PRIVATE_H__

#include "gtkenums.h"
#include "gtkcssparserprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkcssvalueprivate.h"

G_BEGIN_DECLS

GtkCssValue *   _gtk_css_border_style_value_new       (GtkBorderStyle     border_style);
GtkCssValue *   _gtk_css_border_style_value_try_parse (GtkCssParser      *parser);
GtkBorderStyle  _gtk_css_border_style_value_get       (const GtkCssValue *value);

GtkCssValue *   _gtk_css_font_size_value_new          (GtkCssFontSize     size);
GtkCssValue *   _gtk_css_font_size_value_try_parse    (GtkCssParser      *parser);
GtkCssFontSize  _gtk_css_font_size_value_get          (const GtkCssValue *value);
double          _gtk_css_font_size_get_default        (GtkStyleProviderPrivate *provider);

GtkCssValue *   _gtk_css_font_style_value_new         (PangoStyle         style);
GtkCssValue *   _gtk_css_font_style_value_try_parse   (GtkCssParser      *parser);
PangoStyle      _gtk_css_font_style_value_get         (const GtkCssValue *value);

GtkCssValue *   _gtk_css_font_variant_value_new       (PangoVariant       variant);
GtkCssValue *   _gtk_css_font_variant_value_try_parse (GtkCssParser      *parser);
PangoVariant    _gtk_css_font_variant_value_get       (const GtkCssValue *value);

GtkCssValue *   _gtk_css_font_weight_value_new        (PangoWeight        weight);
GtkCssValue *   _gtk_css_font_weight_value_try_parse  (GtkCssParser      *parser);
PangoWeight     _gtk_css_font_weight_value_get        (const GtkCssValue *value);

GtkCssValue *   _gtk_css_area_value_new               (GtkCssArea         area);
GtkCssValue *   _gtk_css_area_value_try_parse         (GtkCssParser      *parser);
GtkCssArea      _gtk_css_area_value_get               (const GtkCssValue *value);

GtkCssValue *   _gtk_css_direction_value_new          (GtkCssDirection    direction);
GtkCssValue *   _gtk_css_direction_value_try_parse    (GtkCssParser      *parser);
GtkCssDirection _gtk_css_direction_value_get          (const GtkCssValue *value);

GtkCssValue *   _gtk_css_play_state_value_new         (GtkCssPlayState    play_state);
GtkCssValue *   _gtk_css_play_state_value_try_parse   (GtkCssParser      *parser);
GtkCssPlayState _gtk_css_play_state_value_get         (const GtkCssValue *value);

GtkCssValue *   _gtk_css_fill_mode_value_new          (GtkCssFillMode     fill_mode);
GtkCssValue *   _gtk_css_fill_mode_value_try_parse    (GtkCssParser      *parser);
GtkCssFillMode  _gtk_css_fill_mode_value_get          (const GtkCssValue *value);

G_END_DECLS

#endif /* __GTK_CSS_ENUM_VALUE_PRIVATE_H__ */
