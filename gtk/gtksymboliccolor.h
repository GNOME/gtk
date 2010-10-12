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

#ifndef __GTK_SYMBOLIC_COLOR_H__
#define __GTK_SYMBOLIC_COLOR_H__

#include <gdk/gdk.h>

G_BEGIN_DECLS

/* Dummy typedefs */
typedef struct GtkSymbolicColor GtkSymbolicColor;
typedef struct GtkGradient GtkGradient;

#define GTK_TYPE_SYMBOLIC_COLOR (gtk_symbolic_color_get_type ())
#define GTK_TYPE_GRADIENT (gtk_gradient_get_type ())

GType gtk_symbolic_color_get_type (void) G_GNUC_CONST;
GType gtk_gradient_get_type (void) G_GNUC_CONST;

GtkSymbolicColor * gtk_symbolic_color_new_literal (GdkColor         *color);
GtkSymbolicColor * gtk_symbolic_color_new_name    (const gchar      *name);
GtkSymbolicColor * gtk_symbolic_color_new_shade   (GtkSymbolicColor *color,
                                                   gdouble           factor);
GtkSymbolicColor * gtk_symbolic_color_new_mix     (GtkSymbolicColor *color1,
                                                   GtkSymbolicColor *color2,
                                                   gdouble           factor);

GtkSymbolicColor * gtk_symbolic_color_ref   (GtkSymbolicColor *color);
void               gtk_symbolic_color_unref (GtkSymbolicColor *color);

GtkGradient * gtk_gradient_new_linear (gdouble x0,
                                       gdouble y0,
                                       gdouble x1,
                                       gdouble y1);
GtkGradient * gtk_gradient_new_radial (gdouble x0,
				       gdouble y0,
				       gdouble radius0,
				       gdouble x1,
				       gdouble y1,
				       gdouble radius1);

void gtk_gradient_add_color_stop (GtkGradient      *gradient,
                                  gdouble           offset,
                                  GtkSymbolicColor *color);

GtkGradient * gtk_gradient_ref   (GtkGradient *gradient);
void          gtk_gradient_unref (GtkGradient *gradient);

G_END_DECLS

#endif /* __GTK_SYMBOLIC_COLOR_H__ */
