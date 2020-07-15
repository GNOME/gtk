/* GTK - The GIMP Toolkit
 * Copyright (C) 2019 Chun-wei Fan <fanc999@yahoo.com.tw>
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

#ifndef __GTK_PANGO_FONT_UTILS_PRIVATE_H__
#define __GTK_PANGO_FONT_UTILS_PRIVATE_H__

#include <pango/pango.h>

#include <ft2build.h>
#include FT_FREETYPE_H

gpointer _gtk_pango_font_init_extra_ft_items (PangoFont    *font);

FT_Face  _gtk_pango_font_get_ft_face         (PangoFont    *font,
                                              PangoFontMap *font_map,
                                              gpointer      ft_items);

void     _gtk_pango_font_release_ft_face     (PangoFont    *font,
                                              gpointer      ft_items);

gboolean _gtk_pango_font_release_ft_items    (gpointer      ft_items);

#endif /* __GTK_PANGO_FONT_UTILS_PRIVATE_H__ */
