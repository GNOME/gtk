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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_GRADIENT_H__
#define __GTK_GRADIENT_H__

#include <gdk/gdk.h>
#include <gtk/gtkstylecontext.h>
#include <gtk/gtkstyleproperties.h>
#include <gtk/gtksymboliccolor.h>

G_BEGIN_DECLS

#define GTK_TYPE_GRADIENT (gtk_gradient_get_type ())

GType         gtk_gradient_get_type       (void) G_GNUC_CONST;

GtkGradient * gtk_gradient_new_linear     (gdouble              x0,
                                           gdouble              y0,
                                           gdouble              x1,
                                           gdouble              y1);
GtkGradient * gtk_gradient_new_radial     (gdouble              x0,
                                           gdouble              y0,
                                           gdouble              radius0,
                                           gdouble              x1,
                                           gdouble              y1,
                                           gdouble              radius1);

void          gtk_gradient_add_color_stop (GtkGradient         *gradient,
                                           gdouble              offset,
                                           GtkSymbolicColor    *color);

GtkGradient * gtk_gradient_ref            (GtkGradient         *gradient);
void          gtk_gradient_unref          (GtkGradient         *gradient);

gboolean      gtk_gradient_resolve        (GtkGradient         *gradient,
                                           GtkStyleProperties  *props,
                                           cairo_pattern_t    **resolved_gradient);
cairo_pattern_t *
              gtk_gradient_resolve_for_context
                                          (GtkGradient         *gradient,
                                           GtkStyleContext     *context);

char *        gtk_gradient_to_string      (GtkGradient         *gradient);

G_END_DECLS

#endif /* __GTK_GRADIENT_H__ */
