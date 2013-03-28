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

#ifndef __GTK_CSS_COLOR_VALUE_PRIVATE_H__
#define __GTK_CSS_COLOR_VALUE_PRIVATE_H__

#include "gtkcssparserprivate.h"
#include "gtkcssvalueprivate.h"

G_BEGIN_DECLS


GtkCssValue *   _gtk_css_color_value_new_literal        (const GdkRGBA  *color);
GtkCssValue *   _gtk_css_color_value_new_rgba           (double          red,
                                                         double          green,
                                                         double          blue,
                                                         double          alpha);
GtkCssValue *   _gtk_css_color_value_new_name           (const gchar    *name);
GtkCssValue *   _gtk_css_color_value_new_shade          (GtkCssValue    *color,
                                                         gdouble         factor);
GtkCssValue *   _gtk_css_color_value_new_alpha          (GtkCssValue    *color,
                                                         gdouble         factor);
GtkCssValue *   _gtk_css_color_value_new_mix            (GtkCssValue    *color1,
                                                         GtkCssValue    *color2,
                                                         gdouble         factor);
GtkCssValue *   _gtk_css_color_value_new_win32          (const gchar    *theme_class,
                                                         gint            id);
GtkCssValue *   _gtk_css_color_value_new_current_color  (void);

GtkCssValue *   _gtk_css_color_value_parse              (GtkCssParser   *parser);

GtkCssValue *   _gtk_css_color_value_resolve            (GtkCssValue             *color,
                                                         GtkStyleProviderPrivate *provider,
                                                         GtkCssValue             *current,
                                                         GtkCssDependencies       current_deps,
                                                         GtkCssDependencies      *dependencies,
                                                         GSList                  *cycle_list);


G_END_DECLS

#endif /* __GTK_CSS_COLOR_VALUE_PRIVATE_H__ */
