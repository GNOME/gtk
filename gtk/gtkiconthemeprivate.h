/* GTK - The GIMP Toolkit
 * Copyright (C) 2015 Benjamin Otte <otte@gnome.org>
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

#ifndef __GTK_ICON_THEME_PRIVATE_H__
#define __GTK_ICON_THEME_PRIVATE_H__

#include <gtk/gtkicontheme.h>
#include <gtk/gtkcssstyleprivate.h>

void        gtk_icon_theme_lookup_symbolic_colors       (GtkCssStyle    *style,
                                                         GdkRGBA        *color_out,
                                                         GdkRGBA        *success_out,
                                                         GdkRGBA        *warning_out,
                                                         GdkRGBA        *error_out);

GtkIconInfo *gtk_icon_info_new_for_file (GFile *file,
                                         gint   size,
                                         gint   scale);

GdkPixbuf * gtk_icon_theme_color_symbolic_pixbuf (GdkPixbuf     *symbolic,
                                                  const GdkRGBA *fg_color,
                                                  const GdkRGBA *success_color,
                                                  const GdkRGBA *warning_color,
                                                  const GdkRGBA *error_color);


#endif /* __GTK_ICON_THEME_PRIVATE_H__ */
