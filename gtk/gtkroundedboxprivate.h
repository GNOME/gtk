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
#include <gtk/gtkenums.h>
#include <gtk/gtkthemingengine.h>

#include "gtkcsstypesprivate.h"

G_BEGIN_DECLS

typedef struct _GtkRoundedBox GtkRoundedBox;
typedef struct _GtkRoundedBoxCorner GtkRoundedBoxCorner;

struct _GtkRoundedBoxCorner {
  double                   horizontal;
  double                   vertical;
};

struct _GtkRoundedBox {
  /*< private >*/
  cairo_rectangle_t        box;
  GtkRoundedBoxCorner      corner[4];
};

void            _gtk_rounded_box_init_rect                      (GtkRoundedBox       *box,
                                                                 double               x,
                                                                 double               y,
                                                                 double               width,
                                                                 double               height);

void            _gtk_rounded_box_apply_border_radius_for_engine (GtkRoundedBox       *box,
                                                                 GtkThemingEngine    *engine,
                                                                 GtkJunctionSides     junction);
void            _gtk_rounded_box_apply_border_radius_for_context (GtkRoundedBox    *box,
                                                                  GtkStyleContext  *context,
                                                                  GtkJunctionSides  junction);

void            _gtk_rounded_box_grow                           (GtkRoundedBox       *box,
                                                                 double               top,
                                                                 double               right,
                                                                 double               bottom,
                                                                 double               left);
void            _gtk_rounded_box_shrink                         (GtkRoundedBox       *box,
                                                                 double               top,
                                                                 double               right,
                                                                 double               bottom,
                                                                 double               left);
void            _gtk_rounded_box_move                           (GtkRoundedBox       *box,
                                                                 double               dx,
                                                                 double               dy);

double          _gtk_rounded_box_guess_length                   (const GtkRoundedBox *box,
                                                                 GtkCssSide           side);

void            _gtk_rounded_box_path                           (const GtkRoundedBox *box,
                                                                 cairo_t             *cr);
void            _gtk_rounded_box_path_side                      (const GtkRoundedBox *box,
                                                                 cairo_t             *cr,
                                                                 GtkCssSide           side);
void            _gtk_rounded_box_path_top                       (const GtkRoundedBox *outer,
                                                                 const GtkRoundedBox *inner,
                                                                 cairo_t             *cr);
void            _gtk_rounded_box_path_right                     (const GtkRoundedBox *outer,
                                                                 const GtkRoundedBox *inner,
                                                                 cairo_t             *cr);
void            _gtk_rounded_box_path_bottom                    (const GtkRoundedBox *outer,
                                                                 const GtkRoundedBox *inner,
                                                                 cairo_t             *cr);
void            _gtk_rounded_box_path_left                      (const GtkRoundedBox *outer,
                                                                 const GtkRoundedBox *inner,
                                                                 cairo_t             *cr);
void            _gtk_rounded_box_clip_path                      (const GtkRoundedBox *box,
                                                                 cairo_t             *cr);

G_END_DECLS

#endif /* __GTK_ROUNDED_BOX_PRIVATE_H__ */
