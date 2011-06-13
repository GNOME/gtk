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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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

struct _GtkRoundedBox {
  /*< private >*/
  cairo_rectangle_t  box;
  GtkCssBorderRadius border_radius;
};

void            _gtk_rounded_box_init_rect                      (GtkRoundedBox       *box,
                                                                 double               x,
                                                                 double               y,
                                                                 double               width,
                                                                 double               height);

void            _gtk_rounded_box_apply_border_radius            (GtkRoundedBox       *box,
                                                                 GtkThemingEngine    *engine,
                                                                 GtkStateFlags        state,
                                                                 GtkJunctionSides     junction);
void            _gtk_rounded_box_shrink                         (GtkRoundedBox       *box,
                                                                 double               top,
                                                                 double               right,
                                                                 double               bottom,
                                                                 double               left);
void            _gtk_rounded_box_move                           (GtkRoundedBox       *box,
                                                                 double               dx,
                                                                 double               dy);

void            _gtk_rounded_box_path                           (const GtkRoundedBox *box,
                                                                 cairo_t             *cr);
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
