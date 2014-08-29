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

#ifndef __GTK_CSS_ENGINE_VALUE_PRIVATE_H__
#define __GTK_CSS_ENGINE_VALUE_PRIVATE_H__

#include "gtkcssparserprivate.h"
#include "gtkcssvalueprivate.h"
#include "deprecated/gtkthemingengine.h"

G_BEGIN_DECLS

GtkCssValue *       _gtk_css_engine_value_new           (GtkThemingEngine       *engine);
GtkCssValue *       _gtk_css_engine_value_parse         (GtkCssParser           *parser);

GtkThemingEngine *  _gtk_css_engine_value_get_engine    (const GtkCssValue      *engine);


G_END_DECLS

#endif /* __GTK_CSS_ENGINE_VALUE_PRIVATE_H__ */
