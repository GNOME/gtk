/* GSK - The GTK Scene Kit
 *
 * Copyright 2017 Red Hat, Inc.
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

#ifndef __GSK_PANGO_H__
#define __GSK_PANGO_H__

#include <gsk/gsk.h>

G_BEGIN_DECLS

#define GSK_TYPE_PANGO_RENDERER (gsk_pango_renderer_get_type ())

#define GSK_PANGO_RENDERER(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GSK_TYPE_PANGO_RENDERER, GskPangoRenderer))
#define GSK_IS_PANGO_RENDERER(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSK_TYPE_PANGO_RENDERER))

typedef struct _GskPangoRenderer        GskPangoRenderer;
typedef struct _GskPangoRendererClass   GskPangoRendererClass;

GDK_AVAILABLE_IN_3_92
GType gsk_pango_renderer_get_type (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_3_92
void gsk_pango_show_layout (cairo_t     *cr,
                            PangoLayout *layout);

G_END_DECLS

#endif /* __GSK_PANGO_H__ */
