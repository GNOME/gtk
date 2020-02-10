/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
 *               2020 Benjamin Otte <otte@gnome.org>
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

#ifndef __GTK_CSS_STYLE_SHEET_PRIVATE_H__
#define __GTK_CSS_STYLE_SHEET_PRIVATE_H__

#include "gtkcssstylesheet.h"

G_BEGIN_DECLS

gchar *                 gtk_get_theme_dir                               (void);


const gchar *           gtk_css_style_sheet_get_theme_dir               (GtkCssStyleSheet       *self);

void                    gtk_css_style_sheet_set_keep_css_sections       (void);

G_END_DECLS

#endif /* __GTK_CSS_STYLE_SHEET_PRIVATE_H__ */
