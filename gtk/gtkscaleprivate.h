/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
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

#ifndef __GTK_SCALE_PRIVATE_H__
#define __GTK_SCALE_PRIVATE_H__


#include <gtk/gtkscale.h>


G_BEGIN_DECLS

void    _gtk_scale_clear_layout   (GtkScale *scale);
void    _gtk_scale_get_value_size (GtkScale *scale,
                                   gint     *width,
                                   gint     *height);
gchar * _gtk_scale_format_value   (GtkScale *scale,
                                   gdouble   value);

G_END_DECLS

#endif /* __GTK_SCALE_PRIVATE_H__ */
