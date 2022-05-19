/*
 * Copyright © 2022 Benjamin Otte
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

#ifndef __GTK_TEXT_TILE_H__
#define __GTK_TEXT_TILE_H__

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkmediastream.h>
#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_TEXT_TILE         (gtk_text_tile_get_type ())

GDK_AVAILABLE_IN_4_8
G_DECLARE_FINAL_TYPE (GtkTextTile, gtk_text_tile, GTK, TEXT_TILE, GtkWidget)

GDK_AVAILABLE_IN_4_8
GtkWidget *     gtk_text_tile_new                       (const char             *text);

GDK_AVAILABLE_IN_4_8
const char *    gtk_text_tile_get_text                  (GtkTextTile            *self);
GDK_AVAILABLE_IN_4_8
void            gtk_text_tile_set_text                  (GtkTextTile            *self,
                                                         const char             *text);

G_END_DECLS

#endif  /* __GTK_TEXT_TILE_H__ */
