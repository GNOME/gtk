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

void        gtk_render_content_path  (GtkStyleContext   *context,
                                      cairo_t           *cr,
                                      double             x,
                                      double             y,
                                      double             width,
                                      double             height);

#endif /* __GTK_RENDER_PRIVATE_H__ */
