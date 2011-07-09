/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * Authors: Carlos Garnacho <carlosg@gnome.org>
 *          Cosimo Cecchi <cosimoc@gnome.org>
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

#ifndef __GTK_BORDER_IMAGE_H__
#define __GTK_BORDER_IMAGE_H__

#include "gtkborder.h"
#include "gtkgradient.h"
#include "gtkstyleproperties.h"
#include "gtkthemingengine.h"
#include "gtkcsstypesprivate.h"

G_BEGIN_DECLS

typedef struct _GtkBorderImage GtkBorderImage;

#define GTK_TYPE_BORDER_IMAGE (_gtk_border_image_get_type ())

GType             _gtk_border_image_get_type         (void) G_GNUC_CONST;

GtkBorderImage *  _gtk_border_image_new              (cairo_pattern_t      *source,
                                                      GtkBorder            *slice,
                                                      GtkBorder            *width,
                                                      GtkCssBorderImageRepeat *repeat);
GtkBorderImage *  _gtk_border_image_new_for_gradient (GtkGradient          *gradient,
                                                      GtkBorder            *slice,
                                                      GtkBorder            *width,
                                                      GtkCssBorderImageRepeat *repeat);

GtkBorderImage *  _gtk_border_image_ref              (GtkBorderImage       *image);
void              _gtk_border_image_unref            (GtkBorderImage       *image);

void              _gtk_border_image_render           (GtkBorderImage       *image,
                                                      GtkBorder            *border_width,
                                                      cairo_t              *cr,
                                                      gdouble               x,
                                                      gdouble               y,
                                                      gdouble               width,
                                                      gdouble               height);

GParameter *      _gtk_border_image_unpack           (const GValue         *value,
                                                      guint                *n_params);
void              _gtk_border_image_pack             (GValue               *value,
                                                      GtkStyleProperties   *props,
                                                      GtkStateFlags         state);

G_END_DECLS

#endif /* __GTK_BORDER_IMAGE_H__ */
