/*
 * Copyright © 2015 Endless Mobile, Inc.
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
 * Authors: Cosimo Cecchi <cosimoc@gnome.org>
 */

#ifndef __GTK_ICON_PRIVATE_H__
#define __GTK_ICON_PRIVATE_H__

#include "gtkwidget.h"
#include "gtkcsstypesprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_ICON (gtk_icon_get_type ())

G_DECLARE_FINAL_TYPE (GtkIcon, gtk_icon, GTK, ICON, GtkWidget)

GtkWidget *  gtk_icon_new                    (const char *css_name);

void         gtk_icon_set_image              (GtkIcon                *self,
                                              GtkCssImageBuiltinType  image);

void         gtk_icon_set_css_name           (GtkIcon    *self,
                                              const char *css_name);

G_END_DECLS

#endif /* __GTK_ICON_PRIVATE_H__ */
