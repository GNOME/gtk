/*
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_COLOR_CHOOSER_PRIVATE_H__
#define __GTK_COLOR_CHOOSER_PRIVATE_H__

#include "gtkcolorchooser.h"

G_BEGIN_DECLS

void _gtk_color_chooser_color_activated (GtkColorChooser *chooser,
                                         const GdkRGBA   *color);

cairo_pattern_t * _gtk_color_chooser_get_checkered_pattern (void);

G_END_DECLS

#endif /* ! __GTK_COLOR_CHOOSER_PRIVATE_H__ */
