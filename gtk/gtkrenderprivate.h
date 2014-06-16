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

#ifndef __GTK_RENDER_PRIVATE_H__
#define __GTK_RENDER_PRIVATE_H__

#include <cairo.h>
#include <pango/pango.h>
#include <gdk/gdk.h>

#include <gtk/gtkenums.h>
#include <gtk/gtktypes.h>

void        gtk_do_render_check    (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height);
void        gtk_do_render_option   (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height);
void        gtk_do_render_arrow    (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              angle,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              size);
void        gtk_do_render_background (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height);
void        gtk_do_render_frame    (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height);
void        gtk_do_render_expander (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height);
void        gtk_do_render_focus    (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height);
void        gtk_do_render_layout   (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    PangoLayout         *layout);
void        gtk_do_render_line     (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x0,
                                    gdouble              y0,
                                    gdouble              x1,
                                    gdouble              y1);
void        gtk_do_render_slider   (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height,
                                    GtkOrientation       orientation);
void        gtk_do_render_frame_gap (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height,
                                    GtkPositionType      gap_side,
                                    gdouble              xy0_gap,
                                    gdouble              xy1_gap);
void        gtk_do_render_extension(GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height,
                                    GtkPositionType      gap_side);
void        gtk_do_render_handle   (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height);
void        gtk_do_render_activity (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    gdouble              x,
                                    gdouble              y,
                                    gdouble              width,
                                    gdouble              height);
GdkPixbuf * gtk_do_render_icon_pixbuf (GtkStyleContext     *context,
                                    const GtkIconSource *source,
                                    GtkIconSize          size);
void        gtk_do_render_icon     (GtkStyleContext     *context,
                                    cairo_t             *cr,
                                    GdkPixbuf           *pixbuf,
                                    gdouble              x,
                                    gdouble              y);
void        gtk_do_render_icon_surface (GtkStyleContext *context,
				    cairo_t             *cr,
				    cairo_surface_t     *surface,
				    gdouble              x,
				    gdouble              y);

void        gtk_render_paint_spinner (cairo_t           *cr,
                                    gdouble              radius,
                                    gdouble              progress);

#endif /* __GTK_RENDER_PRIVATE_H__ */
