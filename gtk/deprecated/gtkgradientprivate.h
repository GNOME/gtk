/* GTK - The GIMP Toolkit
 * Copyright (C) 2012 Benjamin Otte <otte@gnome.org>
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

#ifndef __GTK_GRADIENT_PRIVATE_H__
#define __GTK_GRADIENT_PRIVATE_H__

#include "gtkgradient.h"
#include "gtkcsstypesprivate.h"

G_BEGIN_DECLS

cairo_pattern_t *       _gtk_gradient_resolve_full            (GtkGradient             *gradient,
                                                               GtkStyleProviderPrivate *provider,
                                                               GtkCssStyle    *values,
                                                               GtkCssStyle    *parent_values,
                                                               GtkCssDependencies      *dependencies);

GtkGradient *           _gtk_gradient_transition              (GtkGradient             *start,
                                                               GtkGradient             *end,
                                                               guint                    property_id,
                                                               double                   progress);

G_END_DECLS

#endif /* __GTK_STYLE_PROPERTIES_PRIVATE_H__ */
