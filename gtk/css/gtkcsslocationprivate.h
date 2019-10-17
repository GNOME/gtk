/*
 * Copyright © 2019 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */


#ifndef __GTK_CSS_LOCATION_PRIVATE_H__
#define __GTK_CSS_LOCATION_PRIVATE_H__

#include "gtkcsslocation.h"

G_BEGIN_DECLS

void                    gtk_css_location_init                   (GtkCssLocation         *location);

void                    gtk_css_location_advance                (GtkCssLocation         *location,
                                                                 gsize                   bytes,
                                                                 gsize                   chars);
void                    gtk_css_location_advance_newline        (GtkCssLocation         *location,
                                                                 gboolean                is_windows);

G_END_DECLS

#endif /* __GTK_CSS_LOCATION_PRIVATE_H__ */

