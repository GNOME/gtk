/* GTK - The GIMP Toolkit
 * gtkfontfilterprivate.h:
 * Copyright (C) 2024 Niels De Graef <nielsdegraef@gmail.com>
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

#pragma once

#include <gtk/gtkfilter.h>

G_BEGIN_DECLS

#define GTK_TYPE_FONT_FILTER (gtk_font_filter_get_type ())
G_DECLARE_FINAL_TYPE (GtkFontFilter, gtk_font_filter,
                      GTK, FONT_FILTER,
                      GtkFilter)

GtkFilter *        _gtk_font_filter_new                    (void);

void               _gtk_font_filter_set_pango_context      (GtkFontFilter *self,
                                                            PangoContext  *context);

gboolean           _gtk_font_filter_get_monospace          (GtkFontFilter *self);

void               _gtk_font_filter_set_monospace          (GtkFontFilter *self,
                                                            gboolean       monospace);

PangoLanguage *    _gtk_font_filter_get_language           (GtkFontFilter *self);

void               _gtk_font_filter_set_language           (GtkFontFilter *self,
                                                            PangoLanguage *language);

G_END_DECLS

