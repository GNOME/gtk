/*
 * Copyright © 2019 Benjamin Otte
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#ifndef __GTK_FONT_LIST_H__
#define __GTK_FONT_LIST_H__


#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gdk/gdk.h>


G_BEGIN_DECLS

#define GTK_TYPE_FONT_LIST (gtk_font_list_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkFontList, gtk_font_list, GTK, FONT_LIST, GObject)

GDK_AVAILABLE_IN_ALL
GtkFontList *           gtk_font_list_new                       (void);

GDK_AVAILABLE_IN_ALL
GdkDisplay *            gtk_font_list_get_display               (GtkFontList            *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_font_list_set_display               (GtkFontList            *self,
                                                                 GdkDisplay             *display);
GDK_AVAILABLE_IN_ALL
PangoFontMap *          gtk_font_list_get_font_map              (GtkFontList            *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_font_list_set_font_map              (GtkFontList            *self,
                                                                 PangoFontMap           *font_map);
GDK_AVAILABLE_IN_ALL
gboolean                gtk_font_list_get_families_only         (GtkFontList            *self);
GDK_AVAILABLE_IN_ALL
void                    gtk_font_list_set_families_only         (GtkFontList            *self,
                                                                 gboolean                families_only);


G_END_DECLS

#endif /* __GTK_FONT_LIST_H__ */
