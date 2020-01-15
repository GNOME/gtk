/*
 * Copyright © 2016 Benjamin Otte <otte@gnome.org>
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
 */

#ifndef __GTK_CSS_FONT_SIZE_VALUE_PRIVATE_H__
#define __GTK_CSS_FONT_SIZE_VALUE_PRIVATE_H__

#include "gtkcssnumbervalueprivate.h"

G_BEGIN_DECLS

GtkCssValue *   gtk_css_font_size_value_new        (GtkCssValue    *number);
GtkCssValue *   gtk_css_font_size_value_new_enum   (GtkCssFontSize  font_size);

GtkCssValue *   gtk_css_font_size_value_parse      (GtkCssParser   *parser);
double          gtk_css_font_size_value_get_value  (GtkCssValue      *value);

double          gtk_css_font_size_get_default_px   (GtkStyleProvider *provider,
                                                    GtkCssStyle      *style);

G_END_DECLS

#endif /* __GTK_CSS_FONT_SIZE_VALUE_PRIVATE_H__ */
