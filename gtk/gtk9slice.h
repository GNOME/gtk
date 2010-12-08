/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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

#ifndef __GTK_9SLICE_H__
#define __GTK_9SLICE_H__

#include "gtktimeline.h"

G_BEGIN_DECLS

/* Dummy typedefs */
typedef struct Gtk9Slice Gtk9Slice;

#define GTK_TYPE_9SLICE (_gtk_9slice_get_type ())

typedef enum {
  GTK_SLICE_REPEAT,
  GTK_SLICE_STRETCH
} GtkSliceSideModifier;

GType       _gtk_9slice_get_type (void) G_GNUC_CONST;

Gtk9Slice * _gtk_9slice_new      (GdkPixbuf            *pixbuf,
                                  gdouble               distance_top,
                                  gdouble               distance_bottom,
                                  gdouble               distance_left,
                                  gdouble               distance_right,
                                  GtkSliceSideModifier  horizontal_modifier,
                                  GtkSliceSideModifier  vertical_modifier);

Gtk9Slice * _gtk_9slice_ref      (Gtk9Slice            *slice);
void        _gtk_9slice_unref    (Gtk9Slice            *slice);

void        _gtk_9slice_render   (Gtk9Slice            *slice,
                                  cairo_t              *cr,
                                  gdouble               x,
                                  gdouble               y,
                                  gdouble               width,
                                  gdouble               height);

G_END_DECLS

#endif /* __GTK_9SLICE_H__ */
