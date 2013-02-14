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

#ifdef G_OS_WIN32

#include <windows.h>

typedef HANDLE HTHEME;

#else /* !G_OS_WIN32 */

typedef void * HTHEME;

#endif /* G_OS_WIN32 */

G_BEGIN_DECLS

#define GTK_WIN32_THEME_SYMBOLIC_COLOR_NAME "-gtk-win32-color"

HTHEME             _gtk_win32_lookup_htheme_by_classname (const char  *classname);
cairo_surface_t *  _gtk_win32_theme_part_create_surface  (HTHEME       theme,
                                                          int          xp_part,
                                                          int          state,
                                                          int          margins[4],
                                                          int          width,
                                                          int          height,
							  int         *x_offs_out,
							  int         *y_offs_out);

int                _gtk_win32_theme_int_parse     (GtkCssParser      *parser,
						   int               *value);
gboolean           _gtk_win32_theme_color_resolve (const char        *theme_class,
						   gint               id,
						   GdkRGBA           *color);
const char *      _gtk_win32_theme_get_default    (void);

G_END_DECLS

#endif /* __GTK_WIN32_THEME_PART_H__ */
