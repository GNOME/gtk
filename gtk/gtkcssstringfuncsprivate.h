/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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

#ifndef __GTK_CSS_STRINGFUNCS_PRIVATE_H__
#define __GTK_CSS_STRINGFUNCS_PRIVATE_H__

#include <gtk/gtksymboliccolor.h>

G_BEGIN_DECLS

gboolean                _gtk_css_value_from_string        (GValue        *value,
                                                           GFile         *base,
                                                           const char    *string,
                                                           GError       **error);
char *                  _gtk_css_value_to_string          (const GValue  *value);

GtkSymbolicColor *      _gtk_css_parse_symbolic_color     (const char    *str,
                                                           GError       **error);

GFile *                 _gtk_css_parse_url                (GFile         *base,
                                                           const char    *str,
                                                           char         **end,
                                                           GError       **error);

G_END_DECLS

#endif /* __GTK_CSS_STRINGFUNCS_PRIVATE_H__ */
