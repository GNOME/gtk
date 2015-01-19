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

#ifndef __GTK_SHADOW_H__
#define __GTK_SHADOW_H__

#include <cairo.h>
#include <pango/pango.h>

#include "gtktypes.h"
#include "gtkcssparserprivate.h"
#include "gtkcssvalueprivate.h"
#include "gtkroundedboxprivate.h"

G_BEGIN_DECLS

GtkCssValue *   _gtk_css_shadow_value_new_for_transition (GtkCssValue           *target);

GtkCssValue *   _gtk_css_shadow_value_parse           (GtkCssParser             *parser,
                                                       gboolean                  box_shadow_mode);

gboolean        _gtk_css_shadow_value_get_inset       (const GtkCssValue        *shadow);

void            _gtk_css_shadow_value_get_geometry    (const GtkCssValue        *shadow,
                                                       gdouble                  *hoffset,
                                                       gdouble                  *voffset,
                                                       gdouble                  *radius,
                                                       gdouble                  *spread);

void            _gtk_css_shadow_value_paint_layout    (const GtkCssValue        *shadow,
                                                       cairo_t                  *cr,
                                                       PangoLayout              *layout);

void            _gtk_css_shadow_value_paint_icon      (const GtkCssValue        *shadow,
					               cairo_t                  *cr);

void            _gtk_css_shadow_value_paint_box       (const GtkCssValue        *shadow,
                                                       cairo_t                  *cr,
                                                       const GtkRoundedBox      *padding_box);

G_END_DECLS

#endif /* __GTK_SHADOW_H__ */
