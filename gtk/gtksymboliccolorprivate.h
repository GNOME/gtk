/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Benjamin Otte <otte@gnome.org>
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

#ifndef __GTK_SYMBOLIC_COLOR_PRIVATE_H__
#define __GTK_SYMBOLIC_COLOR_PRIVATE_H__

#include "gtk/gtksymboliccolor.h"
#include "gtk/gtkcssparserprivate.h"
#include "gtk/gtkcssvalueprivate.h"

G_BEGIN_DECLS

GtkCssValue *      _gtk_symbolic_color_resolve_full       (GtkSymbolicColor           *color,
                                                           GtkStyleProviderPrivate    *provider,
                                                           GtkCssValue                *current,
                                                           GtkCssDependencies          current_deps,
                                                           GtkCssDependencies         *dependencies);

GtkSymbolicColor * _gtk_symbolic_color_get_current_color  (void);

GtkCssValue *      _gtk_css_symbolic_value_new            (GtkCssParser               *parser);

/* I made these inline functions instead of macros to gain type safety for the arguments passed in. */
static inline GtkSymbolicColor *
_gtk_symbolic_color_new_take_value (GtkCssValue *value)
{
  return (GtkSymbolicColor *) value;
}

static inline GtkCssValue *
_gtk_css_symbolic_value_new_take_symbolic_color (GtkSymbolicColor *color)
{
  return (GtkCssValue *) color;
}

G_END_DECLS

#endif /* __GTK_SYMBOLIC_COLOR_PRIVATE_H__ */
