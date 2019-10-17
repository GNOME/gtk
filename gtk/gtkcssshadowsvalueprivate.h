/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * Author: Cosimo Cecchi <cosimoc@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GTK_CSS_SHADOWS_VALUE_H__
#define __GTK_CSS_SHADOWS_VALUE_H__

#include <cairo.h>
#include <pango/pango.h>

#include "gtkborder.h"
#include "gtktypes.h"
#include "gtkcssparserprivate.h"
#include "gtkcssvalueprivate.h"
#include "gtkroundedboxprivate.h"

G_BEGIN_DECLS

GtkCssValue *   _gtk_css_shadows_value_new_none       (void);
GtkCssValue *   _gtk_css_shadows_value_parse          (GtkCssParser             *parser,
                                                       gboolean                  box_shadow_mode);

gboolean        _gtk_css_shadows_value_is_none        (const GtkCssValue        *shadows);

gsize           gtk_css_shadows_value_get_n_shadows   (const GtkCssValue        *shadows);

void            gtk_css_shadows_value_get_shadows    (const GtkCssValue        *shadows,
                                                      GskShadow                *out_shadows);

void            gtk_css_shadows_value_snapshot_outset (const GtkCssValue        *shadows,
                                                       GtkSnapshot              *snapshot,
                                                       const GskRoundedRect     *border_box);
void            gtk_css_shadows_value_snapshot_inset  (const GtkCssValue        *shadows,
                                                       GtkSnapshot              *snapshot,
                                                       const GskRoundedRect     *padding_box);

void            _gtk_css_shadows_value_get_extents    (const GtkCssValue        *shadows,
                                                       GtkBorder                *border);
gboolean        gtk_css_shadows_value_push_snapshot   (const GtkCssValue        *shadows,
                                                       GtkSnapshot              *snapshot);

G_END_DECLS

#endif /* __GTK_CSS_SHADOWS_VALUE_H__ */
