/*
 * Copyright (C) 2012 Canonical Ltd
 *
 * This  library is free  software; you can  redistribute it and/or
 * modify it  under  the terms  of the  GNU Lesser  General  Public
 * License  as published  by the Free  Software  Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed  in the hope that it will be useful,
 * but  WITHOUT ANY WARRANTY; without even  the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License  along  with  this library;  if not,  write to  the Free
 * Software Foundation, Inc., 51  Franklin St, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 * Authored by Andrea Cimitan <andrea.cimitan@canonical.com>
 * Original code from Mirco Mueller <mirco.mueller@canonical.com>
 *
 */

#ifndef _GTK_CAIRO_BLUR_H
#define _GTK_CAIRO_BLUR_H

#include <glib.h>
#include <cairo.h>

G_BEGIN_DECLS

void            _gtk_cairo_blur_surface (cairo_surface_t *surface,
                                         double           radius);

G_END_DECLS

#endif /* _GTK_CAIRO_BLUR_H */
