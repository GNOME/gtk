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

#ifndef __GTK_ROUNDED_BOX_PRIVATE_H__
#define __GTK_ROUNDED_BOX_PRIVATE_H__

#include <glib-object.h>
#include <cairo.h>
#include <gsk/gsk.h>
#include <gtk/gtkenums.h>
#include <gtk/gtktypes.h>

#include "gtkcsstypesprivate.h"

G_BEGIN_DECLS

void            _gtk_rounded_box_init_rect                      (GskRoundedRect         *box,
                                                                 double                  x,
                                                                 double                  y,
                                                                 double                  width,
                                                                 double                  height);
void            gtk_rounded_boxes_init_for_style                (GskRoundedRect         *border_box,
                                                                 GskRoundedRect         *padding_box,
                                                                 GskRoundedRect         *content_box,
                                                                 GtkCssStyle            *style,
                                                                 double                  x,
                                                                 double                  y,
                                                                 double                  width,
                                                                 double                  height);

double          _gtk_rounded_box_guess_length                   (const GskRoundedRect   *box,
                                                                 GtkCssSide              side);

void            _gtk_rounded_box_path_side                      (const GskRoundedRect   *box,
                                                                 cairo_t                *cr,
                                                                 GtkCssSide              side);
void            _gtk_rounded_box_path_top                       (const GskRoundedRect   *outer,
                                                                 const GskRoundedRect   *inner,
                                                                 cairo_t                *cr);
void            _gtk_rounded_box_path_right                     (const GskRoundedRect   *outer,
                                                                 const GskRoundedRect   *inner,
                                                                 cairo_t                *cr);
void            _gtk_rounded_box_path_bottom                    (const GskRoundedRect   *outer,
                                                                 const GskRoundedRect   *inner,
                                                                 cairo_t                *cr);
void            _gtk_rounded_box_path_left                      (const GskRoundedRect   *outer,
                                                                 const GskRoundedRect   *inner,
                                                                 cairo_t                *cr);
void            _gtk_rounded_box_clip_path                      (const GskRoundedRect   *box,
                                                                 cairo_t                *cr);

G_END_DECLS

#endif /* __GTK_ROUNDED_BOX_PRIVATE_H__ */
