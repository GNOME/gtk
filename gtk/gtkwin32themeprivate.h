/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * Authors: Alexander Larsson <alexl@gnome.org>
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

#ifndef __GTK_WIN32_THEME_PART_H__
#define __GTK_WIN32_THEME_PART_H__

#include "gtkcssparserprivate.h"

G_BEGIN_DECLS

typedef struct _GtkWin32Theme GtkWin32Theme;

#define GTK_WIN32_THEME_SYMBOLIC_COLOR_NAME "-gtk-win32-color"

GtkWin32Theme *         gtk_win32_theme_lookup          (const char     *class_name);
GtkWin32Theme *         gtk_win32_theme_parse           (GtkCssParser   *parser);

GtkWin32Theme *         gtk_win32_theme_ref             (GtkWin32Theme  *theme);
void                    gtk_win32_theme_unref           (GtkWin32Theme  *theme);

gboolean                gtk_win32_theme_equal           (GtkWin32Theme  *theme1,
                                                         GtkWin32Theme  *theme2);

void                    gtk_win32_theme_print           (GtkWin32Theme  *theme,
                                                         GString        *string);

cairo_surface_t *       gtk_win32_theme_create_surface  (GtkWin32Theme *theme,
                                                         int            xp_part,
                                                         int            state,
                                                         int            margins[4],
                                                         int            width,
                                                         int            height,
							 int           *x_offs_out,
							 int           *y_offs_out);

void                    gtk_win32_theme_get_part_border (GtkWin32Theme  *theme,
                                                         int             part,
                                                         int             state,
                                                         GtkBorder      *out_border);
void                    gtk_win32_theme_get_part_size   (GtkWin32Theme  *theme,
                                                         int             part,
                                                         int             state,
                                                         int            *width,
                                                         int            *height);
int                     gtk_win32_theme_get_size        (GtkWin32Theme  *theme,
			                                 int             id);
void                    gtk_win32_theme_get_color       (GtkWin32Theme  *theme,
                                                         gint            id,
                                                         GdkRGBA        *color);

G_END_DECLS

#endif /* __GTK_WIN32_THEME_PART_H__ */
