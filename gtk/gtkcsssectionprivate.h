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

#ifndef __GTK_CSS_SECTION_PRIVATE_H__
#define __GTK_CSS_SECTION_PRIVATE_H__

#include "gtkcsssection.h"

#include "gtkcssparserprivate.h"

G_BEGIN_DECLS

GtkCssSection *    _gtk_css_section_new                (GtkCssSection        *parent,
                                                        GtkCssSectionType     type,
                                                        GtkCssParser         *parser);
GtkCssSection *    _gtk_css_section_new_for_file       (GtkCssSectionType     type,
                                                        GFile                *file);

void               _gtk_css_section_end                (GtkCssSection        *section);

void               _gtk_css_section_print              (const GtkCssSection  *section,
                                                        GString              *string);
char *             _gtk_css_section_to_string          (const GtkCssSection  *section);

G_END_DECLS

#endif /* __GTK_CSS_SECTION_PRIVATE_H__ */
