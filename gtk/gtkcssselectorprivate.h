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

#ifndef __GTK_CSS_SELECTOR_PRIVATE_H__
#define __GTK_CSS_SELECTOR_PRIVATE_H__

#include "gtk/gtkcssmatcherprivate.h"
#include "gtk/gtkcssparserprivate.h"

G_BEGIN_DECLS

typedef struct _GtkCssSelector GtkCssSelector;

GtkCssSelector *  _gtk_css_selector_parse           (GtkCssParser           *parser);
void              _gtk_css_selector_free            (GtkCssSelector         *selector);

char *            _gtk_css_selector_to_string       (const GtkCssSelector   *selector);
void              _gtk_css_selector_print           (const GtkCssSelector   *selector,
                                                     GString                *str);

GtkStateFlags     _gtk_css_selector_get_state_flags (const GtkCssSelector   *selector);

GtkCssChange      _gtk_css_selector_get_change      (const GtkCssSelector   *selector);
gboolean          _gtk_css_selector_matches         (const GtkCssSelector   *selector,
                                                     const GtkCssMatcher    *matcher);
int               _gtk_css_selector_compare         (const GtkCssSelector   *a,
                                                     const GtkCssSelector   *b);

G_END_DECLS

#endif /* __GTK_CSS_SELECTOR_PRIVATE_H__ */
