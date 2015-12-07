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

#ifndef __GTK_ICON_FACTORY_PRIVATE_H__
#define __GTK_ICON_FACTORY_PRIVATE_H__

#include <gtk/deprecated/gtkiconfactory.h>

GList *     _gtk_icon_factory_list_ids                  (void);
void        _gtk_icon_factory_ensure_default_icons      (void);

GdkPixbuf * gtk_icon_set_render_icon_pixbuf_for_scale   (GtkIconSet             *icon_set,
                                                         GtkCssStyle            *style,
                                                         GtkTextDirection        direction,
                                                         GtkIconSize             size,
                                                         gint                    scale);

#endif /* __GTK_ICON_FACTORY_PRIVATE_H__ */
