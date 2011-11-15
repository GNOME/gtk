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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GTK_WIN32_THEME_PART_H__
#define __GTK_WIN32_THEME_PART_H__

#include "gtkcssparserprivate.h"

G_BEGIN_DECLS

typedef struct _GtkWin32ThemePart GtkWin32ThemePart;

#define GTK_TYPE_WIN32_THEME_PART (_gtk_win32_theme_part_get_type ())

GType              _gtk_win32_theme_part_get_type  (void) G_GNUC_CONST;

GtkWin32ThemePart *_gtk_win32_theme_part_new       (const char *class, 
						    int xp_part, int state);
GtkWin32ThemePart *_gtk_win32_theme_part_ref       (GtkWin32ThemePart *part);
void               _gtk_win32_theme_part_unref     (GtkWin32ThemePart *part);
int                _gtk_win32_theme_part_parse     (GtkCssParser      *parser, 
						    GFile             *base, 
						    GValue            *value);
cairo_pattern_t   *_gtk_win32_theme_part_render   (GtkWin32ThemePart  *part,
						   int                 width,
						   int                 height);

G_END_DECLS

#endif /* __GTK_WIN32_THEME_PART_H__ */
