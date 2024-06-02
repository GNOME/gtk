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

#pragma once

#include <gtk/css/gtkcss.h>
#include "gtk/css/gtkcsstokenizerprivate.h"
#include "gtk/css/gtkcssparserprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkcssvalueprivate.h"
#include "gtkcsscolorprivate.h"

G_BEGIN_DECLS

typedef enum /*< skip >*/ {
  GTK_CSS_POSITIVE_ONLY = (1 << 0),
  GTK_CSS_PARSE_PERCENT = (1 << 1),
  GTK_CSS_PARSE_NUMBER = (1 << 2),
  GTK_CSS_PARSE_LENGTH = (1 << 3),
  GTK_CSS_PARSE_ANGLE = (1 << 4),
  GTK_CSS_PARSE_TIME = (1 << 5)
} GtkCssNumberParseFlags;

typedef struct
{
  /* Context needed when parsing numbers */
  GtkCssValue *color;
  GtkCssColorSpace color_space;
  gboolean legacy_rgb_scale; /* r, g, b must be scaled to 255 */
} GtkCssNumberParseContext;

#define GTK_CSS_PARSE_DIMENSION (GTK_CSS_PARSE_LENGTH|GTK_CSS_PARSE_ANGLE|GTK_CSS_PARSE_TIME)

GtkCssValue *   gtk_css_dimension_value_new         (double                  value,
                                                     GtkCssUnit              unit);

GtkCssValue *   gtk_css_number_value_new            (double                  value,
                                                     GtkCssUnit              unit);
gboolean        gtk_css_number_value_can_parse      (GtkCssParser           *parser);
GtkCssValue *   gtk_css_number_value_parse          (GtkCssParser           *parser,
                                                     GtkCssNumberParseFlags  flags);

GtkCssValue *   gtk_css_number_value_parse_with_context (GtkCssParser             *parser,
                                                         GtkCssNumberParseFlags    flags,
                                                         GtkCssNumberParseContext *context);

GtkCssDimension gtk_css_number_value_get_dimension  (const GtkCssValue      *value) G_GNUC_PURE;
gboolean        gtk_css_number_value_has_percent    (const GtkCssValue      *value) G_GNUC_PURE;
GtkCssValue *   gtk_css_number_value_multiply       (GtkCssValue            *value,
                                                     double                  factor);
GtkCssValue *   gtk_css_number_value_add            (GtkCssValue            *value1,
                                                     GtkCssValue            *value2);
GtkCssValue *   gtk_css_number_value_try_add        (GtkCssValue            *value1,
                                                     GtkCssValue            *value2);
double          gtk_css_number_value_get            (const GtkCssValue      *number,
                                                     double                  one_hundred_percent) G_GNUC_PURE;
double          gtk_css_number_value_get_canonical  (GtkCssValue            *number,
                                                     double                  one_hundred_percent) G_GNUC_PURE;

gboolean        gtk_css_dimension_value_is_zero     (const GtkCssValue      *value) G_GNUC_PURE;

GtkCssValue *   gtk_css_number_value_new_color_component (GtkCssValue      *color,
                                                          GtkCssColorSpace  color_space,
                                                          gboolean          legacy_srgb,
                                                          guint             coord);

enum {
  ROUND_NEAREST,
  ROUND_UP,
  ROUND_DOWN,
  ROUND_TO_ZERO,
};

GtkCssValue *   gtk_css_math_value_new              (guint                    type,
                                                     guint                    mode,
                                                     GtkCssValue            **values,
                                                     guint                    n_values);

G_END_DECLS

