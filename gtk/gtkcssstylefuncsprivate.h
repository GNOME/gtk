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

#ifndef __GTK_CSS_STYLE_FUNCS_PRIVATE_H__
#define __GTK_CSS_STYLE_FUNCS_PRIVATE_H__

#include "gtkcssparserprivate.h"
#include "gtkstylecontext.h"
#include "gtkcssvalueprivate.h"

G_BEGIN_DECLS

gboolean            _gtk_css_style_funcs_parse_value       (GValue                  *value,
                                                            GtkCssParser            *parser);
void                _gtk_css_style_funcs_print_value       (const GValue            *value,
                                                            GString                 *string);
GtkCssValue *       _gtk_css_style_funcs_compute_value     (GtkStyleProviderPrivate *provider,
                                                            GtkCssComputedValues    *values,
                                                            GtkCssComputedValues    *parent_values,
							    GType                    target_type,
                                                            GtkCssValue             *specified,
                                                            GtkCssDependencies      *dependencies);

G_END_DECLS

#endif /* __GTK_CSS_STYLE_FUNCS_PRIVATE_H__ */
